/*
 *  Low level Event Capture API callable from Assembly Code
 *  vineetg: Feb 2008
 *
 *  TBD: SMP Safe
 *
 * Copyright (C) 2004, 2007-2010, 2011-2012 Synopsys, Inc. (www.synopsys.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __ASM_ARC_EVENT_LOG_ASM_H
#define __ASM_ARC_EVENT_LOG_ASM_H

#include <asm/event-log.h>

#ifdef __ASSEMBLY__

#ifndef CONFIG_ARC_DBG_EVENT_TIMELINE

.macro TAKE_SNAP_TLB reg0, reg1, miss_type
.endm

.macro TAKE_SNAP_SYSCALL reg0, reg1
.endm

.macro TAKE_SNAP_C_FROM_ASM type
.endm

#else

#include <asm/asm-offsets.h>

/*
 * Macro to invoke the "C" event logger routine from assmebly code
 */
.macro TAKE_SNAP_C_FROM_ASM type
	mov r0, \type
	mov r1, sp
	bl take_snap3
.endm

/*
 * Log buffer and iterator are uncached so when we retrieve them form mdb,
 * nothing is left in cache
 */
#define LD_DI	ld.di
#define ST_DI	st.di

.macro SNAP_PROLOGUE reg0, reg1, event_id

	PUSH		\reg0
	PUSH		\reg1

	/* ony support one core logging at a time */
	GET_CPU_ID	\reg0
	tst		\reg0, \reg0	; Z set if Zero
	bnz		899f

	LD_DI \reg1, [timeline_ctr]

	/* HACK to detect if the circular log buffer is being overflowed */
	brne \reg1, MAX_SNAPS, 1f
	flag 1
	nop
1:
#ifdef CONFIG_ARC_HAS_HW_MPY
	mpyu \reg1, \reg1, EVLOG_RECORD_SZ
#else
#error "even logger broken for !CONFIG_ARC_HAS_HW_MPY
#endif

	add \reg1, timeline_log, \reg1

	/*############ Common data ########## */

	/* TIMER1 count in timeline_log[timeline_ctr].time */
	lr \reg0, [0x100]
	ST_DI \reg0, [\reg1, EVLOG_FIELD_TIME]

	/* current task ptr in timeline_log[timeline_ctr].task */
	ld \reg0, [_current_task]
	ld \reg0, [\reg0, TASK_TGID]
	ST_DI \reg0, [\reg1, EVLOG_FIELD_TASK]

	/* Type of event (Intr/Excp/Trap etc) */
	mov \reg0, \event_id
	ST_DI \reg0, [\reg1, EVLOG_FIELD_EVENT_ID]

	ST_DI sp, [\reg1, EVLOG_FIELD_SP]

	lr \reg0, [eret]
	ST_DI \reg0, [\reg1, EVLOG_FIELD_PC]

	lr \reg0, [efa]    ; EFA
	ST_DI \reg0, [\reg1, EVLOG_FIELD_EFA]

	lr \reg0, [0x403]	; ECR
	ST_DI \reg0, [\reg1, EVLOG_FIELD_CAUSE]

	lr \reg0, [erstatus]
	ST_DI \reg0, [\reg1, EVLOG_FIELD_STATUS]

	lr \reg0, [0xd]    ; AUX_SP
	ST_DI \reg0, [\reg1, EVLOG_FIELD_EXTRA]

.endm


.macro SNAP_EPILOGUE reg0, reg1

	/* increment timeline_ctr  with mode on max */
	LD_DI \reg0, [timeline_ctr]
	add \reg0, \reg0, 1
	and \reg0, \reg0, MAX_SNAPS_MASK
	ST_DI \reg0, [timeline_ctr]

899:
	/* Restore back orig scratch reg */
	POP	\reg1
	POP	\reg0
.endm

.macro TAKE_SNAP_TLB reg0, reg1, event_id
	SNAP_PROLOGUE \reg0, \reg1, \event_id
	SNAP_EPILOGUE \reg0, \reg1
.endm

.macro TAKE_SNAP_SYSCALL reg0, reg1
	SNAP_PROLOGUE \reg0, \reg1, SNAP_TRAP_IN

	ST_DI r8, [\reg1, EVLOG_FIELD_CAUSE]	; syscall num

	SNAP_EPILOGUE \reg0, \reg1
.endm

#endif	/* CONFIG_ARC_DBG_EVENT_TIMELINE */

#endif	/* __ASSEMBLY__ */

#endif
