/*
 * This file contains various random system calls that
 * have a non-standard calling sequence on the Linux/Meta
 * platform.
 */

#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/syscalls.h>
#include <linux/mman.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/unistd.h>
#include <asm/switch.h>
#include <asm/syscall.h>
#include <asm/syscalls.h>
#include <asm/user_gateway.h>

#define merge_64(hi, lo) ((((unsigned long long)(hi)) << 32) + \
			  ((lo) & 0xffffffffUL))

int metag_mmap_check(unsigned long addr, unsigned long len,
		     unsigned long flags)
{
	/* We can't have people trying to write to the bottom of the
	 * memory map, there are mysterious unspecified things there that
	 * we don't want people trampling on.
	 */
	if ((flags & MAP_FIXED) && (addr < TASK_UNMAPPED_BASE))
		return -EINVAL;

	return 0;
}

asmlinkage long sys_mmap2(unsigned long addr, unsigned long len,
			  unsigned long prot, unsigned long flags,
			  unsigned long fd, unsigned long pgoff)
{
	/* The shift for mmap2 is constant, regardless of PAGE_SIZE setting. */
	if (pgoff & ((1 << (PAGE_SHIFT - 12)) - 1))
		return -EINVAL;

	pgoff >>= PAGE_SHIFT - 12;

	return sys_mmap_pgoff(addr, len, prot, flags, fd, pgoff);
}

asmlinkage int sys_metag_spinlock(int __user *spinlock)
{
	int ret = 0, tmp;

	local_irq_disable();
	get_user(tmp, spinlock);
	if (tmp)
		ret = 1;
	tmp = 1;
	put_user(tmp, spinlock);
	local_irq_enable();
	return ret;
}

asmlinkage int sys_metag_setglobalbit(char __user *addr, int mask)
{
	char tmp;
	int ret = 0;
	unsigned int flags;

	if (!((__force unsigned int)addr >= LINCORE_BASE))
		return -EFAULT;

	TBI_LOCK(flags);

	__TBIDataCacheFlush((__force void *)addr, sizeof(mask));

	ret = __get_user(tmp, addr);
	if (ret)
		goto out;
	tmp |= mask;
	ret = __put_user(tmp, addr);

	__TBIDataCacheFlush((__force void *)addr, sizeof(mask));

out:
	TBI_UNLOCK(flags);

	return ret;
}

/*
 * Do a system call from kernel instead of calling sys_execve so we
 * end up with proper pt_regs.
 */
int kernel_execve(const char *filename, const char *const argv[],
		  const char *const envp[])
{
	register long __call __asm__("D1Re0") = __NR_execve;
	register long __res __asm__("D0Re0");
	register long __a __asm__("D1Ar1") = (long)(filename);
	register long __b __asm__("D0Ar2") = (long)(argv);
	register long __c __asm__("D1Ar3") = (long)(envp);
	__asm__ __volatile__("SWITCH	#%c1"
			     : "=d" (__res)
			     : "i" (__METAG_SW_SYS), "d" (__call),
			       "d" (__a), "d" (__b), "d" (__c)
			     : "memory");
	return __res;
}

#define TXDEFR_FPU_MASK ((0x1f << 16) | 0x1f)

asmlinkage void sys_metag_set_fpu_flags(unsigned int flags)
{
	unsigned int temp;

	flags &= TXDEFR_FPU_MASK;

	__asm__ __volatile__("MOV %0,TXDEFR\n" : "=r" (temp));

	temp &= ~TXDEFR_FPU_MASK;
	temp |= flags;

	__asm__ __volatile__("MOV TXDEFR,%0\n" : : "r" (temp));
}

asmlinkage int sys_metag_set_tls(void __user *ptr)
{
	current->thread.tls_ptr = ptr;
	set_gateway_tls(ptr);

	return 0;
}

asmlinkage void *sys_metag_get_tls(void)
{
	return (__force void *)current->thread.tls_ptr;
}

asmlinkage long sys32_truncate64(const char __user *path, unsigned long lo,
				 unsigned long hi)
{
	return sys_truncate64(path, merge_64(hi, lo));
}

asmlinkage long sys32_ftruncate64(unsigned int fd, unsigned long lo,
				  unsigned long hi)
{
	return sys_ftruncate64(fd, merge_64(hi, lo));
}

asmlinkage long sys32_fadvise64_64(int fd, unsigned long offs_lo,
				   unsigned long offs_hi, unsigned long len_lo,
				   unsigned long len_hi, int advice)
{
	return sys_fadvise64_64(fd, merge_64(offs_hi, offs_lo),
				merge_64(len_hi, len_lo), advice);
}

asmlinkage long sys32_readahead(int fd, unsigned long lo, unsigned long hi,
				size_t count)
{
	return sys_readahead(fd, merge_64(hi, lo), count);
}

asmlinkage ssize_t sys32_pread64(unsigned long fd, char __user *buf,
				 size_t count, unsigned long lo,
				 unsigned long hi)
{
	return sys_pread64(fd, buf, count, merge_64(hi, lo));
}

asmlinkage ssize_t sys32_pwrite64(unsigned long fd, char __user *buf,
				  size_t count, unsigned long lo,
				  unsigned long hi)
{
	return sys_pwrite64(fd, buf, count, merge_64(hi, lo));
}

asmlinkage long sys32_sync_file_range(int fd, unsigned long offs_lo,
				      unsigned long offs_hi,
				      unsigned long len_lo,
				      unsigned long len_hi,
				      unsigned int flags)
{
	return sys_sync_file_range(fd, merge_64(offs_hi, offs_lo),
				   merge_64(len_hi, len_lo), flags);
}

/* Provide the actual syscall number to call mapping. */
#undef __SYSCALL
#define __SYSCALL(nr, call) [nr] = (call),

/*
 * Note that we can't include <linux/unistd.h> here since the header
 * guard will defeat us; <asm/unistd.h> checks for __SYSCALL as well.
 */
const void *sys_call_table[__NR_syscalls] = {
	[0 ... __NR_syscalls-1] = sys_ni_syscall,
#include <asm/unistd.h>
	/* we need wrappers for anything with unaligned 64bit arguments */
	__SYSCALL(__NR_truncate64, sys32_truncate64)
	__SYSCALL(__NR_ftruncate64, sys32_ftruncate64)
	__SYSCALL(__NR_fadvise64_64, sys32_fadvise64_64)
	__SYSCALL(__NR_readahead, sys32_readahead)
	__SYSCALL(__NR_pread64, sys32_pread64)
	__SYSCALL(__NR_pwrite64, sys32_pwrite64)
	__SYSCALL(__NR_sync_file_range, sys32_sync_file_range)
};
