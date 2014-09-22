
#ifndef __ASM_ARC_ENTRY_ARCV2_H
#define __ASM_ARC_ENTRY_ARCV2_H

.macro ISR_PROLOGUE

	; SP auto-switched to kernel mode stack by hardware
	; Lot of registers auto-saved too (look at EXCEPTION_PROLOGUE below)
	PUSH	r12
	PUSH	gp
	PUSH	fp

	PUSHAX	AUX_USER_SP	; user mode SP

#ifdef CONFIG_ARC_CURR_IN_REG
	PUSH	r25
	GET_CURR_TASK_ON_CPU	r25
#else
	sub	sp, sp, 4
#endif

	sub	sp, sp, 12	; BTA/ECR/orig_r0 placeholder per pt_regs
.endm

.macro EXCEPTION_PROLOGUE

	; SP auto-switched to kernel mode stack by hardware
	PUSH	r9		; freeup a register: slot of erstatus

	PUSHAX	eret
	sub	sp, sp, 12	; skip JLI, LDI, EI
	PUSH	lp_count
	PUSHAX	lp_start
	PUSHAX	lp_end
	PUSH	blink

	PUSH	r11
	PUSH	r10

	ld.as	r9,  [sp, 10]	; load stashed r9 (status32 stack slot)
	lr	r10, [erstatus]
	st.as	r10, [sp, 10]	; save status32 at it's right stack slot

	PUSH	r9
	PUSH	r8
	PUSH	r7
	PUSH	r6
	PUSH	r5
	PUSH	r4
	PUSH	r3
	PUSH	r2
	PUSH	r1
	PUSH	r0

	; -- for interrupts, regs above are auto-saved by h/w in that order --
	PUSH	r12
	PUSH	gp
	PUSH	fp
	PUSHAX	AUX_USER_SP

#ifdef CONFIG_ARC_CURR_IN_REG
	PUSH	r25
	GET_CURR_TASK_ON_CPU r25
#else
	sub	sp, sp, 4
#endif

	PUSHAX	erbta
	PUSHAX	ecr		; r9 contains ECR, expected by EV_Trap

	PUSH	r0		; orig_r0
.endm

.macro EXCEPTION_EPILOGUE

	add	sp, sp, 8	;orig_r0/ECR don't need restoring
	POPAX	erbta

#ifdef CONFIG_ARC_CURR_IN_REG
	POP	r25
#else
	add	sp, sp, 4
#endif

	POPAX	AUX_USER_SP

	POP	fp
	POP	gp

	POP	r12

	POP	r0
	POP	r1
	POP	r2
	POP	r3
	POP	r4
	POP	r5
	POP	r6
	POP	r7
	POP	r8
	POP	r9
	POP	r10
	POP	r11

	POP	blink
	POPAX	lp_end
	POPAX	lp_start

	POP	r9
	mov	lp_count, r9

	add	sp, sp, 12	; skip JLI, LDI, EI
	POPAX	eret
	POPAX	erstatus

	ld.as	r9, [sp, -12]	; reload r9 which got clobbered

	rtie
.endm

#endif
