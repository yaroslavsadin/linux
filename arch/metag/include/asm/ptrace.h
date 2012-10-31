#ifndef _METAG_PTRACE_H
#define _METAG_PTRACE_H

#include <uapi/asm/ptrace.h>

#ifndef __ASSEMBLY__

#define user_mode(regs) (((regs)->ctx.SaveMask & TBICTX_PRIV_BIT) > 0)

#define instruction_pointer(regs) ((unsigned long)(regs)->ctx.CurrPC)
#define profile_pc(regs) instruction_pointer(regs)

#define task_pt_regs(task) \
	((struct pt_regs *)(task_stack_page(task) + \
			    sizeof(struct thread_info)))

int syscall_trace_enter(struct pt_regs *regs);
void syscall_trace_leave(struct pt_regs *regs);

#endif /* __ASSEMBLY__ */
#endif /* _METAG_PTRACE_H */
