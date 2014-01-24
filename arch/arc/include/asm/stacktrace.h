#ifndef __ASM_STACKTRACE_H
#define __ASM_STACKTRACE_H

#include <linux/sched.h>

notrace noinline unsigned int
arc_unwind_core(struct task_struct *tsk, struct pt_regs *regs,
		int (*consumer_fn) (unsigned int, void *), void *arg,
		bool user_ok);

#endif /* __ASM_STACKTRACE_H */
