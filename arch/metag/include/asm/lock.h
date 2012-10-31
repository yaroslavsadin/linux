#ifndef __ASM_METAG_LOCK_H
#define __ASM_METAG_LOCK_H

#define METAG_ALL_VALUES
#define METAC_ALL_VALUES
#include <asm/tbx/machine.inc>
#include <asm/tbx/metagtbx.h>

#include <linux/compiler.h>

#define __global_lock1(flags) { TBI_CRITON(flags); barrier(); }
#define __global_unlock1(flags) { barrier(); TBI_CRITOFF(flags); }

#define __global_lock2(flags) { TBI_LOCK(flags); barrier(); }
#define __global_unlock2(flags) { barrier(); TBI_UNLOCK(flags); }

#endif /* __ASM_METAG_LOCK_H */
