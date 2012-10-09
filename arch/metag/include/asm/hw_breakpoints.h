#ifndef __HW_BREAKPOINT_H__
#define __HW_BREAKPOINT_H__

enum {
	META_HWBP_CONT_REGS_BASE = CODEB0ADDR,
	META_HWBP_CONT_REGS_STRIDE = 8,
	META_HWBP_CONT_REGS_COUNT = 20,
	META_HWBP_WRITTEN = 1,
	META_HWBP_DATA_END = -1
};

#ifdef __KERNEL__

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

#endif

#endif /* __HW_BREAKPOINT_H__ */
