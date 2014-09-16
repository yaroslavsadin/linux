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

.macro TAKE_SNAP_ASM reg_scratch, reg_ptr, type
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

.macro SNAP_PROLOGUE reg_scratch, reg_ptr, type

	/*
	 * Earlier we used to save only reg_scratch and clobber reg_ptr and rely
	 * on caller to understand this. Too much trouble.
	 * Now we save both
	 */
	st \reg_scratch, [tmp_save_reg]
	st \reg_ptr, [tmp_save_reg2]

	ld \reg_ptr, [timeline_ctr]

	/* HACK to detect if the circular log buffer is being overflowed */
	brne \reg_ptr, MAX_SNAPS, 1f
	flag 1
	nop
1:
#ifdef CONFIG_ARC_HAS_HW_MPY
	mpyu \reg_ptr, \reg_ptr, EVLOG_RECORD_SZ
#else
#error "even logger broken for !CONFIG_ARC_HAS_HW_MPY
#endif

	add \reg_ptr, timeline_log, \reg_ptr

	/*############ Common data ########## */

	/* TIMER1 count in timeline_log[timeline_ctr].time */
	lr \reg_scratch, [0x100]
	st \reg_scratch, [\reg_ptr, EVLOG_FIELD_TIME]

	/* current task ptr in timeline_log[timeline_ctr].task */
	ld \reg_scratch, [_current_task]
	ld \reg_scratch, [\reg_scratch, TASK_TGID]
	st \reg_scratch, [\reg_ptr, EVLOG_FIELD_TASK]

	/* Type of event (Intr/Excp/Trap etc) */
	mov \reg_scratch, \type
	st \reg_scratch, [\reg_ptr, EVLOG_FIELD_EVENT_ID]

	st sp, [\reg_ptr, EVLOG_FIELD_SP]

	lr \reg_scratch, [efa]    ; EFA
	st \reg_scratch, [\reg_ptr, EVLOG_FIELD_EXTRA2]

	lr \reg_scratch, [0xd]    ; AUX_SP
	st \reg_scratch, [\reg_ptr, EVLOG_FIELD_EXTRA3]

	lr \reg_scratch, [0x403]	; ECR
	st \reg_scratch, [\reg_ptr, EVLOG_FIELD_CAUSE]
.endm


.macro SNAP_EPILOGUE reg_scratch, reg_ptr

	/* increment timeline_ctr  with mode on max */
	ld \reg_scratch, [timeline_ctr]
	add \reg_scratch, \reg_scratch, 1
	and \reg_scratch, \reg_scratch, MAX_SNAPS_MASK
	st \reg_scratch, [timeline_ctr]

	/* Restore back orig scratch reg */
	ld \reg_scratch, [tmp_save_reg]
	ld \reg_ptr, [tmp_save_reg2]
.endm

.macro TAKE_SNAP_TLB reg_scratch, reg_ptr, type

	SNAP_PROLOGUE \reg_scratch, \reg_ptr, \type

	lr \reg_scratch, [eret]
	st \reg_scratch, [\reg_ptr, EVLOG_FIELD_PC]

	lr \reg_scratch, [erstatus]
	st \reg_scratch, [\reg_ptr, EVLOG_FIELD_EXTRA]

	SNAP_EPILOGUE \reg_scratch, \reg_ptr
.endm

#endif	/* CONFIG_ARC_DBG_EVENT_TIMELINE */

#endif	/* __ASSEMBLY__ */

#endif
