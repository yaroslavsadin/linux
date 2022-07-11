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
#define MAX_SNAPS	(8192 * 2)
#define MAX_SNAPS_MASK	(MAX_SNAPS-1)

/* Helpers to setup Event IDs:
 * 8 classes of events possible
 * 0xFFFFFF unique events for each Class
 * Right now we have only 3 classes:
 * Entry into kernel, exit from kernel and everything else is custom event
 *
 * Need for this fancy numbering scheme so that in event logger, class specific
 * things, common for all events in class, could be easily done
 */
#define EVENT_ID(x)		(x << 8)
#define EVENT_CLASS_ENTER	0x01	/* Need to start from 1, not 0 */
#define EVENT_CLASS_EXIT	0x02
#define EVENT_CLASS_CUSTOM	0x80

#define KERNEL_ENTER_EVENT(x)	(EVENT_ID(x)|EVENT_CLASS_ENTER)
#define KERNEL_EXIT_EVENT(x)	(EVENT_ID(x)|EVENT_CLASS_EXIT)
#define CUSTOM_EVENT(x)		(EVENT_ID(x)|EVENT_CLASS_CUSTOM)

/* Actual Event IDs used in kernel code */
#define SNAP_INTR_IN		KERNEL_ENTER_EVENT(0)
#define SNAP_EXCP_IN		KERNEL_ENTER_EVENT(1)
#define SNAP_TRAP_IN		KERNEL_ENTER_EVENT(2)
#define SNAP_INTR_IN2		KERNEL_ENTER_EVENT(3)
#define SNAP_TLB		KERNEL_ENTER_EVENT(4)

#define SNAP_INTR_OUT		KERNEL_EXIT_EVENT(0)
#define SNAP_EXCP_OUT		KERNEL_EXIT_EVENT(1)
#define SNAP_TRAP_OUT		KERNEL_EXIT_EVENT(2)
#define SNAP_INTR_OUT2		KERNEL_EXIT_EVENT(3)

#define SNAP_PRE_CTXSW_2_U	CUSTOM_EVENT(0)
#define SNAP_PRE_CTXSW_2_K	CUSTOM_EVENT(1)
#define SNAP_TLB_FLUSH		CUSTOM_EVENT(2)
#define SNAP_PREEMPT_SCH_IRQ	CUSTOM_EVENT(3)
#define SNAP_PREEMPT_SCH	CUSTOM_EVENT(4)
#define SNAP_SIGRETURN		CUSTOM_EVENT(5)
#define SNAP_EXIT		CUSTOM_EVENT(6)
#define SNAP_UMC		CUSTOM_EVENT(7)
#define SNAP_TLB_FAST		CUSTOM_EVENT(8)
#define SNAP_IPI_SENT		CUSTOM_EVENT(9)
#define SNAP_IPI_ELIDE		CUSTOM_EVENT(10)
#define SNAP_DMA_ALLOC		CUSTOM_EVENT(11)
#define SNAP_DMA_FREE		CUSTOM_EVENT(12)
#define SNAP_MEMSET		CUSTOM_EVENT(13)
#define SNAP_CACHE_OP_INV	CUSTOM_EVENT(14)
#define SNAP_CACHE_OP_WB	CUSTOM_EVENT(15)
#define SNAP_CACHE_OP_WB_INV	CUSTOM_EVENT(16)

#define SNAP_SENTINEL		CUSTOM_EVENT(1 << 23)

#ifndef CONFIG_ARC_DBG_EVENT_TIMELINE
#define take_snap2(event, a1, a2)
#define take_snap4(event, a1, a2, a3, a4)
#define take_snap_regs(event, regs)

#else

#ifndef __ASSEMBLY__

enum arc_event {
	IRQ_I = SNAP_INTR_IN, //+ /* IRQ Enter */
	IRQ_O = SNAP_INTR_OUT, //+ /* IRQ Exit */
	//EX_I = SNAP_EXCP_IN,
	EX_O = SNAP_EXCP_OUT, //+
	SYSCALL = SNAP_TRAP_IN,	//+ /* Trap/Syscall */
	//TRAP_O = SNAP_TRAP_OUT,
	SW_U = SNAP_PRE_CTXSW_2_U, //+ /* Task switsh to user */
	SW_K = SNAP_PRE_CTXSW_2_K, //+ /* Task switsh to kerenel */
	TLB_refill = SNAP_TLB_FAST,	    /* Fast path TLB Miss, Insert and return */
	TLB_Miss = SNAP_TLB,		//+ /* Slow path TLB Miss */
	TLB_install = SNAP_UMC,		/* Slow path TLB Insert */
	//PREEMPT_I=SNAP_PREEMPT_SCH_IRQ,
	//EXIT = SNAP_EXIT,
	TLB_Flush = SNAP_TLB_FLUSH, //+
	IPI_sent = SNAP_IPI_SENT, //+
	ipi_elide = SNAP_IPI_ELIDE,
	dma_ALLOC = SNAP_DMA_ALLOC,
	dma_free = SNAP_DMA_FREE,
	cache_wb_inv = SNAP_CACHE_OP_WB_INV,
	cache_wb = SNAP_CACHE_OP_WB,
	cache_inv = SNAP_CACHE_OP_INV,
	MEMSET = SNAP_MEMSET,
};

typedef struct {
	int cpu;
	unsigned long time;
	enum arc_event event;
	unsigned int cause;
	unsigned int  stat32;
	unsigned int pc;
	unsigned int efa;
	unsigned int extra; /* Traps: Syscall num, Intr: IRQ, Excep */
	unsigned int task;
	unsigned int sp;
} timeline_log_t;

void take_snap2(int event, unsigned int a1, unsigned int a2);
void take_snap4(int event, unsigned int a1, unsigned int a2, unsigned int a3,  unsigned int a4);
void take_snap_regs(int event, struct pt_regs *regs);

#endif /* __ASSEMBLY__ */

#endif /* CONFIG_ARC_DBG_EVENT_TIMELINE */

#endif /* __ASM_ARC_EVENT_PROFILE_H */
