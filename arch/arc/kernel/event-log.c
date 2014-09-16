/*
 * event-log.c : Poorman's version of LTT for low level event capturing
 *
 * captures IRQ/Exceptions/Sys-Calls/arbitrary function call
 * XXX: Not SMP Safe
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

/*
 * Log buffer which stores the event info
 *
 *  There is race condition when the counter goes 1 more than
 *  max-value (if IRQ sneaks in in the logging routine. Since
 *  we don't want to do fancy intr-enable-disable etc,
 *  we keep 1 extra element in log buffer
 */
timeline_log_t timeline_log[MAX_SNAPS + 1];

/* counter in log bugger for next entry */
int timeline_ctr;

/* Used in the low level asm handler to free up a reg */
int tmp_save_reg, tmp_save_reg2;

int get_cpu_id(void)
{
	unsigned int id = read_aux_reg(AUX_IDENTITY);
	return (id >> 8) & 0xFF;
}

/* This is called from low level handlers, before doing EARLY RTIE
 * We just capture data and return
 * Don't call anything generic - printk is absolute NO
 */
void noinline __take_snap(int event, struct pt_regs *regs, unsigned int extra3)
{
	timeline_log_t *entry = &timeline_log[timeline_ctr];

	if (get_cpu_id() != 0)
		return;

	memset(entry, 0, sizeof(timeline_log_t));

	entry->time = read_aux_reg(0x100); //ARC_REG_TIMER1_CNT);
	entry->task = current->pid;
	entry->event = event;
	entry->sp = regs->sp;

	entry->pc = regs->ret;
	entry->stat32 =  regs->status32;
	entry->extra = extra3;

	if (event == SNAP_INTR_IN) {
	} else if (event == SNAP_TRAP_IN) {
		entry->extra = task_pt_regs(current)->r8;
	} else if (event & EVENT_CLASS_EXIT) {
	}
	else {
		entry->cause = read_aux_reg(0x403);	/* ecr */
		entry->efa = read_aux_reg(0x404);	// EFA
	}

	if (timeline_ctr == (MAX_SNAPS - 1))
		timeline_ctr = 0;
	else
		timeline_ctr++;
}

void take_snap3(int event, unsigned int sp)
{
	struct pt_regs *regs = (struct pt_regs *)sp;

	if (!sp)
		regs = task_pt_regs(current);

	__take_snap(event, regs, regs->bta);
}

void take_snap(int event, unsigned int extra)
{
	struct pt_regs *regs = (struct pt_regs *)task_pt_regs(current);

	__take_snap(event, regs, extra);
}

/* Event Sort API, so that counter Rollover is not visibel to user */
void sort_snaps(int halt_after_sort)
{
//	take_snap(SNAP_SENTINEL, 0, 0);

	if (halt_after_sort)
		__asm__("flag 1");
}
EXPORT_SYMBOL(sort_snaps);
