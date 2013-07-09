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
int l2_ctr;


/* Event capture API */
void take_snap(int event, unsigned int arg1, unsigned int arg2)
{
	timeline_log_t *entry = &timeline_log[timeline_ctr];
	unsigned int x;

	entry->time = read_aux_reg(0x100); //ARC_REG_TIMER1_CNT);
	entry->task = current->pid;
	entry->event = event;
	entry->extra2 = read_aux_reg(0xa);	/* status32 */

	entry->cause = read_aux_reg(0x403);	/* ecr */
	entry->fault_addr = read_aux_reg(0x404);	/* efa */

	entry->extra3 = arg1;

	__asm__ __volatile__("mov %0, sp   \r\n" : "=r"(x));
	entry->sp = x;

	__asm__ __volatile__("mov %0, r25   \r\n" : "=r"(x));
	entry->extra = x;

	if (timeline_ctr == (MAX_SNAPS - 1))
		timeline_ctr = 0;
	else
		timeline_ctr++;

}
EXPORT_SYMBOL(take_snap);

void take_snap2(int event)
{
	unsigned long x, flags = 0, stat32;
	timeline_log_t *entry = &timeline_log[timeline_ctr];


	/* In case this is for Level 1 ISR, disable further Interrupts
	 * so that timeline_ctr is not clobbered
	 */
	if (event == SNAP_INTR_IN)
		local_irq_save(flags);

	entry->time = read_aux_reg(0x100); //ARC_REG_TIMER1_CNT);
	entry->task = current->pid;
	entry->event = event;
	entry->extra2 =  read_aux_reg(0xa);

	entry->sp = current_thread_info()->preempt_count;

	if (event == SNAP_INTR_IN2) {
		entry->cause = read_aux_reg(0x40B);	/* icause2 */
		entry->extra = read_aux_reg(0x0C);	/* statsu32_l2 */
//		__asm__ __volatile__("mov %0, ilink2   \r\n" : "=r"(x));
		entry->fault_addr = x;
	} else if (event == SNAP_INTR_IN) {
		entry->cause = read_aux_reg(0x40A);	/* icause1 */
		entry->extra = read_aux_reg(0x0B);	/* statsu32_l1 */
//		__asm__ __volatile__("mov %0, ilink1   \r\n" : "=r"(x));
		entry->fault_addr = x;
	} else if (event == SNAP_INTR_OUT) {
		entry->cause = 0;
		entry->extra =  task_pt_regs(current)->status32;
		entry->fault_addr = task_pt_regs(current)->ret;
	}

	if (timeline_ctr == (MAX_SNAPS - 1))
		timeline_ctr = 0;
	else
		timeline_ctr++;

	if (current_thread_info()->preempt_count == 0xFFFFFFFF)
		sort_snaps(1);

	if (event == SNAP_INTR_IN)
		local_irq_restore(flags);
}
EXPORT_SYMBOL(take_snap2);

void take_snap3(int event, unsigned int sp)
{
	unsigned long x, flags = 0;
	timeline_log_t *entry = &timeline_log[timeline_ctr];
	unsigned long stat32, ret;
	struct pt_regs *regs = (struct pt_regs *)sp;

	/* In case this is for Level 1 ISR, disable further Interrupts
	 * so that timeline_ctr is not clobbered
	 */
	if (event == SNAP_INTR_IN)
		local_irq_save(flags);

	entry->time = read_aux_reg(0x100); //ARC_REG_TIMER1_CNT);
	entry->task = current->pid;
	entry->event = event;
	entry->extra2 = 0;
	entry->fault_addr = regs->ret;

	entry->sp = read_aux_reg(0x43);	// AUX_IRQ_ACT
	entry->extra =  stat32;
	entry->extra2 = regs->sp;
	entry->extra3 = read_aux_reg(0xd);	// AUX_SP

	stat32 = regs->status32;
	ret = regs->ret;

	if (event == SNAP_INTR_IN) {
		entry->cause = 0;

//		if ((stat32 & 0x80) && ( ret > 0x80000000))
//			asm("flag 1\n");

	} else if (event == SNAP_TRAP_IN) {
		entry->cause = task_pt_regs(current)->r8;
	} else if (event == SNAP_DO_PF_ENTER) {
		entry->cause = read_aux_reg(0x403);	/* ecr */
		entry->fault_addr = read_aux_reg(0x404);	/* efa */
	} else if (event & EVENT_CLASS_EXIT) {
	//	printk("%x %x\n", stat32, ret);
//		if ((stat32 & 0x80) && (ret > 0x80000000))
//			asm("flag 1\n");
//		if (!(stat32 & 0x80 ) && (ret < 0x80000000))
//			asm("flag 1\n");
	}

	if (timeline_ctr == (MAX_SNAPS - 1))
		timeline_ctr = 0;
	else
		timeline_ctr++;

	if (current_thread_info()->preempt_count == 0xFFFFFFFF)
		sort_snaps(1);

	if (event == SNAP_INTR_IN)
		local_irq_restore(flags);
}

/* Event Sort API, so that counter Rollover is not visibel to user */
void sort_snaps(int halt_after_sort)
{
	take_snap(SNAP_SENTINEL, 0, 0);

	if (halt_after_sort)
		__asm__("flag 1");
}
EXPORT_SYMBOL(sort_snaps);
