#ifndef _ASM_METAG_SETUP_H
#define _ASM_METAG_SETUP_H

#include <asm-generic/setup.h>

#ifdef  __KERNEL__
void per_cpu_trap_init(unsigned long);
extern void __init dump_machine_table(void);
#endif /* __KERNEL__ */

#endif /* _ASM_METAG_SETUP_H */
