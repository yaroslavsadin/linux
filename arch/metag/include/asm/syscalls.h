#ifndef _ASM_METAG_SYSCALLS_H
#define _ASM_METAG_SYSCALLS_H

#include <linux/compiler.h>
#include <linux/linkage.h>
#include <linux/types.h>
#include <linux/signal.h>

/* kernel/process.c */
asmlinkage int sys_fork(unsigned long, unsigned long,
			unsigned long, unsigned long,
			unsigned long, unsigned long,
			struct pt_regs *);
asmlinkage int sys_execve(char __user *, char __user *__user *,
			  char __user *__user *, unsigned long,
			  unsigned long, unsigned long,
			  struct pt_regs *);
asmlinkage int sys_clone(unsigned long, unsigned long,
			 unsigned long, unsigned long,
			 unsigned long, unsigned long,
			 struct pt_regs *);

/* kernel/signal.c */
asmlinkage int sys_sigaltstack(const stack_t __user *, stack_t __user *,
			       unsigned long, unsigned long,
			       unsigned long, unsigned long,
			       struct pt_regs *);
asmlinkage int sys_rt_sigreturn(unsigned long, unsigned long,
				unsigned long, unsigned long,
				unsigned long, unsigned long,
				struct pt_regs *);
asmlinkage long sys_rt_sigaction(int sig,
				 const struct sigaction __user *act,
				 struct sigaction __user *oact,
				 size_t sigsetsize);
asmlinkage long sys_rt_sigsuspend(sigset_t __user *unewset,
				  size_t sigsetsize);
asmlinkage long sys_rt_tgsigqueueinfo(pid_t, pid_t, int, siginfo_t __user *);

/* kernel/sys_metag.c */
asmlinkage long sys_mmap2(unsigned long, unsigned long, unsigned long,
			  unsigned long, unsigned long, unsigned long);
asmlinkage int sys_metag_spinlock(int __user *);
asmlinkage int sys_metag_setglobalbit(char __user *, int);
asmlinkage void sys_metag_set_fpu_flags(unsigned int);
asmlinkage int sys_metag_set_tls(void __user *);
asmlinkage void *sys_metag_get_tls(void);
asmlinkage long sys32_truncate64(const char __user *, unsigned long,
				 unsigned long);
asmlinkage long sys32_ftruncate64(unsigned int, unsigned long,
				  unsigned long);
asmlinkage long sys32_fadvise64_64(int, unsigned long, unsigned long,
				   unsigned long, unsigned long, int);
asmlinkage long sys32_readahead(int, unsigned long, unsigned long, size_t);
asmlinkage ssize_t sys32_pread64(unsigned long, char __user *, size_t,
				 unsigned long, unsigned long);
asmlinkage ssize_t sys32_pwrite64(unsigned long, char __user *, size_t,
				  unsigned long, unsigned long);
asmlinkage long sys32_sync_file_range(int, unsigned long, unsigned long,
				      unsigned long, unsigned long,
				      unsigned int);

extern void do_notify_resume(struct pt_regs *regs, int from_syscall,
			     unsigned int orig_syscall,
			     unsigned long thread_info_flags);

#endif /* _ASM_METAG_SYSCALLS_H */
