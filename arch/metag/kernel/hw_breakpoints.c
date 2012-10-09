/*
 * Copyright (C) 2005,2006,2007,2008,2010,2012 Imagination Technologies Ltd.
 */

#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <asm/hw_breakpoints.h>

/*
 * Allocate and initialise a breakpoint control block
 */
struct meta_hw_breakpoint *create_hwbp(void)
{
	struct meta_hw_breakpoint *temp;

	temp = kzalloc(sizeof(struct meta_hw_breakpoint), GFP_KERNEL);
	if (temp)
		temp->start = META_HWBP_DATA_END;

	return temp;
}

/*
 * Sets up the hardware breakpoint controller
 * only write to the registers that we have data for
 * having first saved what was there before
 */
void setup_hwbp_controller(struct meta_hw_breakpoint *hwbp)
{
	unsigned long address, temp;
	unsigned int reg;

	reg = hwbp->start;
	while (reg != META_HWBP_DATA_END) {
		address = (META_HWBP_CONT_REGS_BASE +
			   (META_HWBP_CONT_REGS_STRIDE * reg));
		temp = readl(address);
		writel(hwbp->bp[reg].ctx, address);
		hwbp->bp[reg].ctx = temp;
		hwbp->written |= META_HWBP_WRITTEN;
		reg = hwbp->bp[reg].next;
	}
}

/*
 * Restores the hw break controller to it's original condition
 * Only restore if we've previously written to it
 * and only restore the registers we wrote to
 * Save the current state of the controller before restoring the original values
 */
void restore_hwbp_controller(struct meta_hw_breakpoint *hwbp)
{
	unsigned long address, temp;
	unsigned int reg;

	reg = hwbp->start;
	while (reg != META_HWBP_DATA_END) {
		address = (META_HWBP_CONT_REGS_BASE +
			   (META_HWBP_CONT_REGS_STRIDE * reg));
		temp = readl(address);
		writel(hwbp->bp[reg].ctx, address);
		hwbp->bp[reg].ctx = temp;
		reg = hwbp->bp[reg].next;
	}
	hwbp->written &= ~META_HWBP_WRITTEN;
}

/*
 * Clear the hardware breakpoint controller by
 * writing the original values back to it
 * We only restore the registers we've disturbed
 * And free the breakpoint data
 */
static void clear_hwbp_controller(struct meta_hw_breakpoint *hwbp)
{
	unsigned long address;
	unsigned int reg;

	if (hwbp) {
		reg = hwbp->start;
		while (reg != META_HWBP_DATA_END) {
			address = (META_HWBP_CONT_REGS_BASE +
				   (META_HWBP_CONT_REGS_STRIDE * reg));
			writel(hwbp->bp[reg].ctx, address);
			reg = hwbp->bp[reg].next;
		}
		kfree(hwbp);
	}
}

/*
 * Hook into the exit function to clear the breakpoint controller
 */
void flush_ptrace_hw_breakpoint(struct task_struct *tsk)
{
	struct meta_hw_breakpoint *hwbp = tsk->thread.hwbp_context;

	tsk->thread.hwbp_context = NULL;
	clear_hwbp_controller(hwbp);
}
