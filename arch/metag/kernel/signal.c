/*
 *  Copyright (C) 1991,1992  Linus Torvalds
 *  Copyright (C) 2005,2006,2007,2008,2009  Imagination Technologies Ltd.
 *
 *  1997-11-28  Modified for POSIX.1b signals by Richard Henderson
 *
 */

#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/smp.h>
#include <linux/kernel.h>
#include <linux/signal.h>
#include <linux/errno.h>
#include <linux/wait.h>
#include <linux/ptrace.h>
#include <linux/unistd.h>
#include <linux/stddef.h>
#include <linux/personality.h>
#include <linux/uaccess.h>
#include <linux/tracehook.h>

#include <asm/ucontext.h>
#include <asm/cacheflush.h>
#include <asm/switch.h>
#include <asm/syscall.h>
#include <asm/syscalls.h>

#define REG_FLAGS	ctx.SaveMask
#define REG_RETVAL	ctx.DX[0].U0
#define REG_SYSCALL	ctx.DX[0].U1
#define REG_SP		ctx.AX[0].U0
#define REG_ARG1	ctx.DX[3].U1
#define REG_ARG2	ctx.DX[3].U0
#define REG_ARG3	ctx.DX[2].U1
#define REG_PC		ctx.CurrPC
#define REG_RTP		ctx.DX[4].U1

int sys_sigaltstack(const stack_t __user *uss,
		    stack_t __user *uoss,
		    unsigned long arg3,
		    unsigned long arg4,
		    unsigned long arg5,
		    unsigned long arg6,
		    struct pt_regs *regs)
{
	return do_sigaltstack(uss, uoss, regs->REG_SP);
}

struct rt_sigframe {
	struct siginfo info;
	struct ucontext uc;
	unsigned long retcode[2];
};

static int restore_sigcontext(struct pt_regs *regs,
			      struct sigcontext __user *sc)
{
	int err;

	/* Always make any pending restarted system calls return -EINTR */
	current_thread_info()->restart_block.fn = do_no_restart_syscall;

	err = __copy_from_user(&regs->ctx, sc, sizeof(regs->ctx));

	if (regs->REG_FLAGS & TBICTX_XCBF_BIT) {
		err |= __copy_from_user(regs->extcb0,
					(void __user *)((unsigned long)sc +
							sizeof(regs->ctx)),
					sizeof(regs->extcb0));
	}

	/* This is a user-mode context. */
	regs->REG_FLAGS |= TBICTX_PRIV_BIT;

	return err;
}

int sys_rt_sigreturn(unsigned long arg1,
		     unsigned long arg2,
		     unsigned long arg3,
		     unsigned long arg4,
		     unsigned long arg5,
		     unsigned long arg6,
		     struct pt_regs *regs)    /* regs always arg7 on Meta */
{
	/* NOTE - Meta stack goes UPWARDS - so we wind the stack back */
	struct rt_sigframe __user *frame;
	sigset_t set;
	stack_t st;

	frame = (__force struct rt_sigframe __user *)(regs->REG_SP -
						      sizeof(*frame));

	if (!access_ok(VERIFY_READ, frame, sizeof(*frame)))
		goto badframe;

	if (__copy_from_user(&set, &frame->uc.uc_sigmask, sizeof(set)))
		goto badframe;

	set_current_blocked(&set);

	if (restore_sigcontext(regs, &frame->uc.uc_mcontext))
		goto badframe;

	if (__copy_from_user(&st, &frame->uc.uc_stack, sizeof(st)))
		goto badframe;
	/* It is more difficult to avoid calling this function than to
	   call it and ignore errors.  */
	do_sigaltstack((__force const stack_t __user *)&st, NULL, regs->REG_SP);

	return regs->REG_RETVAL;

badframe:
	force_sig(SIGSEGV, current);

	return 0;
}

static int setup_sigcontext(struct sigcontext __user *sc, struct pt_regs *regs,
			    unsigned long mask)
{
	int err;

	err = __copy_to_user(sc, &regs->ctx, sizeof(regs->ctx));

	if (regs->REG_FLAGS & TBICTX_XCBF_BIT) {
		err |= __copy_to_user((void __user *)((unsigned long)sc +
						      sizeof(regs->ctx)),
				      regs->extcb0,
				      sizeof(regs->extcb0));
	}

	/* OK, clear that cbuf flag in the old context, or our stored
	 * catch buffer will be restored when we go to call the signal
	 * handler. Also clear out the CBRP RA/RD pipe bit incase
	 * that is pending as well!
	 * Note that as we have already stored this context, these
	 * flags will get restored on sigreturn to their original
	 * state.
	 */
	regs->REG_FLAGS &= ~(TBICTX_XCBF_BIT | TBICTX_CBUF_BIT |
			     TBICTX_CBRP_BIT);

	/* Clear out the LSM_STEP bits in case we are in the middle of
	 * and MSET/MGET.
	 */
	regs->ctx.Flags &= ~TXSTATUS_LSM_STEP_BITS;

	err |= __put_user(mask, &sc->oldmask);

	return err;
}

/*
 * Determine which stack to use..
 */
static void __user *get_sigframe(struct k_sigaction *ka, unsigned long sp,
				 size_t frame_size)
{
	/* Meta stacks grows upwards */
	if ((ka->sa.sa_flags & SA_ONSTACK) && (sas_ss_flags(sp) == 0))
		sp = current->sas_ss_sp;

	sp = (sp + 7) & ~7;			/* 8byte align stack */

	return (void __user *)sp;
}

static int setup_rt_frame(int sig, struct k_sigaction *ka, siginfo_t *info,
			  sigset_t *set, struct pt_regs *regs)
{
	struct rt_sigframe __user *frame;
	int err = -EFAULT;
	unsigned long code;

	frame = get_sigframe(ka, regs->REG_SP, sizeof(*frame));
	if (!access_ok(VERIFY_WRITE, frame, sizeof(*frame)))
		goto out;

	err = copy_siginfo_to_user(&frame->info, info);

	/* Create the ucontext.  */
	err |= __put_user(0, &frame->uc.uc_flags);
	err |= __put_user(0, (unsigned long __user *)&frame->uc.uc_link);
	err |= __put_user(current->sas_ss_sp,
			  (unsigned long __user *)&frame->uc.uc_stack.ss_sp);
	err |= __put_user(sas_ss_flags(regs->REG_SP),
			  &frame->uc.uc_stack.ss_flags);
	err |= __put_user(current->sas_ss_size, &frame->uc.uc_stack.ss_size);
	err |= setup_sigcontext(&frame->uc.uc_mcontext,
				regs, set->sig[0]);
	err |= __copy_to_user(&frame->uc.uc_sigmask, set, sizeof(*set));

	if (err)
		goto out;

	/* Set up to return from userspace.  */

	/* MOV D1Re0 (D1.0), #__NR_rt_sigreturn */
	code = 0x03000004 | (__NR_rt_sigreturn << 3);
	err |= __put_user(code, (unsigned long __user *)(&frame->retcode[0]));

	/* SWITCH #__METAG_SW_SYS */
	code = __METAG_SW_ENCODING(SYS);
	err |= __put_user(code, (unsigned long __user *)(&frame->retcode[1]));

	if (err)
		goto out;

	/* Set up registers for signal handler */
	regs->REG_RTP = (unsigned long) frame->retcode;
	regs->REG_SP = (unsigned long) frame + sizeof(*frame);
	regs->REG_ARG1 = sig;
	regs->REG_ARG2 = (unsigned long) &frame->info;
	regs->REG_ARG3 = (unsigned long) &frame->uc;
	regs->REG_PC = (unsigned long) ka->sa.sa_handler;

	pr_debug("SIG deliver (%s:%d): sp=%p pc=%08x pr=%08x\n",
		 current->comm, current->pid, frame, regs->REG_PC,
		 regs->REG_RTP);

	/* Now pass size of 'new code' into sigtramp so we can do a more
	 * effective cache flush - directed rather than 'full flush'.
	 */
	flush_cache_sigtramp(regs->REG_RTP, sizeof(frame->retcode));
out:
	if (err) {
		force_sigsegv(sig, current);
		return -EFAULT;
	}
	return 0;
}

static void handle_signal(unsigned long sig, siginfo_t *info,
			  struct k_sigaction *ka, struct pt_regs *regs,
			  int from_syscall, unsigned int orig_syscall)
{
	sigset_t *oldset = sigmask_to_save();

	/* Are we from a system call? */
	if (from_syscall) {
		/* If so, check system call restarting.. */
		switch (syscall_get_error(current, regs)) {
		case -ERESTART_RESTARTBLOCK:
		case -ERESTARTNOHAND:
			regs->REG_RETVAL = -EINTR;
			break;

		case -ERESTARTSYS:
			if (!(ka->sa.sa_flags & SA_RESTART)) {
				regs->REG_RETVAL = -EINTR;
				break;
			}
		/* fallthrough */
		case -ERESTARTNOINTR:
			regs->REG_SYSCALL = orig_syscall;
			regs->REG_PC -= 4;
			break;
		}
	}

	/* Set up the stack frame */
	if (setup_rt_frame(sig, ka, info, oldset, regs))
		return;

	signal_delivered(sig, info, ka, regs, test_thread_flag(TIF_SINGLESTEP));
}

 /* Notes for Meta.
  * We have moved from the old 2.4.9 SH way of using syscall_nr (in the stored
  * context) to passing in the syscall flag and orig_syscall on the stack.
  * This is for two reasons:
  * 1) having syscall_nr in our context does not fit with TBI, and corrupted
  *    the stack.
  * 2) We need to have orig_syscall and from_syscall to implement syscall
  *    restarting.
  */
static void do_signal(struct pt_regs *regs, int from_syscall,
		      unsigned int orig_syscall)
{
	struct k_sigaction ka;
	siginfo_t info;
	int signr;

	/*
	 * We want the common case to go fast, which
	 * is why we may in certain cases get here from
	 * kernel mode. Just return without doing anything
	 * if so.
	 */
	if (!user_mode(regs))
		return;

	signr = get_signal_to_deliver(&info, &ka, regs, NULL);
	if (signr > 0) {
		/* Whee! Actually deliver the signal.  */
		handle_signal(signr, &info, &ka, regs, from_syscall,
			      orig_syscall);
		return;
	}

	/* Did we come from a system call? */
	if (from_syscall) {
		/* Restart the system call - no handlers present */
		switch (syscall_get_error(current, regs)) {
		case -ERESTARTNOHAND:
		case -ERESTARTSYS:
		case -ERESTARTNOINTR:
			regs->REG_SYSCALL = orig_syscall;
			regs->REG_PC -= 4;
			break;

		case -ERESTART_RESTARTBLOCK:
			regs->REG_SYSCALL = __NR_restart_syscall;
			regs->REG_PC -= 4;
			break;
		}
	}

	/*
	 * If there's no signal to deliver, we just put the saved sigmask
	 * back.
	 */
	restore_saved_sigmask();
}

void do_notify_resume(struct pt_regs *regs, int from_syscall,
		      unsigned int orig_syscall,
		      unsigned long thread_info_flags)
{
	/* deal with pending signal delivery */
	if (thread_info_flags & _TIF_SIGPENDING)
		do_signal(regs, from_syscall, orig_syscall);

	if (thread_info_flags & _TIF_NOTIFY_RESUME) {
		clear_thread_flag(TIF_NOTIFY_RESUME);
		tracehook_notify_resume(regs);
	}
}
