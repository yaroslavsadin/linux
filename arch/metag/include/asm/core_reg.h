#ifndef __ASM_METAG_CORE_REG_H_
#define __ASM_METAG_CORE_REG_H_

extern void core_reg_write(int unit, int reg, int thread, unsigned int val);
extern unsigned int core_reg_read(int unit, int reg, int thread);

#endif
