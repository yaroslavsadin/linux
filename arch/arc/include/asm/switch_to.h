/*
 * Copyright (C) 2004, 2007-2010, 2011-2012 Synopsys, Inc. (www.synopsys.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _ASM_ARC_SWITCH_TO_H
#define _ASM_ARC_SWITCH_TO_H

#ifndef __ASSEMBLY__

#include <linux/sched.h>

#ifdef CONFIG_ARC_FPU_SAVE_RESTORE

extern void fpu_save_restore(struct task_struct *p, struct task_struct *n);
#define ARC_FPU_PREV(p, n)	fpu_save_restore(p, n)
#define ARC_FPU_NEXT(t)

#else

#define ARC_FPU_PREV(p, n)
#define ARC_FPU_NEXT(n)

#endif /* !CONFIG_ARC_FPU_SAVE_RESTORE */

/* Hook into Schedular to be invoked prior to Context Switch
 *  -If ARC H/W profiling enabled it does some stuff
 *  -If event logging enabled it takes a event snapshot
 *
 *  Having a funtion would have been cleaner but to get the correct caller
 *  (from __builtin_return_address) it needs to be inline
 */

/* Things to do for event logging prior to Context switch */
#ifdef CONFIG_ARC_DBG_EVENT_TIMELINE
#include <asm/event-log.h>

#define prepare_arch_switch(next)              				\
do {									\
	if (next->mm)							\
		take_snap(SNAP_PRE_CTXSW_2_U,				\
			 (unsigned int) __builtin_return_address(0));	\
	else								\
		take_snap(SNAP_PRE_CTXSW_2_K,				\
			 (unsigned int) __builtin_return_address(0));	\
}									\
while (0)
#endif


struct task_struct *__switch_to(struct task_struct *p, struct task_struct *n);

#define switch_to(prev, next, last)	\
do {					\
	ARC_FPU_PREV(prev, next);	\
	last = __switch_to(prev, next);\
	ARC_FPU_NEXT(next);		\
	mb();				\
} while (0)

#endif

#endif
