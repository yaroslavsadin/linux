/*
 *  Copyright (C) 2005,2006,2007,2008,2009,2012 Imagination Technologies Ltd.
 *
 * This file is subject to the terms and conditions of the GNU General
 * Public License.  See the file COPYING in the main directory of
 * this archive for more details.
 */

#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/errno.h>
#include <linux/ptrace.h>
#include <linux/user.h>
#include <linux/regset.h>
#include <linux/tracehook.h>
#include <linux/elf.h>
#include <linux/uaccess.h>
#include <trace/syscall.h>

#define CREATE_TRACE_POINTS
#include <trace/events/syscalls.h>

enum metag_regset {
	REGSET_GENERAL,
	REGSET_FP,
	REGSET_EXT,
};


/*
 * Read 32-bit data from bp context in the task struct
 */
static int ptrace_read_bp(struct task_struct *tsk, unsigned long addr,
			    unsigned long __user *data)
{
	unsigned long tmp = 0;

	if (tsk->thread.hwbp_context)
		tmp = tsk->thread.hwbp_context->bp[addr].ctx;

	return put_user(tmp, data);
}

/*
 * Write 32-bit data to bp context in the task struct
 * creating the struct if required
 * return 0 success, 1 on fail
 */
static int ptrace_write_bp(struct task_struct *tsk, unsigned long addr,
			     unsigned long data)
{

	if (!tsk->thread.hwbp_context) {
		tsk->thread.hwbp_context = create_hwbp();
		if (!tsk->thread.hwbp_context)
			return 1;
	}

	if (addr == PTRACE_CLEAR_BP) {
		tsk->thread.hwbp_context->start = META_HWBP_DATA_END;
		return 0;
	 }

	tsk->thread.hwbp_context->bp[addr].next = tsk->thread.hwbp_context->start;
	tsk->thread.hwbp_context->start = addr;
	tsk->thread.hwbp_context->bp[addr].ctx = data;
	return 0;
}

/*
 * Read an actual register from the tast_struct area.
 */
static int __peek_user(struct task_struct *task, int offset)
{
	int *regp;

	/* Find stored regblock in stack after task struct */
	regp = (int *) (task_pt_regs(task));

	/* Align to 8 bytes - Meta stacks are always 8byte aligned */
	regp = (int *) (((int) regp + 7) & ~7UL);

	return *(int *) ((int) regp + offset);
}

/*
 * Write contents of register REGNO in task TASK.
 */
static int __poke_user(struct task_struct *task, int offset, unsigned long data)
{
	int *regp;

	/* Find stored regblock in stack after task struct */
	regp = (int *) (task_pt_regs(task));

	/* Align to 8 bytes - Meta stacks are always 8byte aligned */
	regp = (int *) (((int) regp + 7) & ~7UL);

	*(int *) ((int) regp + offset) = (int) data;
	return 0;
}


/*
 * Called by kernel/ptrace.c when detaching..
 *
 * Make sure single step bits etc are not set.
 */
void ptrace_disable(struct task_struct *child)
{
	/* nothing to do.. */
}

/*
 * Read the word at offset "off" into the "struct user".  We
 * actually access the pt_regs stored on the kernel stack.
 */
static int ptrace_read_user(struct task_struct *tsk, unsigned long off,
			    unsigned long __user *data)
{
	unsigned long tmp = 0;

	if (off & 3)
		return -EIO;

	if (off < sizeof(struct pt_regs))
		tmp = __peek_user(tsk, off);

	return put_user(tmp, data);
}

static int ptrace_write_user(struct task_struct *tsk, unsigned long off,
			     unsigned long data)
{
	int ret = -EIO;

	if (off & 3)
		return -EIO;

	if (off < sizeof(struct pt_regs)) {
		if (__poke_user(tsk, off, data))
			return -EIO;
		ret = 0;
	}

	return ret;
}

/*
 * user_regset definitions.
 */

static int metag_regs_get(struct task_struct *target,
			  const struct user_regset *regset,
			  unsigned int pos, unsigned int count,
			  void *kbuf, void __user *ubuf)
{
	if (kbuf) {
		unsigned long *k = kbuf;
		while (count > 0) {
			*k++ = __peek_user(target, pos);
			count -= sizeof(*k);
			pos += sizeof(*k);
		}
	} else {
		unsigned long __user *u = ubuf;
		while (count > 0) {
			if (__put_user(__peek_user(target, pos), u++))
				return -EFAULT;
			count -= sizeof(*u);
			pos += sizeof(*u);
		}
	}
	return 0;
}

static int metag_regs_set(struct task_struct *target,
			  const struct user_regset *regset,
			  unsigned int pos, unsigned int count,
			  const void *kbuf, const void __user *ubuf)
{
	int rc = 0;

	if (kbuf) {
		const unsigned long *k = kbuf;
		while (count > 0 && !rc) {
			rc = __poke_user(target, pos, *k++);
			count -= sizeof(*k);
			pos += sizeof(*k);
		}
	} else {
		const unsigned long  __user *u = ubuf;
		while (count > 0 && !rc) {
			unsigned long word;
			rc = __get_user(word, u++);
			if (rc)
				break;
			rc = __poke_user(target, pos, word);
			count -= sizeof(*u);
			pos += sizeof(*u);
		}
	}

	return rc;
}

#ifdef CONFIG_META_DSP
/*
 * Expand the saved TBI extended context into a kernel extended context
 * structure. This makes it easier to process by userspace applications.
 */
static void expand_ext_context(int flags, struct meta_ext_context *in,
			       struct meta_ext_context *out)
{
	/*
	 * We assume that the following flags are set:
	 *
	 * TBICTX_XEXT_BIT - if this wasn't then we'd not have an extended
	 *      context to expand.
	 * TBICTX_XMCC_BIT - a bit set to signify that this task is a MECC
	 *      task, however has no real use in the kernel.
	 *
	 * However, if the extended context bit is not set, then silently
	 * leave.
	 */
	unsigned char *pos = (unsigned char *)in;

	if (!(flags & TBICTX_XEXT_BIT))
		return;

	/* Copy the TBICTX_XEXT context */
	memcpy(&(out->regs.ctx), pos, sizeof(TBIEXTCTX));
	pos += sizeof(TBIEXTCTX);

	/* Copy the TBICTX_XDX8 registers */
	if (flags & TBICTX_XDX8_BIT) {
		memcpy(&(out->regs.bb8), pos, sizeof(TBICTXEXTBB8));
		pos += sizeof(TBICTXEXTBB8);
	}

	/* Ax.4 -> Ax.7 context */
	if (flags & TBICTX_XAXX_BIT) {
		memcpy(&(out->regs.ax[0]), pos, TBICTXEXTAXX_BYTES);
		pos += TBICTXEXTAXX_BYTES;
	}

	/* Hardware loop */
	if (flags & TBICTX_XHL2_BIT) {
		memcpy(&(out->regs.hl2), pos, sizeof(TBICTXEXTHL2));
		pos += sizeof(TBICTXEXTHL2);
	}

	/* Per-thread DSP registers */
	if (flags & TBICTX_XTDP_BIT) {
		memcpy(&(out->regs.ext), pos, sizeof(TBICTXEXTTDPR));
		pos += sizeof(TBICTXEXTTDPR);
	}

	/*
	 * There are two pointer variables in the meta_dsp_context which are
	 * of no immediate interest to us at this moment in time:
	 *      void *ram[2], and
	 *      unsigned int ram_sz[2].
	 * These describe the address and size of the DSP RAMs used in the
	 * task - whether they are useful is a poignant question.
	 */
}

static int metag_ext_ctx_get(struct task_struct *target,
			  const struct user_regset *regset,
			  unsigned int pos, unsigned int count,
			  void *kbuf, void __user *ubuf)
{
	unsigned long dsp_size = sizeof(struct meta_ext_context);
	struct meta_ext_context *dsp_ctx;
	int ret = 0;

	if (target->thread.dsp_context) {
		dsp_ctx = kzalloc(dsp_size, GFP_KERNEL);
		if (!dsp_ctx) {
			ret = -ENOMEM;
			goto out;
		}
		/* Expand the DSP context if necessary */
		expand_ext_context((target->thread.user_flags >> 16),
				target->thread.dsp_context, dsp_ctx);
		if (kbuf) {
			unsigned long *k = kbuf;
			memcpy(k, dsp_ctx, dsp_size);
		} else {
			unsigned long __user *u = ubuf;
			ret = copy_to_user(u, dsp_ctx, dsp_size);
			if (ret)
				ret = -EFAULT;
		}
		kfree(dsp_ctx);
	}
out:
	return ret;
}

static int compress_ext_context(unsigned int flags, struct meta_ext_context *in,
				unsigned char *out)
{
	/* No extended flag set, no point to compress the context */
	if (!(flags & TBICTX_XEXT_BIT))
		return -EINVAL;

	/* No need to copy XEXT struct, as it's already in place */
	memcpy(out, &(in->regs.ctx), sizeof(TBIEXTCTX));
	out += sizeof(TBIEXTCTX);

	/* Copy the TBICTX_XDX8 registers */
	if (flags & TBICTX_XDX8_BIT) {
		memcpy(out, &(in->regs.bb8), sizeof(TBICTXEXTBB8));
		out += sizeof(TBICTXEXTBB8);
	}

	/* Ax.4 -> Ax.7 context */
	if (flags & TBICTX_XAXX_BIT) {
		memcpy(out, &(in->regs.ax[0]), TBICTXEXTAXX_BYTES);
		out += TBICTXEXTAXX_BYTES;
	}

	/* Hardware loop */
	if (flags & TBICTX_XHL2_BIT) {
		memcpy(out, &(in->regs.hl2), sizeof(TBICTXEXTHL2));
		out += sizeof(TBICTXEXTHL2);
	}

	/* Per-thread DSP registers */
	if (flags & TBICTX_XTDP_BIT) {
		memcpy(out, &(in->regs.ext), sizeof(TBICTXEXTTDPR));
		out += sizeof(TBICTXEXTTDPR);
	}

	return 0;
}

static int metag_ext_ctx_set(struct task_struct *target,
		  const struct user_regset *regset,
		  unsigned int pos, unsigned int count,
		  const void *kbuf, const void __user *ubuf)
{
	struct meta_ext_context *dsp_ctx = NULL;
	const unsigned int dsp_size = sizeof(struct meta_ext_context);
	unsigned int flags;
	int ret = 0;

	if (!target->thread.dsp_context) {
		ret = -EINVAL;
		goto done;
	}

	/* get a temporary context made */
	dsp_ctx = kzalloc(dsp_size, GFP_KERNEL);
	if (!dsp_ctx) {
		ret = -ENOMEM;
		goto done;
	}

	if (kbuf) {
		const unsigned long *k = kbuf;
		memcpy(dsp_ctx, k, dsp_size);
	} else {
		const unsigned long  __user *u = ubuf;
		if (!access_ok(VERIFY_READ, u, dsp_size)) {
			ret = -EFAULT;
			goto err;
		}
		copy_from_user(dsp_ctx, u, dsp_size);
	}

	/* Compress into the thread extended context */
	flags = (target->thread.user_flags >> 16);
	compress_ext_context(flags, dsp_ctx,
			(unsigned char *)target->thread.dsp_context);

err:
	kfree(dsp_ctx);

done:
	return ret;
}
#endif

static const struct user_regset metag_regsets[] = {
	[REGSET_GENERAL] = {
		.core_note_type = NT_PRSTATUS,
		.n = sizeof(struct pt_regs) / sizeof(long),
		.size = sizeof(long),
		.align = sizeof(long),
		.get = metag_regs_get,
		.set = metag_regs_set,
	},
#ifdef CONFIG_META_DSP
	[REGSET_EXT] = {
		.core_note_type = NT_PRSTATUS,
		.n = sizeof(struct meta_ext_context) / sizeof(long),
		.size = sizeof(long),
		.align = sizeof(long),
		.get = metag_ext_ctx_get,
		.set = metag_ext_ctx_set,
	},
#endif
};

static const struct user_regset_view user_metag_view = {
	.name = "metag",
	.e_machine = EM_METAG,
	.regsets = metag_regsets,
	.n = ARRAY_SIZE(metag_regsets)
};

const struct user_regset_view *task_user_regset_view(struct task_struct *task)
{
	return &user_metag_view;
}

long arch_ptrace(struct task_struct *child, long request, unsigned long addr,
		 unsigned long data)
{
	int ret;
	unsigned long __user *datap = (void __user *)data;

	switch (request) {
	/*
	 * read the word at location addr in the USER area.
	 */
	case PTRACE_PEEKUSR:
		ret = ptrace_read_user(child, addr, datap);
		break;

	/*
	 * write the word at location addr in the USER area.
	 */
	case PTRACE_POKEUSR:
		ret = ptrace_write_user(child, addr, data);
		break;

	case PTRACE_PEEK_BP:
		ret = ptrace_read_bp(child, addr, datap);
		break;

	case PTRACE_POKE_BP:
		ret = ptrace_write_bp(child, addr, data);
		break;

	case PTRACE_GETREGS:	/* Get all gp regs from the child. */
		return copy_regset_to_user(child, &user_metag_view,
					   REGSET_GENERAL,
					   0, sizeof(struct pt_regs),
					   (void __user *)datap);

	case PTRACE_SETREGS:	/* Set all gp regs in the child. */
		return copy_regset_from_user(child, &user_metag_view,
					     REGSET_GENERAL,
					     0, sizeof(struct pt_regs),
					     (const void __user *)datap);

#ifdef CONFIG_META_DSP
	case PTRACE_GETEXTREGS:	/* Get all extended context regs */
		return copy_regset_to_user(child, &user_metag_view,
				REGSET_EXT, 0, sizeof(struct meta_ext_context),
				(void __user *)datap);
		break;

	case PTRACE_SETEXTREGS:	/* Set all gp regs in the child. */
		return copy_regset_from_user(child, &user_metag_view,
				REGSET_EXT, 0, sizeof(struct meta_ext_context),
				(const void __user *)datap);
		break;
#endif

	default:
		ret = ptrace_request(child, request, addr, data);
		break;
	}

	return ret;
}

int syscall_trace_enter(struct pt_regs *regs)
{
	int ret = 0;

	if (test_thread_flag(TIF_SYSCALL_TRACE))
		ret = tracehook_report_syscall_entry(regs);

	if (unlikely(test_thread_flag(TIF_SYSCALL_TRACEPOINT)))
		trace_sys_enter(regs, regs->ctx.DX[0].U1);

	return ret ? 0 : regs->ctx.DX[0].U1;
}

void syscall_trace_leave(struct pt_regs *regs)
{
	if (unlikely(test_thread_flag(TIF_SYSCALL_TRACEPOINT)))
		trace_sys_exit(regs, regs->ctx.DX[0].U1);

	if (test_thread_flag(TIF_SYSCALL_TRACE))
		tracehook_report_syscall_exit(regs, 0);
}
