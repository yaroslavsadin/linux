/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2004, 2007-2010, 2011-2012 Synopsys, Inc. (www.synopsys.com)
 */

#ifndef __ASM_ARC_CMPXCHG_H
#define __ASM_ARC_CMPXCHG_H

#include <linux/build_bug.h>
#include <linux/types.h>

#include <asm/barrier.h>
#include <asm/smp.h>

#ifdef CONFIG_ARC_HAS_LLSC

/*
 * if (*ptr == @old)
 *      *ptr = @new
 */
#define __cmpxchg_relaxed(ptr, old, new)				\
({									\
	__typeof__(*(ptr)) _prev;					\
									\
	__asm__ __volatile__(						\
	"1:	llock  %0, [%1]	\n"					\
	"	brne   %0, %2, 2f	\n"				\
	"	scond  %3, [%1]	\n"					\
	"	bnz     1b		\n"				\
	"2:				\n"				\
	: "=&r"(_prev)	/* Early clobber prevent reg reuse */		\
	: "r"(ptr),	/* Not "m": llock only supports reg */		\
	  "ir"(old),							\
	  "r"(new)	/* Not "ir": scond can't take LIMM */		\
	: "cc",								\
	  "memory");	/* gcc knows memory is clobbered */		\
									\
	_prev;								\
})

#ifdef CONFIG_64BIT

#define __cmpxchg64_relaxed(ptr, old, new)				\
({									\
	__typeof__(*(ptr)) __prev;					\
									\
	__asm__ __volatile__(						\
	"1:	llockl  %0, [%1]	\n"				\
	"	brnel   %0, %2, 2f	\n"				\
	"	scondl  %3, [%1]	\n"				\
	"	bnz     1b		\n"				\
	"2:				\n"				\
	: "=&r"(__prev)							\
	: "r"(ptr),							\
	  "ir"(old),						\
	  "r"(new)							\
	: "cc",								\
	  "memory");							\
									\
	__prev;								\
})

#else

#define __cmpxchg64_relaxed(ptr, old, new)				\
({									\
	BUILD_BUG();							\
	(__typeof__(*(ptr))) -1UL;					\
})
#endif

#define cmpxchg(ptr, old, new)					        \
({									\
	__typeof__(ptr) _p_ = (ptr);					\
	__typeof__(*(ptr)) _o_ = (old);					\
	__typeof__(*(ptr)) _n_ = (new);					\
	__typeof__(*(ptr)) _prev_;					\
									\
	switch(sizeof(*(_p_))) {				        \
	case 4:								\
	        /*							\
		 * Explicit full memory barrier needed before/after	\
	         */							\
		smp_mb();						\
		_prev_ = __cmpxchg_relaxed(_p_, _o_, _n_);		\
		smp_mb();						\
		break;							\
	case 8:								\
		smp_mb();						\
		_prev_ = __cmpxchg64_relaxed(_p_, _o_, _n_);		\
		smp_mb();						\
		break;							\
	default:							\
		BUILD_BUG();						\
	}								\
	_prev_;								\
})

#define cmpxchg_relaxed(ptr, old, new)				        \
({									\
	__typeof__(ptr) _p_ = (ptr);					\
	__typeof__(*(ptr)) _o_ = (old);					\
	__typeof__(*(ptr)) _n_ = (new);					\
	__typeof__(*(ptr)) _prev_;					\
									\
	switch(sizeof(*(_p_))) {			        	\
	case 4:								\
		_prev_ = __cmpxchg_relaxed(_p_, _o_, _n_);		\
		break;							\
	case 8:								\
		_prev_ = __cmpxchg64_relaxed(_p_, _o_, _n_);	\
		break;							\
	default:							\
		BUILD_BUG();						\
	}								\
	_prev_;								\
})

#elif defined(CONFIG_ARC_PLAT_EZNPS)

#define cmpxchg(ptr, old, new)					        \
({									\
	__typeof__(ptr) _p_ = (ptr);					\
	__typeof__(*(ptr)) _o_ = (old);					\
	__typeof__(*(ptr)) _n_ = (new);					\
									\
	BUILD_BUG_ON(sizeof(*_p_) != 4);				\
									\
	/*								\
	 * Explicit full memory barrier needed before/after		\
	 */								\
	smp_mb();							\
									\
	write_aux_reg(CTOP_AUX_GPA1, _o_);			        \
									\
	__asm__ __volatile__(						\
	"	mov r2, %0\n"						\
	"	mov r3, %1\n"						\
	"	.word %2\n"						\
	"	mov %0, r2"						\
	: "+r"(_n_)							\
	: "r"(_p_), "i"(CTOP_INST_EXC_DI_R2_R2_R3)			\
	: "r2", "r3", "memory");					\
									\
	smp_mb();							\
									\
	_n_;	/* semantically old value */				\
})

#else

#define cmpxchg(ptr, old, new)					        \
({									\
	volatile __typeof__(ptr) _p_ = (ptr);				\
	__typeof__(*(ptr)) _o_ = (o);					\
	__typeof__(*(ptr)) _n_ = (n);					\
	__typeof__(*(ptr)) _prev_;					\
									\
	BUILD_BUG_ON(sizeof(*_p_) != 4);				\
									\
	unsigned long __flags;						\
									\
	/*								\
	 * spin lock/unlock provide the needed smp_mb() before/after	\
	 */								\
	atomic_ops_lock(__flags);					\
	_prev_ = *_p_;							\
	if (_prev_ == _o_)						\
		*_p_ = _n_;						\
	atomic_ops_unlock(__flags);					\
	_prev_;								\
})

#endif


/*
 * xchg
 */
#ifdef CONFIG_ARC_HAS_LLSC

#define __xchg_relaxed(ptr, val)					\
({									\
	__asm__ __volatile__(						\
	"	ex  %0, [%1]	\n"	/* set new value */	        \
	: "+r"(val)							\
	: "r"(ptr)							\
	: "memory");							\
	_val_;		/* get old value */				\
})

#ifdef CONFIG_64BIT

#define __xchg64_relaxed(ptr, val)					\
({									\
	__asm__ __volatile__(						\
	"	exl  %0, [%1]	\n"					\
	: "+r"(val)							\
	: "r"(ptr)							\
	: "memory");							\
	_val_;								\
})

#else

#define __xchg64_relaxed(ptr, val)					\
({									\
	BUILD_BUG();							\
	(__typeof__(*(ptr))) -1UL;					\
})
#endif

#define xchg(ptr, val)							\
({									\
	__typeof__(ptr) _p_ = (ptr);					\
	__typeof__(*(ptr)) _val_ = (val);				\
									\
	switch(sizeof(*(_p_))) {					\
	case 4:								\
		smp_mb();						\
		_val_ = __xchg_relaxed(_p_, _val_);			\
	        smp_mb();						\
		break;							\
	case 8:								\
		smp_mb();						\
		_val_ = __xchg64_relaxed(_p_, _val_);			\
	        smp_mb();						\
		break;							\
	default:							\
		BUILD_BUG();						\
	}								\
	_val_;								\
})

#define xchg_relaxed(ptr, val)						\
({									\
	__typeof__(ptr) _p_ = (ptr);					\
	__typeof__(*(ptr)) _val_ = (val);				\
									\
	switch(sizeof(*(_p_))) {					\
	case 4:								\
		_val_ = __xchg_relaxed(_p_, _val_);			\
		break;							\
	case 8:								\
		_val_ = __xchg64_relaxed(_p_, _val_);			\
		break;							\
	default:							\
		BUILD_BUG();						\
	}								\
	_val_;								\
})

#elif defined(CONFIG_ARC_PLAT_EZNPS)

#define xchg(ptr, val)						        \
({									\
	__typeof__(ptr) _p_ = (ptr);					\
	__typeof__(*(ptr)) _val_ = (val);				\
									\
	smp_mb();							\
									\
	__asm__ __volatile__(						\
	"	mov r2, %0\n"						\
	"	mov r3, %1\n"						\
	"	.word %2\n"						\
	"	mov %0, r2\n"						\
	: "+r"(_val_)							\
	: "r"(_p_), "i"(CTOP_INST_XEX_DI_R2_R2_R3)			\
	: "r2", "r3", "memory");					\
									\
	smp_mb();							\
									\
	_val_;								\
})

#else  /* !CONFIG_ARC_HAS_LLSC */

/*
 * EX instructions is baseline and present in !LLSC too. But in this
 * regime it still needs use @atomic_ops_lock spinlock to allow interop
 * with cmpxchg() which uses spinlock in !LLSC
 * (llist.h use xchg and cmpxchg on sama data)
 */

#define xchg(ptr, val)						        \
({									\
	__typeof__(ptr) _p_ = (ptr);					\
	__typeof__(*(ptr)) _val_ = (val);				\
									\
	unsigned long __flags;						\
									\
	atomic_ops_lock(__flags);					\
									\
	__asm__ __volatile__(						\
	"	ex  %0, [%1]	\n"					\
	: "+r"(_val_)							\
	: "r"(_p_)							\
	: "memory");							\
									\
	atomic_ops_unlock(__flags);					\
	_val_;								\
})

#endif

#endif
