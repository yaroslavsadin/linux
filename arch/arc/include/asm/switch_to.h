/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2004, 2007-2010, 2011-2012 Synopsys, Inc. (www.synopsys.com)
 */

#ifndef _ASM_ARC_SWITCH_TO_H
#define _ASM_ARC_SWITCH_TO_H

#ifndef __ASSEMBLY__

#include <linux/sched.h>
#include <asm/dsp-impl.h>
#include <asm/fpu.h>

struct task_struct *__switch_to(struct task_struct *p, struct task_struct *n);

#define switch_to(prev, next, last)	\
do {					\
	dsp_save_restore(prev, next);	\
	fpu_save_restore(prev, next);	\
	last = __switch_to(prev, next);\
	mb();				\
} while (0)

#endif

/* Hook into Schedular to be invoked prior to Context Switch
 *  -If ARC H/W profiling enabled it does some stuff
 *  -If event logging enabled it takes a event snapshot
 *
 *  Having a funtion would have been cleaner but to get the correct caller
 *  (from __builtin_return_address) it needs to be inline
 * 	Calls form Core.c prepare_task_switch()
 */

/* Things to do for event logging prior to Context switch */
#ifdef CONFIG_ARC_DBG_EVENT_TIMELINE
#include <asm/event-log.h>

#define prepare_arch_switch(next)              				\
do {									\
	if (next->mm)							\
		take_snap4(SNAP_PRE_CTXSW_2_U, 0, 0, 0, next->pid);	\
	else								\
		take_snap4(SNAP_PRE_CTXSW_2_K, 0, 0, 0, next->pid);	\
}									\
while (0)
#endif

#endif
