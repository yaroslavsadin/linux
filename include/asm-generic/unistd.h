#if !defined(_ASM_GENERIC_UNISTD_H) || defined(__SYSCALL)
#ifndef __SYSCALL
#endif
#if __BITS_PER_LONG == 32 || defined(__SYSCALL_COMPAT)
#else
#endif
#ifdef __SYSCALL_COMPAT
#else
#endif
#ifdef __ARCH_WANT_SYNC_FILE_RANGE2
#else
#endif
#ifndef __ARCH_NOMMU
#endif
#ifdef __ARCH_WANT_SYSCALL_NO_AT
#endif /* __ARCH_WANT_SYSCALL_NO_AT */
#ifdef __ARCH_WANT_SYSCALL_NO_FLAGS
#endif /* __ARCH_WANT_SYSCALL_NO_FLAGS */
#if (__BITS_PER_LONG == 32 || defined(__SYSCALL_COMPAT)) && \
     defined(__ARCH_WANT_SYSCALL_OFF_T)
#endif /* 32 bit off_t syscalls */
#ifdef __ARCH_WANT_SYSCALL_DEPRECATED
#ifdef CONFIG_MMU
#else
#endif /* CONFIG_MMU */
#endif /* __ARCH_WANT_SYSCALL_DEPRECATED */
#if __BITS_PER_LONG == 64 && !defined(__SYSCALL_COMPAT)
#ifdef __NR3264_stat
#endif
#else
#ifdef __NR3264_stat
#endif
#endif

/*
 * These are required system calls, we should
 * invert the logic eventually and let them
 * be selected by default.
 */
#if __BITS_PER_LONG == 32
#define __ARCH_WANT_STAT64
#define __ARCH_WANT_SYS_LLSEEK
#endif
#define __ARCH_WANT_SYS_RT_SIGACTION
#define __ARCH_WANT_SYS_RT_SIGSUSPEND
#define __ARCH_WANT_COMPAT_SYS_RT_SIGSUSPEND

/*
 * "Conditional" syscalls
 *
 * What we want is __attribute__((weak,alias("sys_ni_syscall"))),
 * but it doesn't work on all toolchains, so we just do it by hand
 */
#ifndef cond_syscall
#define cond_syscall(x) asm(".weak\t" #x "\n\t.set\t" #x ",sys_ni_syscall")
#endif

#endif /* _ASM_GENERIC_UNISTD_H */
