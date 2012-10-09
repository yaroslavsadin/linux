#ifndef _METAG_PTRACE_H
#define _METAG_PTRACE_H

#define METAG_ALL_VALUES
#define METAC_ALL_VALUES
#include <asm/tbx/machine.inc>
#include <asm/tbx/metagtbx.h>

#define PTRACE_GETREGS			12
#define PTRACE_SETREGS			13
/* x86 use the below numbers */
#define PTRACE_GETFPREGS		14
#define PTRACE_SETFPREGS		15
/* 16 & 17 are earmarked by generic as PTRACE_ATTACH/DETACH respectively */

#define PTRACE_CLEAR_BP			-1
#define PTRACE_PEEK_BP			100
#define PTRACE_POKE_BP			101
#define PTRACE_GETEXTREGS		102
#define PTRACE_SETEXTREGS		103



#ifndef __ASSEMBLY__

/* this struct defines the way the registers are stored on the
   stack during a system call. */

struct pt_regs {
	TBICTX ctx;
	TBICTXEXTCB0 extcb0[5];
};

#ifdef __KERNEL__

#define user_mode(regs) (((regs)->ctx.SaveMask & TBICTX_PRIV_BIT) > 0)

#define instruction_pointer(regs) ((unsigned long)(regs)->ctx.CurrPC)
#define profile_pc(regs) instruction_pointer(regs)

#define task_pt_regs(task) \
	((struct pt_regs *)(task_stack_page(task) + \
			    sizeof(struct thread_info)))

int syscall_trace_enter(struct pt_regs *regs);
void syscall_trace_leave(struct pt_regs *regs);

#endif /* __KERNEL__ */
#endif /* __ASSEMBLY__ */
#endif /* _METAG_PTRACE_H */
