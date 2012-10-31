#ifndef _ASM_METAG_SIGCONTEXT_H
#define _ASM_METAG_SIGCONTEXT_H

#define METAG_ALL_VALUES
#define METAC_ALL_VALUES
#include <asm/tbx/metagtbx.h>
/* In a sigcontext structure we need to store the active state of the
 * user process so that it does not get trashed when we call the signal
 * handler. That not really the same as a user context that we are
 * going to store on syscall etc.
 */
struct sigcontext {
	TBICTX ctx;

	/* Space to save catch buffers. To tell if the catch buffers have been
	 * saved then check the CBF bit in the ctx flags.  See TBX headers for
	 * more info.
	 */
	TBICTXEXTCB0 extcb0[5];
	unsigned long oldmask;
};

#endif
