/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Non hardware assisted Atomic-R-M-W
 * Locking would change to irq-disabling only (UP) and spinlocks (SMP)
 */

#ifndef _ASM_ARC_ATOMIC_SPINLOCK_H
#define _ASM_ARC_ATOMIC_SPINLOCK_H

#define atomic_read(v)          READ_ONCE((v)->counter)

static inline void atomic_set(atomic_t *v, int i)
{
	/*
	 * atomic_set() despite being 1 insn (and seemingly atomic) requires
	 * locking to avoid clobbering multi-insn r-m-w atomics
	 */
	unsigned long flags;

	atomic_ops_lock(flags);
	WRITE_ONCE(v->counter, i);
	atomic_ops_unlock(flags);
}

#define atomic_set_release(v, i)	atomic_set((v), (i))

#define ATOMIC_OP(op, c_op, asm_op)					\
static inline void atomic_##op(int i, atomic_t *v)			\
{									\
	unsigned long flags;						\
									\
	atomic_ops_lock(flags);						\
	v->counter c_op i;						\
	atomic_ops_unlock(flags);					\
}

#define ATOMIC_OP_RETURN(op, c_op, asm_op)				\
static inline int atomic_##op##_return(int i, atomic_t *v)		\
{									\
	unsigned long flags;						\
	unsigned int temp;						\
									\
	/*								\
	 * spin lock/unlock provides the needed smp_mb() before/after	\
	 */								\
	atomic_ops_lock(flags);						\
	temp = v->counter;						\
	temp c_op i;							\
	v->counter = temp;						\
	atomic_ops_unlock(flags);					\
									\
	return temp;							\
}

#define ATOMIC_FETCH_OP(op, c_op, asm_op)				\
static inline int atomic_fetch_##op(int i, atomic_t *v)			\
{									\
	unsigned long flags;						\
	unsigned int orig;						\
									\
	/*								\
	 * spin lock/unlock provides the needed smp_mb() before/after	\
	 */								\
	atomic_ops_lock(flags);						\
	orig = v->counter;						\
	v->counter c_op i;						\
	atomic_ops_unlock(flags);					\
									\
	return orig;							\
}

#define ATOMIC_OPS(op, c_op, asm_op)					\
	ATOMIC_OP(op, c_op, asm_op)					\
	ATOMIC_OP_RETURN(op, c_op, asm_op)				\
	ATOMIC_FETCH_OP(op, c_op, asm_op)

ATOMIC_OPS(add, +=, add)
ATOMIC_OPS(sub, -=, sub)

#define atomic_andnot		atomic_andnot
#define atomic_fetch_andnot	atomic_fetch_andnot

#undef ATOMIC_OPS
#define ATOMIC_OPS(op, c_op, asm_op)					\
	ATOMIC_OP(op, c_op, asm_op)					\
	ATOMIC_FETCH_OP(op, c_op, asm_op)

ATOMIC_OPS(and, &=, and)
ATOMIC_OPS(andnot, &= ~, bic)
ATOMIC_OPS(or, |=, or)
ATOMIC_OPS(xor, ^=, xor)

#endif
