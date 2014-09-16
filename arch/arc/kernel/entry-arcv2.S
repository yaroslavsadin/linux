/*
 * ARCv2 ISA based core Low Level Intr/Traps/Exceptions(non-TLB) Handling
 *
 * Copyright (C) 2013 Synopsys, Inc. (www.synopsys.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/linkage.h>   /* ARC_{EXTRY,EXIT} */
#include <asm/entry.h>       /* SAVE_ALL_{INT1,INT2,TRAP...} */
#include <asm/errno.h>
#include <asm/arcregs.h>
#include <asm/irqflags.h>
#include <asm/event-log-asm.h>

	.cpu HS

#define VECTOR	.word

;############################ Vector Table #################################

	.section .vector,"a",@progbits
	.align 4

# Initial 16 slots are Exception Vectors
VECTOR	res_service		; Restart Vector
VECTOR	mem_service		; Mem exception
VECTOR	instr_service		; Instrn Error
VECTOR	EV_MachineCheck		; Fatal Machine check
VECTOR	EV_TLBMissI		; Intruction TLB miss
VECTOR	EV_TLBMissD		; Data TLB miss
VECTOR	EV_TLBProtV		; Protection Violation
VECTOR	EV_PrivilegeV		; Privilege Violation
VECTOR	EV_SWI			; Software Breakpoint
VECTOR	EV_Trap			; Trap exception
VECTOR	EV_Extension		; Extn Instruction Exception
VECTOR	EV_DivZero		; Divide by Zero
VECTOR	EV_DCError		; Data Cache Error
VECTOR	EV_Misaligned		; Misaligned Data Access
VECTOR	reserved		; Reserved slots
VECTOR	reserved		; Reserved slots

# Begin Interrupt Vectors
VECTOR	handle_interrupt	; (16) Timer0
VECTOR	handle_interrupt	; unused (Timer1)
VECTOR	handle_interrupt	; unused (WDT)
VECTOR	handle_interrupt	; (19) ICI (inter core interrupt)
VECTOR	handle_interrupt
VECTOR	handle_interrupt
VECTOR	handle_interrupt
VECTOR	handle_interrupt	; (23) End of fixed IRQs

.rept CONFIG_ARC_NUMBER_OF_INTERRUPTS - 8
	VECTOR	handle_interrupt
.endr

	.section .text, "ax",@progbits

res_service:		; processor restart
	flag    0x1     ; not implemented
	nop
	nop

reserved:		; processor restart
	rtie            ; jump to processor initializations

;##################### Interrupt Handling ##############################

ARC_ENTRY handle_interrupt

	ISR_PROLOGUE
	clri		; To make status32.IE agree with CPU internal state

	TAKE_SNAP_C_FROM_ASM  SNAP_INTR_IN

	lr  r0, [ICAUSE]

	mov   blink, ret_from_exception

	b.d  arch_do_IRQ
	mov r1, sp

ARC_EXIT handle_interrupt

;################### Non TLB Exception Handling #############################

ARC_ENTRY EV_SWI
	flag 1
ARC_EXIT EV_SWI

ARC_ENTRY EV_DivZero
	flag 1
ARC_EXIT EV_DivZero

ARC_ENTRY EV_DCError
	flag 1
ARC_EXIT EV_DCError

ARC_ENTRY EV_Misaligned

	EXCEPTION_PROLOGUE

	lr  r0, [efa]	; Faulting Data address
	mov r1, sp

	FAKE_RET_FROM_EXCPN r9

	SAVE_CALLEE_SAVED_USER
	mov r2, sp              ; callee_regs

	bl  do_misaligned_access

	; TBD: optimize - do this only if a callee reg was involved
	; either a dst of emulated LD/ST or src with address-writeback
	RESTORE_CALLEE_SAVED_USER

	b   ret_from_exception
ARC_EXIT EV_Misaligned

; ---------------------------------------------
; Instruction Error Exception Handler
; ---------------------------------------------

ARC_ENTRY instr_service

	EXCEPTION_PROLOGUE

	lr  r0, [efa]
	mov r1, sp

	FAKE_RET_FROM_EXCPN r9

	bl  do_insterror_or_kprobe
	b   ret_from_exception
ARC_EXIT instr_service

; ---------------------------------------------
; Memory Error Exception Handler
; ---------------------------------------------

ARC_ENTRY mem_service

	EXCEPTION_PROLOGUE

	lr  r0, [efa]
	mov r1, sp

	FAKE_RET_FROM_EXCPN r9

	bl  do_memory_error
	b   ret_from_exception
ARC_EXIT mem_service

; ---------------------------------------------
; Machine Check Exception Handler
; ---------------------------------------------

ARC_ENTRY EV_MachineCheck

	EXCEPTION_PROLOGUE

	lr  r2, [ecr]
	lr  r0, [efa]
	mov r1, sp

	lsr  	r3, r2, 8
	bmsk 	r3, r3, 7
	brne    r3, ECR_C_MCHK_DUP_TLB, 1f

	bl      do_tlb_overlap_fault
	b       ret_from_exception

1:
	; DEAD END: can't do much, display Regs and HALT
	SAVE_CALLEE_SAVED_USER

	GET_CURR_TASK_FIELD_PTR   TASK_THREAD, r10
	st  sp, [r10, THREAD_CALLEE_REG]

	j  do_machine_check_fault

ARC_EXIT EV_MachineCheck

; ---------------------------------------------
; Protection Violation Exception Handler
; ---------------------------------------------

ARC_ENTRY EV_TLBProtV

	EXCEPTION_PROLOGUE

	lr  r0, [efa]	; Faulting Data address

	FAKE_RET_FROM_EXCPN r9

	mov r1, sp	; pt_regs

	mov blink, ret_from_exception
	b   do_page_fault

ARC_EXIT EV_TLBProtV

; ---------------------------------------------
; Privilege Violation Exception Handler
; ---------------------------------------------
ARC_ENTRY EV_PrivilegeV

	EXCEPTION_PROLOGUE

	lr  r0, [efa]
	mov r1, sp

	FAKE_RET_FROM_EXCPN r9

	bl  do_privilege_fault
	b   ret_from_exception
ARC_EXIT EV_PrivilegeV

; ---------------------------------------------
; Extension Instruction Exception Handler
; ---------------------------------------------
ARC_ENTRY EV_Extension

	EXCEPTION_PROLOGUE

	lr  r0, [efa]
	mov r1, sp

	FAKE_RET_FROM_EXCPN r9

	bl  do_extension_fault
	b   ret_from_exception
ARC_EXIT EV_Extension

;######################### System Call Tracing #########################

tracesys:
	; save EFA in case tracer wants the PC of traced task
	; using ERET won't work since next-PC has already committed
	lr  r12, [efa]
	GET_CURR_TASK_FIELD_PTR   TASK_THREAD, r11
	st  r12, [r11, THREAD_FAULT_ADDR]	; thread.fault_address

	; PRE Sys Call Ptrace hook
	mov r0, sp			; pt_regs needed
	bl  @syscall_trace_entry

	; Tracing code now returns the syscall num (orig or modif)
	mov r8, r0

	; Do the Sys Call as we normally would.
	; Validate the Sys Call number
	cmp     r8,  NR_syscalls
	mov.hi  r0, -ENOSYS
	bhi     tracesys_exit

	; Restore the sys-call args. Mere invocation of the hook abv could have
	; clobbered them (since they are in scratch regs). The tracer could also
	; have deliberately changed the syscall args: r0-r7
	ld  r0, [sp, PT_r0]
	ld  r1, [sp, PT_r1]
	ld  r2, [sp, PT_r2]
	ld  r3, [sp, PT_r3]
	ld  r4, [sp, PT_r4]
	ld  r5, [sp, PT_r5]
	ld  r6, [sp, PT_r6]
	ld  r7, [sp, PT_r7]
	ld.as   r9, [sys_call_table, r8]
	jl      [r9]        ; Entry into Sys Call Handler

tracesys_exit:
	st  r0, [sp, PT_r0]     ; sys call return value in pt_regs

	;POST Sys Call Ptrace Hook
	bl  @syscall_trace_exit
	b   ret_from_exception	; NOT ret_from_system_call at is saves r0 which
				; is already done before calling post hook above

;################### Break Point TRAP ##########################

	; ======= (5b) Trap is due to Break-Point =========

trap_with_param:

	; stop_pc info by gdb needs this info
	lr  r0, [efa]
	mov r1, sp

	; Now that we have read EFA, its safe to do "fake" rtie
	;   and get out of CPU exception mode
	FAKE_RET_FROM_EXCPN r11

	; Save callee regs in case gdb wants to have a look
	; SP will grow up by size of CALLEE Reg-File
	; NOTE: clobbers r12
	SAVE_CALLEE_SAVED_USER

	; save location of saved Callee Regs @ thread_struct->pc
	GET_CURR_TASK_FIELD_PTR   TASK_THREAD, r10
	st  sp, [r10, THREAD_CALLEE_REG]

	; Call the trap handler
	bl  do_non_swi_trap

	; unwind stack to discard Callee saved Regs
	DISCARD_CALLEE_SAVED_USER

	b   ret_from_exception

;##################### Trap Handling ##############################
;
; EV_Trap caused by TRAP_S (syscalls)
; ABI: args (r0-r7), syscall num (r8)
;------------------------------------------------------------------

ARC_ENTRY EV_Trap

	EXCEPTION_PROLOGUE

	TAKE_SNAP_C_FROM_ASM SNAP_TRAP_IN
#ifdef CONFIG_ARC_DBG_EVENT_TIMELINE
	ld  r0, [sp, PT_r0]
	ld  r1, [sp, PT_r1]
	ld  r2, [sp, PT_r2]
	ld  r3, [sp, PT_r3]
	ld  r4, [sp, PT_r4]
	ld  r5, [sp, PT_r5]
	ld  r6, [sp, PT_r6]
	ld  r7, [sp, PT_r7]
	ld  r8, [sp, PT_r7 + 4]
	lr  r9, [ecr]
#endif

	; EXCEPTION_PROLOGUE has ECR in r9 already
	bmsk.f	0, r9, 7
	bnz	trap_with_param

	; XXX: can we use KFLAG insn for HS
	; Before doing anything, return from CPU Exception Mode
	FAKE_RET_FROM_EXCPN r11

	; If syscall tracing ongoing, invoke pre-pos-hooks
	GET_CURR_THR_INFO_FLAGS   r10
	btst r10, TIF_SYSCALL_TRACE
	bnz tracesys  ; this never comes back

	;============ This is normal System Call case ==========
	; Sys-call num shd not exceed the total system calls avail
	cmp     r8,  NR_syscalls
	mov.hi  r0, -ENOSYS
	bhi     ret_from_system_call

	; Offset into the syscall_table and call handler
	ld.as   r9,[sys_call_table, r8]
	jl      [r9]        ; Entry into Sys Call Handler

	; fall through to ret_from_system_call
ARC_EXIT EV_Trap

ARC_ENTRY ret_from_system_call

	st  r0, [sp, PT_r0]     ; sys call return value in pt_regs

	; fall through yet again to ret_from_exception

;############# Return from Intr/Excp/Trap (Linux Specifics) ##############
;
; If ret to user mode do we need to handle signals, schedule() et al.

ARC_ENTRY ret_from_exception

	; Pre-{IRQ,Trap,Exception} K/U mode from pt_regs->status32
	ld  r8, [sp, PT_status32]   ; returning to User/Kernel Mode

	bbit0  r8, STATUS_U_BIT, resume_kernel_mode

	; Before returning to User mode check-for-and-complete any pending work
	; such as rescheduling/signal-delivery etc.
resume_user_mode_begin:

	; Disable IRQs to ensures that chk for pending work itself is atomic
	; (and we don't end up missing a NEED_RESCHED/SIGPENDING due to an
	; interim IRQ).
	IRQ_DISABLE	r10

	; Fast Path return to user mode if no pending work
	GET_CURR_THR_INFO_FLAGS   r9
	and.f  0,  r9, _TIF_WORK_MASK
	bz     restore_regs

	; --- (Slow Path #1) task preemption ---
	bbit0  r9, TIF_NEED_RESCHED, .Lchk_pend_signals
	mov    blink, resume_user_mode_begin  ; tail-call to U mode ret chks
	b      @schedule 	; BTST+Bnz causes relo error in link

.Lchk_pend_signals:
	IRQ_ENABLE	r10

	; --- (Slow Path #2) pending signal  ---
	mov r0, sp	; pt_regs for arg to do_signal()/do_notify_resume()

	GET_CURR_THR_INFO_FLAGS   r9
	bbit0  r9, TIF_SIGPENDING, .Lchk_notify_resume

	; Normal Trap/IRQ entry only saves Scratch (caller-saved) regs
	; in pt_reg since the "C" ABI (kernel code) will automatically
	; save/restore callee-saved regs.
	;
	; However, here we need to explicitly save callee regs because
	; (i)  If this signal causes coredump - full regfile needed
	; (ii) If signal is SIGTRAP/SIGSTOP, task is being traced thus
	;      tracer might call PEEKUSR(CALLEE reg)
	;
	; NOTE: SP will grow up by size of CALLEE Reg-File
	SAVE_CALLEE_SAVED_USER		; clobbers r12

	; save location of saved Callee Regs @ thread_struct->callee
	GET_CURR_TASK_FIELD_PTR   TASK_THREAD, r10
	st  sp, [r10, THREAD_CALLEE_REG]

	bl  @do_signal

	; Ideally we want to discard the Callee reg above, however if this was
	; a tracing signal, tracer could have done a POKEUSR(CALLEE reg)
	RESTORE_CALLEE_SAVED_USER

	b      resume_user_mode_begin	; loop back to start of U mode ret

	; --- (Slow Path #3) notify_resume ---
.Lchk_notify_resume:
	btst   r9, TIF_NOTIFY_RESUME
	blnz   @do_notify_resume
	b      resume_user_mode_begin	; unconditionally back to U mode ret chks
					; for single exit point from this block

resume_kernel_mode:

	; Disable Interrupts from this point on
	; CONFIG_PREEMPT: This is a must for preempt_schedule_irq()
	; !CONFIG_PREEMPT: To ensure restore_regs is intr safe
	IRQ_DISABLE	r9

#ifdef CONFIG_PREEMPT

	; Can't preempt if preemption disabled
	GET_CURR_THR_INFO_FROM_SP   r10
	ld  r8, [r10, THREAD_INFO_PREEMPT_COUNT]
	brne  r8, 0, restore_regs

	; check if this task's NEED_RESCHED flag set
	ld  r9, [r10, THREAD_INFO_FLAGS]
	bbit0  r9, TIF_NEED_RESCHED, restore_regs

	; Invoke PREEMPTION
	bl      preempt_schedule_irq

	; preempt_schedule_irq() always returns with IRQ disabled
#endif

	; fall through

;############# Return from Intr/Excp/Trap (ARC Specifics) ##############
;
; Restore the saved sys context (common exit-path for EXCPN/IRQ/Trap)
; IRQ shd definitely not happen between now and rtie
; All 2 entry points to here already disable interrupts

restore_regs:

	ld	r0, [sp, PT_status32]	; U/K mode at time of entry
	lr	r10, [AUX_IRQ_ACT]

	bmsk	r11, r10, 15	; AUX_IRQ_ACT.ACTIVE
	breq	r11, 0, 1f	; if 0, intr not active, so ret from Exception

	; Return from Intr uses AUX_IRQ_ACT.U to swap SP and AUX_SP
	; However if intr taken in K mode, but returns to U mode, then
	; AUX_IRQ_ACT.U needs to be forced for correct AUX_SP swap
	btst	r0, STATUS_U_BIT
	bset.nz	r11, r11, STATUS_IE_BIT
	sr	r11, [AUX_IRQ_ACT]

	TAKE_SNAP_C_FROM_ASM SNAP_INTR_OUT

debug_marker_l1:
	ISR_EPILOGUE
	rtie

1:
	TAKE_SNAP_C_FROM_ASM SNAP_EXCP_OUT

debug_marker_syscall:
	EXCEPTION_EPILOGUE
	rtie

ARC_EXIT ret_from_exception

ARC_ENTRY ret_from_fork
	; when the forked child comes here from the __switch_to function
	; r0 has the last task pointer.
	; put last task in scheduler queue
	bl   @schedule_tail

	; If kernel thread, jump to it's entry-point
	ld   r9, [sp, PT_status32]
	brne r9, 0, 1f

	jl.d [r14]
	mov  r0, r13		; arg to payload

1:
	; special case of kernel_thread entry point returning back due to
	; kernel_execve() - pretend return from syscall to ret to userland
	b    ret_from_exception
ARC_EXIT ret_from_fork

;################### Special Sys Call Wrappers ##########################

ARC_ENTRY sys_clone_wrapper
	SAVE_CALLEE_SAVED_USER
	bl  @sys_clone
	DISCARD_CALLEE_SAVED_USER

	GET_CURR_THR_INFO_FLAGS   r10
	btst r10, TIF_SYSCALL_TRACE
	bnz  tracesys_exit

	b ret_from_system_call
ARC_EXIT sys_clone_wrapper

#ifdef CONFIG_ARC_DW2_UNWIND
; Workaround for bug 94179 (STAR ):
; Despite -fasynchronous-unwind-tables, linker is not making dwarf2 unwinder
; section (.debug_frame) as loadable. So we force it here.
; This also fixes STAR 9000487933 where the prev-workaround (objcopy --setflag)
; would not work after a clean build due to kernel build system dependencies.
.section .debug_frame, "wa",@progbits
#endif
