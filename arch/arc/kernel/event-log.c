/*
 * event-log.c : Poorman's version of LTT for low level event capturing
 *
 * captures IRQ/Exceptions/Sys-Calls/arbitrary function call
 *
 * Copyright (C) 2004, 2007-2010, 2011-2012 Synopsys, Inc. (www.synopsys.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  vineetg Jan 2009
 *      -Converted strcpy to strncpy
 *
 *  vineetg: Feb 2008: Event capturing Framework
 *      -Captures the event-id and related info in circular log buffer
 *
 *      -USAGE:
 *          Events are defined in API file, include/asm-arc/event-log.h
 *          To log the event, "C" code calls API
 *              take_snap(event-id, event-specific-info)
 *          To log the event, ASM caller calls a "asm" macro
 *              TAKE_SNAP_ASM reg-x, reg-y, event-id
 *          To stop the capture and sort the log buffer,
 *              sort_snaps(halt-after-sort)
 *
 *      -The reason for 2 APIs is that in low level handlers
 *          which we are interested in capturing, often don't have
 *          stack switched, thus a "C" API wont work. Also there
 *          is a very strict requirement of which registers are usable
 *          hence the 2 regs
 *
 *      -Done primarily to chase the Random Segmentation Faults
 *          when we switched from gcc 3.4 to 4.2
 */

#include <linux/sort.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <asm/current.h>
#include <asm/event-log.h>
#include <linux/module.h>
#include <linux/reboot.h>

#ifdef CONFIG_ARC_CURR_IN_REG
/*
 * current on ARC is a register variable "r25" setup on entry to kernel and
 * restored back to user value on return.
 * However if the event snap shotting return is called very late from
 * ISR/Exception return code, r25 might already have been restored to user
 * value, hence would no longer point to current task. This can cause weird
 * de-referencing crashes. Safest option is to undef it and instead define
 * it in terms of current_thread_info() which is derived from SP
 */
#undef current
#define current (current_thread_info()->task)
#endif

/* des, src are NOT addr */
#define STR(dest, val)	arc_write_uncached_32(&(dest), val)
#define LDR(src)	arc_read_uncached_32(&(src))

timeline_log_t timeline_log[MAX_SNAPS] __attribute__((aligned(128)));
int timeline_ctr __attribute__((aligned(128)));

#ifdef CONFIG_SMP
DEFINE_RAW_SPINLOCK(timeline_lock);
#endif

static void inline __take_snap_lowest(int event, unsigned int sp, unsigned int pc,
	unsigned int a1, unsigned int a2, unsigned int a3,  unsigned int a4)
{
	int c;
	timeline_log_t *entry;
	unsigned long flags;
	int cpu = IS_ENABLED(CONFIG_SMP) ? ((read_aux_reg(AUX_IDENTITY) >> 8 ) & 0xFF) : 0;

	local_irq_save(flags);

#ifdef CONFIG_SMP
	arch_spin_lock(&timeline_lock.raw_lock);
#endif

	c = LDR(timeline_ctr);
	entry = &timeline_log[c];

	STR(entry->cpu, cpu);

#ifdef CONFIG_SMP
#ifdef CONFIG_ISA_ARCV2
	write_aux_reg(0x600, 0x42);		// ARC_REG_MCIP_CMD
	STR(entry->time, read_aux_reg(0x602));	// ARC_REG_MCIP_READBACK
#else
	STR(entry->time, jiffies);
#endif
#else
	STR(entry->time, read_aux_reg(0x100)); //ARC_REG_TIMER1_CNT);
#endif

	STR(entry->task, current->pid);
	STR(entry->event, event);
	STR(entry->sp, sp);
	STR(entry->pc, pc);

	STR(entry->cause, a1);
	STR(entry->stat32, a2);
	STR(entry->efa, a3);
	STR(entry->extra, a4);

	/* next empty entry (not last valid entry) */
	c = (c + 1) & (MAX_SNAPS - 1);

	STR(timeline_ctr, c);

#ifdef CONFIG_SMP
	arch_spin_unlock(&timeline_lock.raw_lock);
#endif
	local_irq_restore(flags);
}

void take_snap_regs(int event, struct pt_regs *regs)
{
	__take_snap_lowest(event, regs->sp, regs->ret, 0, 0, 0, 0);
}

void take_snap_irq(int event, struct pt_regs *regs)
{
	__take_snap_lowest(event, regs->sp, regs->ret, read_aux_reg(0x40a), regs->status32, 0, 0);
}

void take_snap4(int event, unsigned int a1, unsigned int a2, unsigned int a3,  unsigned int a4)
{
	unsigned long caller_pc = (unsigned long)__builtin_return_address(0);
	unsigned long sp;

	asm volatile("mov %0, sp\n":"=r" (sp));

	__take_snap_lowest(event, sp, caller_pc, a1, a2, a3, a4);
}

void take_snap2(int event, unsigned int a1, unsigned int a2)
{
	take_snap4(event, a1, a2, 0, 0);
}
