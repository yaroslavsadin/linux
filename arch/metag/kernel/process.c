/*
 * Copyright (C) 2005,2006,2007,2008,2009,2010,2011 Imagination Technologies
 *
 * This file contains the architecture-dependent parts of process handling.
 *
 */

#include <linux/errno.h>
#include <linux/export.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/unistd.h>
#include <linux/ptrace.h>
#include <linux/user.h>
#include <linux/reboot.h>
#include <linux/elfcore.h>
#include <linux/fs.h>
#include <linux/tick.h>
#include <linux/slab.h>
#include <linux/mman.h>
#include <linux/pm.h>
#include <linux/uaccess.h>
#include <asm/hw_breakpoints.h>
#include <asm/user_gateway.h>
#include <asm/tcm.h>
#include <asm/traps.h>
#include <asm/switch_to.h>

void cpu_idle(void)
{
	set_thread_flag(TIF_POLLING_NRFLAG);

	while (1) {
		tick_nohz_idle_enter();
		rcu_idle_enter();

		while (!need_resched()) {
#ifdef CONFIG_HOTPLUG_CPU
			if (cpu_is_offline(smp_processor_id()))
				cpu_die();
#endif
			__asm__ volatile ("SWITCH #0x400000\n"
					  : : : "memory");
		}

		rcu_idle_exit();
		tick_nohz_idle_exit();
		schedule_preempt_disabled();
	 }
}

void (*pm_power_off)(void);
EXPORT_SYMBOL(pm_power_off);

void (*soc_restart)(char *cmd);
void (*soc_halt)(void);

void machine_restart(char *cmd)
{
	if (soc_restart)
		soc_restart(cmd);
	hard_processor_halt(HALT_OK);
}

void machine_halt(void)
{
	if (soc_halt)
		soc_halt();
	smp_send_stop();
	hard_processor_halt(HALT_OK);
}

void machine_power_off(void)
{
	if (pm_power_off)
		pm_power_off();
	smp_send_stop();
	hard_processor_halt(HALT_OK);
}

#define FLAG_Z 0x8
#define FLAG_N 0x4
#define FLAG_O 0x2
#define FLAG_C 0x1

void show_regs(struct pt_regs *regs)
{
	int i;
	const char *AX0_names[] = {"A0StP", "A0FrP"};
	const char *AX1_names[] = {"A1GbP", "A1LbP"};

	const char *DX0_names[] = {
		"D0Re0",
		"D0Ar6",
		"D0Ar4",
		"D0Ar2",
		"D0FrT",
		"D0.5 ",
		"D0.6 ",
		"D0.7 "
	};

	const char *DX1_names[] = {
		"D1Re0",
		"D1Ar5",
		"D1Ar3",
		"D1Ar1",
		"D1RtP",
		"D1.5 ",
		"D1.6 ",
		"D1.7 "
	};

	pr_info(" pt_regs @ %p\n", regs);
	pr_info(" SaveMask = 0x%04hx\n", regs->ctx.SaveMask);
	pr_info(" Flags = 0x%04hx (%c%c%c%c)\n", regs->ctx.Flags,
		regs->ctx.Flags & FLAG_Z ? 'Z' : 'z',
		regs->ctx.Flags & FLAG_N ? 'N' : 'n',
		regs->ctx.Flags & FLAG_O ? 'O' : 'o',
		regs->ctx.Flags & FLAG_C ? 'C' : 'c');
	pr_info(" TXRPT = 0x%08x\n", regs->ctx.CurrRPT);
	pr_info(" PC = 0x%08x\n", regs->ctx.CurrPC);

	/* AX regs */
	for (i = 0; i < 2; i++) {
		pr_info(" %s = 0x%08x    ",
			AX0_names[i],
			regs->ctx.AX[i].U0);
		printk(" %s = 0x%08x\n",
			AX1_names[i],
			regs->ctx.AX[i].U1);
	}

	if (regs->ctx.SaveMask & TBICTX_XEXT_BIT)
		pr_warn(" Extended state present - AX2.[01] will be WRONG\n");

	/* Special place with AXx.2 */
	pr_info(" A0.2  = 0x%08x    ",
		regs->ctx.Ext.AX2.U0);
	printk(" A1.2  = 0x%08x\n",
		regs->ctx.Ext.AX2.U1);

	/* 'extended' AX regs (nominally, just AXx.3) */
	for (i = 0; i < (TBICTX_AX_REGS - 3); i++) {
		pr_info(" A0.%d  = 0x%08x    ", i + 3, regs->ctx.AX3[i].U0);
		printk(" A1.%d  = 0x%08x\n", i + 3, regs->ctx.AX3[i].U1);
	}

	for (i = 0; i < 8; i++) {
		pr_info(" %s = 0x%08x    ", DX0_names[i], regs->ctx.DX[i].U0);
		printk(" %s = 0x%08x\n", DX1_names[i], regs->ctx.DX[i].U1);
	}

	show_trace(NULL, (unsigned long *)regs->ctx.AX[0].U0, regs);
}

extern void kernel_thread_helper(void);
__asm__(".pushsection .text\n\t"
	".type _kernel_thread_helper,function\n\t"
	"_kernel_thread_helper:\n\t"
	"SWAP    PC,D1RtP\n\t"
	"MOV     D1Ar1,D0Re0\n\t"
	"MOVT    D1RtP,#HI(_do_exit)\n\t"
	"CALL    D1RtP,#LO(_do_exit)\n\t"
	".size   _kernel_thread_helper,.-_kernel_thread_helper\n\t"
	".popsection");

/*
 * Create a kernel thread
 */
int kernel_thread(int (*fn)(void *), void *arg, unsigned long flags)
{
	struct pt_regs regs;
	unsigned long global_base;

	memset(&regs, 0, sizeof(regs));
	regs.ctx.DX[3].U1 = (unsigned long)arg;
	regs.ctx.DX[4].U1 = (unsigned long)fn;

	asm volatile("MOV %0,A1GbP\n" : "=r" (global_base));
	regs.ctx.AX[0].U1 = (unsigned long)global_base;

	regs.ctx.CurrPC = (unsigned long)kernel_thread_helper;

	/* Ok, create the new process.. */
	return do_fork(flags | CLONE_VM | CLONE_UNTRACED, 0,
		       &regs, 0, NULL, NULL);
}

int copy_thread(unsigned long clone_flags, unsigned long usp,
		unsigned long unused, struct task_struct *tsk,
		struct pt_regs *regs)
{
	struct pt_regs *childregs = task_pt_regs(tsk);
	void *kernel_context = ((void *) childregs +
				sizeof(struct pt_regs));

	BUG_ON(((unsigned long)childregs) & 0x7);
	BUG_ON(((unsigned long)kernel_context) & 0x7);

	/* Get a pointer to where the new child's register block should have
	 * been pushed.
	 * The Meta's stack grows upwards, and the context is the the first
	 * thing to be pushed by TBX (phew)
	 */
	*childregs = *regs;

	/* Set the correct stack for the clone mode */
	if (user_mode(childregs)) {
		childregs->ctx.AX[0].U0 = ALIGN(usp, 8);
		tsk->thread.int_depth = 1;
	} else {
		childregs->ctx.AX[0].U0 = (unsigned long) kernel_context;
		tsk->thread.int_depth = 2;
	}

	/* set return value for child process */
	childregs->ctx.DX[0].U0 = 0;

	tsk->thread.kernel_context = __TBISwitchInit(kernel_context,
						     ret_from_fork,
						     0, 0);

	/* The TLS pointer is passed as an argument to sys_clone. */
	if (clone_flags & CLONE_SETTLS)
		tsk->thread.tls_ptr = (__force void __user *)regs->ctx.DX[1].U1;

#ifdef CONFIG_META_FPU
	if (tsk->thread.fpu_context) {
		struct meta_fpu_context *ctx;

		ctx = kmemdup(tsk->thread.fpu_context,
			      sizeof(struct meta_fpu_context), GFP_ATOMIC);
		tsk->thread.fpu_context = ctx;
	}
#endif

#ifdef CONFIG_META_DSP
	if (tsk->thread.dsp_context) {
		struct meta_ext_context *ctx;
		int i;

		ctx = kmemdup(tsk->thread.dsp_context,
			      sizeof(struct meta_ext_context), GFP_ATOMIC);
		for (i = 0; i < 2; i++)
			ctx->ram[i] = kmemdup(ctx->ram[i], ctx->ram_sz[i],
					      GFP_ATOMIC);
		tsk->thread.dsp_context = ctx;
	}
#endif

	return 0;
}

#ifdef CONFIG_META_FPU
static void alloc_fpu_context(struct thread_struct *thread)
{
	thread->fpu_context = kzalloc(sizeof(struct meta_fpu_context),
				      GFP_ATOMIC);
}

static void clear_fpu(struct thread_struct *thread)
{
	thread->user_flags &= ~TBICTX_FPAC_BIT;
	kfree(thread->fpu_context);
	thread->fpu_context = NULL;
}
#else
static void clear_fpu(struct thread_struct *thread)
{
}
#endif

#ifdef CONFIG_META_DSP
static void clear_dsp(struct thread_struct *thread)
{
	if (thread->dsp_context) {
		kfree(thread->dsp_context->ram[0]);
		kfree(thread->dsp_context->ram[1]);

		kfree(thread->dsp_context);

		thread->dsp_context = NULL;
	}

	TBI_SETREG(D0.8, 0);
}
#else
static void clear_dsp(struct thread_struct *thread)
{
}
#endif

struct task_struct *__sched __switch_to(struct task_struct *prev,
					struct task_struct *next)
{
	TBIRES to, from;

	to.Switch.pCtx = next->thread.kernel_context;
	to.Switch.pPara = prev;

#ifdef CONFIG_META_FPU
	if (prev->thread.user_flags & TBICTX_FPAC_BIT) {
		struct pt_regs *regs = task_pt_regs(prev);
		TBIRES state;

		state.Sig.SaveMask = prev->thread.user_flags;
		state.Sig.pCtx = &regs->ctx;

		if (!prev->thread.fpu_context)
			alloc_fpu_context(&prev->thread);
		if (prev->thread.fpu_context)
			__TBICtxFPUSave(state, prev->thread.fpu_context);
	}
	/*
	 * Force a restore of the FPU context next time this process is
	 * scheduled.
	 */
	if (prev->thread.fpu_context)
		prev->thread.fpu_context->needs_restore = true;
#endif


	if (current->thread.hwbp_context &&
		(current->thread.hwbp_context->written & META_HWBP_WRITTEN))
		restore_hwbp_controller(current->thread.hwbp_context);

	from = __TBISwitch(to, &prev->thread.kernel_context);

	/* Restore TLS pointer for this process. */
	set_gateway_tls(current->thread.tls_ptr);

	if (current->thread.hwbp_context)
		setup_hwbp_controller(current->thread.hwbp_context);

	return (struct task_struct *) from.Switch.pPara;
}

int sys_clone(unsigned long clone_flags,
	      unsigned long newsp,
	      unsigned long parent_tidptr,
	      unsigned long child_tidptr,
	      unsigned long tls,
	      unsigned long arg6,
	      struct pt_regs *pregs)
{
	if (!newsp)
		newsp = pregs->ctx.AX[0].U0;

	return do_fork(clone_flags, newsp, pregs, 0,
		       (int __user *)parent_tidptr,
		       (int __user *)child_tidptr);
}

/*
 * sys_execve() executes a new program.
 */
int sys_execve(char __user *ufilename,
	       const char __user *__user *uargv,
	       const char __user *__user *uenvp,
	       unsigned long arg4,
	       unsigned long arg5,
	       unsigned long arg6,
	       struct pt_regs *regs)
{
	int error;
	char *filename;

	filename = getname(ufilename);

	error = PTR_ERR(filename);
	if (IS_ERR(filename))
		goto out;

	error = do_execve(filename, uargv, uenvp, regs);
	putname(filename);
out:
	return error;
}

void flush_thread(void)
{
	clear_fpu(&current->thread);
	clear_dsp(&current->thread);
}

/*
 * Free current thread data structures etc.
 */
void exit_thread(void)
{
	clear_fpu(&current->thread);
	clear_dsp(&current->thread);
}

/* TODO: figure out how to unwind the kernel stack here to figure out
 * where we went to sleep. */
unsigned long get_wchan(struct task_struct *p)
{
	return 0;
}

int dump_fpu(struct pt_regs *regs, elf_fpregset_t *fpu)
{
	/* Returning 0 indicates that the FPU state was not stored (as it was
	 * not in use) */
	return 0;
}

#ifdef CONFIG_META_USER_TCM

#define ELF_MIN_ALIGN	PAGE_SIZE

#define ELF_PAGESTART(_v) ((_v) & ~(unsigned long)(ELF_MIN_ALIGN-1))
#define ELF_PAGEOFFSET(_v) ((_v) & (ELF_MIN_ALIGN-1))
#define ELF_PAGEALIGN(_v) (((_v) + ELF_MIN_ALIGN - 1) & ~(ELF_MIN_ALIGN - 1))

#define BAD_ADDR(x) ((unsigned long)(x) >= TASK_SIZE)

unsigned long __metag_elf_map(struct file *filep, unsigned long addr,
			      struct elf_phdr *eppnt, int prot, int type,
			      unsigned long total_size)
{
	unsigned long map_addr, size;
	unsigned long page_off = ELF_PAGEOFFSET(eppnt->p_vaddr);
	unsigned long raw_size = eppnt->p_filesz + page_off;
	unsigned long off = eppnt->p_offset - page_off;
	unsigned int tcm_tag;
	addr = ELF_PAGESTART(addr);
	size = ELF_PAGEALIGN(raw_size);

	/* mmap() will return -EINVAL if given a zero size, but a
	 * segment with zero filesize is perfectly valid */
	if (!size)
		return addr;

	tcm_tag = tcm_lookup_tag(addr);

	if (tcm_tag != TCM_INVALID_TAG)
		type &= ~MAP_FIXED;

	/*
	* total_size is the size of the ELF (interpreter) image.
	* The _first_ mmap needs to know the full size, otherwise
	* randomization might put this image into an overlapping
	* position with the ELF binary image. (since size < total_size)
	* So we first map the 'big' image - and unmap the remainder at
	* the end. (which unmap is needed for ELF images with holes.)
	*/
	if (total_size) {
		total_size = ELF_PAGEALIGN(total_size);
		map_addr = vm_mmap(filep, addr, total_size, prot, type, off);
		if (!BAD_ADDR(map_addr))
			vm_munmap(map_addr+size, total_size-size);
	} else
		map_addr = vm_mmap(filep, addr, size, prot, type, off);

	if (!BAD_ADDR(map_addr) && tcm_tag != TCM_INVALID_TAG) {
		struct tcm_allocation *tcm;
		unsigned long tcm_addr;

		tcm = kmalloc(sizeof(*tcm), GFP_KERNEL);
		if (!tcm)
			return -ENOMEM;

		tcm_addr = tcm_alloc(tcm_tag, raw_size);
		if (tcm_addr != addr) {
			kfree(tcm);
			return -ENOMEM;
		}

		tcm->tag = tcm_tag;
		tcm->addr = tcm_addr;
		tcm->size = raw_size;

		list_add(&tcm->list, &current->mm->context.tcm);

		eppnt->p_vaddr = map_addr;
		if (copy_from_user((void *) addr, (void __user *) map_addr,
				   raw_size))
			return -EFAULT;
	}

	return map_addr;
}
#endif
