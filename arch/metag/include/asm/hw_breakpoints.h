#ifndef __HW_BREAKPOINT_H__
#define __HW_BREAKPOINT_H__

#include <uapi/asm/hw_breakpoints.h>


struct meta_hw_breakpoint_item {
	unsigned long ctx;
	unsigned int next;
};

struct meta_hw_breakpoint {
	struct meta_hw_breakpoint_item bp[META_HWBP_CONT_REGS_COUNT];
	unsigned int written, start;
};

struct meta_hw_breakpoint *create_hwbp(void);
void setup_hwbp_controller(struct meta_hw_breakpoint *hwbp);
void restore_hwbp_controller(struct meta_hw_breakpoint *hwbp);

#endif /* __HW_BREAKPOINT_H__ */
