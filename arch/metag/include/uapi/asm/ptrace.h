#ifndef _UAPI_METAG_PTRACE_H
#define _UAPI_METAG_PTRACE_H

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

#endif /* __ASSEMBLY__ */
#endif /* _UAPI_METAG_PTRACE_H */
