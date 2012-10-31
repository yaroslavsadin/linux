/*
 * Copyright (C) 2010 Imagination Technologies Ltd.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 */

#include <linux/oprofile.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/io.h>
#include <linux/uaccess.h>
#include "backtrace.h"

#ifdef CONFIG_FRAME_POINTER

#ifdef CONFIG_KALLSYMS
#include <linux/kallsyms.h>
#include <linux/module.h>

static unsigned long tbi_boing_addr;
static unsigned long tbi_boing_size;
#endif

static void user_backtrace_fp(unsigned long __user *fp, unsigned int depth)
{
	while (depth-- && access_ok(VERIFY_READ, fp, 8)) {
		unsigned long addr;
		unsigned long __user *fpnew;
		if (__copy_from_user_inatomic(&addr, fp + 1, sizeof(addr)))
			break;
		addr -= 4;

		oprofile_add_trace(addr);

		/* stack grows up, so frame pointers must decrease */
		if (__copy_from_user_inatomic(&fpnew, fp + 0, sizeof(fpnew)))
			break;
		if (fpnew > fp)
			break;
		fp = fpnew;
	}
}

static void kernel_backtrace_fp(unsigned long *fp, unsigned long *stack,
				unsigned int depth)
{
#ifdef CONFIG_KALLSYMS
	/* We need to know where TBIBoingVec is and it's size */
	if (!tbi_boing_addr) {
		unsigned long size;
		unsigned long offset;
		char modname[MODULE_NAME_LEN];
		char name[KSYM_NAME_LEN];
		tbi_boing_addr = kallsyms_lookup_name("___TBIBoingVec");
		if (!tbi_boing_addr)
			tbi_boing_addr = 1;
		else if (!lookup_symbol_attrs(tbi_boing_addr, &size,
						&offset, modname, name))
			tbi_boing_size = size;
	}
#endif
	/* detect when the frame pointer has been used for other purposes and
	 * doesn't point to the stack (it may point completely elsewhere which
	 * kstack_end may not detect).
	 */
	while (depth-- && fp >= stack && fp + 8 <= stack + THREAD_SIZE) {
		unsigned long addr;
		unsigned long *fpnew;

		addr = fp[1] - 4;
		if (!__kernel_text_address(addr))
			break;

		oprofile_add_trace(addr);

		/* stack grows up, so frame pointers must decrease */
		fpnew = (unsigned long *)fp[0];
		if (fpnew > fp)
			break;
		fp = fpnew;

#ifdef CONFIG_KALLSYMS
		/* If we've reached TBIBoingVec then we're at an interrupt
		 * entry point or a syscall entry point. The frame pointer
		 * points to a pt_regs which can be used to continue tracing on
		 * the other side of the boing.
		 */
		if (tbi_boing_size && addr >= tbi_boing_addr &&
				addr < tbi_boing_addr + tbi_boing_size) {
			struct pt_regs *regs = (struct pt_regs *)fp;
			/* OProfile doesn't understand backtracing into
			 * userland.
			 */
			if (!user_mode(regs) && --depth) {
				oprofile_add_trace(regs->ctx.CurrPC);
				metag_backtrace(regs, depth);
			}
			break;
		}
#endif
	}
}
#else
static void kernel_backtrace_sp(unsigned long *sp, unsigned int depth)
{
	while (!kstack_end(sp)) {
		unsigned long addr = *sp--;

		if (!__kernel_text_address(addr - 4))
			continue;
		if (!depth--)
			break;
		oprofile_add_trace(addr);
	}
}
#endif

void metag_backtrace(struct pt_regs * const regs, unsigned int depth)
{
#ifdef CONFIG_FRAME_POINTER
	unsigned long *fp = (unsigned long *)regs->ctx.AX[1].U0;
	if (user_mode(regs))
		user_backtrace_fp((unsigned long __user __force *)fp, depth);
	else
		kernel_backtrace_fp(fp, task_stack_page(current), depth);
#else
	if (!user_mode(regs)) {
		unsigned long *sp = (unsigned long *)regs->ctx.AX[0].U0;
		kernel_backtrace_sp(sp, depth);
	}
#endif
}
