/*
 * Copyright (C) 2004, 2007-2010, 2011-2012 Synopsys, Inc. (www.synopsys.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  vineetg: Dec 2009
 *      Reworked the numbering scheme into Event Classes for making it easier to
 *      do class specific things in the snapshot routines
 *
 *  vineetg: Feb 2008
 *      System Event Logging APIs
 */

#ifndef __ASM_ARC_EVENT_LOG_H
#define __ASM_ARC_EVENT_LOG_H

/*######################################################################
 *
 *    Event Logging API
 *
 *#####################################################################*/

/* Size of the log buffer */
#define MAX_SNAPS	2048
#define MAX_SNAPS_MASK	(MAX_SNAPS-1)

/* Helpers to setup Event IDs:
 * 8 classes of events possible
 * 23 unique events for each Class
 * Right now we have only 3 classes:
 * Entry into kernel, exit from kernel and everything else is custom event
 *
 * Need for this fancy numbering scheme so that in event logger, class specific
 * things, common for all events in class, could be easily done
 */
#define EVENT_ID(x)             (0x100 << x)
#define EVENT_CLASS_ENTER       0x01	/* Need to start from 1, not 0 */
#define EVENT_CLASS_EXIT        0x02
#define EVENT_CLASS_CUSTOM      0x80

#define KERNEL_ENTER_EVENT(x)   (EVENT_ID(x)|EVENT_CLASS_ENTER)
#define KERNEL_EXIT_EVENT(x)    (EVENT_ID(x)|EVENT_CLASS_EXIT)
#define CUSTOM_EVENT(x)         (EVENT_ID(x)|EVENT_CLASS_CUSTOM)

/* Actual Event IDs used in kernel code */
#define SNAP_INTR_IN            KERNEL_ENTER_EVENT(0)	//  101
#define SNAP_EXCP_IN            KERNEL_ENTER_EVENT(1)	//  201
#define SNAP_TRAP_IN            KERNEL_ENTER_EVENT(2)	//  401
#define SNAP_INTR_IN2           KERNEL_ENTER_EVENT(3)	//  801
#define SNAP_ITLB		KERNEL_ENTER_EVENT(4)	// 1001
#define SNAP_DTLB_LD		KERNEL_ENTER_EVENT(5)	// 2001
#define SNAP_DTLB_ST		KERNEL_ENTER_EVENT(6)	// 4001

#define SNAP_INTR_OUT           KERNEL_EXIT_EVENT(0)	//  102
#define SNAP_EXCP_OUT           KERNEL_EXIT_EVENT(1)	//  202
#define SNAP_TRAP_OUT           KERNEL_EXIT_EVENT(2)	//  402
#define SNAP_INTR_OUT2          KERNEL_EXIT_EVENT(3)	//  802
#define SNAP_EXCP_OUT_FAST      KERNEL_EXIT_EVENT(4)	// 1002

#define SNAP_PRE_CTXSW_2_U      CUSTOM_EVENT(0)		//  180
#define SNAP_PRE_CTXSW_2_K      CUSTOM_EVENT(1)		//  280
#define SNAP_DO_PF_ENTER        CUSTOM_EVENT(2)		//  480
#define SNAP_DO_PF_EXIT         CUSTOM_EVENT(3)		//  880
#define SNAP_TLB_FLUSH_ALL      CUSTOM_EVENT(4)		// 1080
#define SNAP_PREEMPT_SCH_IRQ    CUSTOM_EVENT(5)		// 2080
#define SNAP_PREEMPT_SCH        CUSTOM_EVENT(6)		// 4080
#define SNAP_SIGRETURN          CUSTOM_EVENT(7)		// 8080
#define SNAP_BEFORE_SIG         CUSTOM_EVENT(8)		//10080


#define SNAP_SENTINEL           CUSTOM_EVENT(22)

#ifndef CONFIG_ARC_DBG_EVENT_TIMELINE

#define take_snap3(event, sp)
#define sort_snaps(halt_after_sort)

#else

#ifndef __ASSEMBLY__

enum arc_event {
	IRQ_I = SNAP_INTR_IN,
	IRQ_O = SNAP_INTR_OUT,
	EX_I = SNAP_EXCP_IN,
	EX_O = SNAP_EXCP_OUT,
	TRAP = SNAP_TRAP_IN,
	TRAP_O = SNAP_TRAP_OUT,
	SW_U = SNAP_PRE_CTXSW_2_U,
	SW_K = SNAP_PRE_CTXSW_2_K,
	PF = SNAP_DO_PF_ENTER,
	ITLB = SNAP_ITLB,
	DITLB_ld = SNAP_DTLB_LD,
	DTLB_st = SNAP_DTLB_ST,

	extra1		= 0x20080,
	extra2		= 0x40080,
};

typedef struct {

	/* 0 */ enum arc_event event;
	/* 4 */ unsigned int cause;
	/* 8 */ unsigned int extra; /* Traps: Syscall num, Intr: IRQ, Excep */
	/* 12 */ unsigned int pc;
	/* 16 */ unsigned int extra2;
	/* 20 */ unsigned int extra3;
	/* 24 */ unsigned int task;
	/* 28 */ unsigned long time;
	/* 32 */ unsigned int sp;

} timeline_log_t;

void take_snap3(int event, unsigned int sp);

#endif /* __ASSEMBLY__ */

#endif /* CONFIG_ARC_DBG_EVENT_TIMELINE */

#endif /* __ASM_ARC_EVENT_PROFILE_H */
