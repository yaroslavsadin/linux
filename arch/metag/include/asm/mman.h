#ifndef __METAG_MMAN_H__
#define __METAG_MMAN_H__

#include <asm-generic/mman.h>

#ifdef __KERNEL__
#ifndef __ASSEMBLY__
#define arch_mmap_check metag_mmap_check
int metag_mmap_check(unsigned long addr, unsigned long len,
		     unsigned long flags);
#endif
#endif

#endif /* __METAG_MMAN_H__ */
