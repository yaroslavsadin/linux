/*
 * Copyright (C) 2004, 2007-2010, 2011-2012 Synopsys, Inc. (www.synopsys.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Amit Bhor, Sameer Dhavale: Codito Technologies 2004
 */
#ifndef __ASM_ARC_PTRACE_H
#define __ASM_ARC_PTRACE_H

#include <uapi/asm/ptrace.h>


#ifndef __ASSEMBLY__

/* THE pt_regs: Defines how regs are saved during entry into kernel */

struct pt_regs {
	/*
	 * 1 word gutter after reg-file has been saved
	 * Technically not needed, Since SP always points to a "full" location
	 * (vs. "empty"). But pt_regs is shared with tools....
	 */
	long res;

	/* Real registers */
	long bta;	/* bta_l1, bta_l2, erbta */
	long lp_start;
	long lp_end;
	long lp_count;
	long status32;	/* status32_l1, status32_l2, erstatus */
	long ret;	/* ilink1, ilink2 or eret */
	long blink;
	long fp;
	long r26;	/* gp */
	long r12;
	long r11;
	long r10;
	long r9;
	long r8;
	long r7;
	long r6;
	long r5;
	long r4;
	long r3;
	long r2;
	long r1;
	long r0;
	long sp;	/* user/kernel sp depending on where we came from  */
	long orig_r0;
	long orig_r8;	/*to distinguish bet excp, sys call, int1 or int2 */
};

/* Callee saved registers - need to be saved only when you are scheduled out */

struct callee_regs {
	long res;	/* Again this is not needed */
	long r25;
	long r24;
	long r23;
	long r22;
	long r21;
	long r20;
	long r19;
	long r18;
	long r17;
	long r16;
	long r15;
	long r14;
	long r13;
};

/* User mode registers, used for core dumps. */
struct user_regs_struct {
	struct pt_regs scratch;
	struct callee_regs callee;
	long efa;	/* break pt addr, for break points in delay slots */
	long stop_pc;	/* give dbg stop_pc directly after checking orig_r8 */
};

#define instruction_pointer(regs)	((regs)->ret)
#define profile_pc(regs)		instruction_pointer(regs)

/* return 1 if user mode or 0 if kernel mode */
#define user_mode(regs) (regs->status32 & STATUS_U_MASK)

/* return 1 if PC in delay slot */
#define delay_mode(regs) ((regs->status32 & STATUS_DE_MASK) == STATUS_DE_MASK)

/* return 1 if in syscall, 0 if Intr or Exception */
#define in_syscall(regs) (((regs->orig_r8) >= 0 && \
			   (regs->orig_r8 <= NR_syscalls)) ? 1 : 0)

#define in_brkpt_trap(regs) (((regs->orig_r8) == (NR_syscalls + 2)) ? 1 : 0)

#define user_stack_pointer(regs)\
({  unsigned int sp;		\
	if (user_mode(regs))	\
		sp = (regs)->sp;\
	else			\
		sp = -1;	\
	sp;			\
})

#endif /* !__ASSEMBLY__ */

#endif /* __ASM_PTRACE_H */
