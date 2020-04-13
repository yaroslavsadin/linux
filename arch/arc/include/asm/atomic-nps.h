/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef _ASM_ARC_ATOMIC_NPS_H
#define _ASM_ARC_ATOMIC_NPS_H

static inline int atomic_read(const atomic_t *v)
{
	int temp;

	__asm__ __volatile__(
	"	ld.di %0, [%1]"
	: "=r"(temp)
	: "r"(&v->counter)
	: "memory");
	return temp;
}

static inline void atomic_set(atomic_t *v, int i)
{
	__asm__ __volatile__(
	"	st.di %0,[%1]"
	:
	: "r"(i), "r"(&v->counter)
	: "memory");
}

#define ATOMIC_OP(op, c_op, asm_op)					\
static inline void atomic_##op(int i, atomic_t *v)			\
{									\
	__asm__ __volatile__(						\
	"	mov r2, %0\n"						\
	"	mov r3, %1\n"						\
	"       .word %2\n"						\
	:								\
	: "r"(i), "r"(&v->counter), "i"(asm_op)				\
	: "r2", "r3", "memory");					\
}									\

#define ATOMIC_OP_RETURN(op, c_op, asm_op)				\
static inline int atomic_##op##_return(int i, atomic_t *v)		\
{									\
	unsigned int temp = i;						\
									\
	/* Explicit full memory barrier needed before/after */		\
	smp_mb();							\
									\
	__asm__ __volatile__(						\
	"	mov r2, %0\n"						\
	"	mov r3, %1\n"						\
	"       .word %2\n"						\
	"	mov %0, r2"						\
	: "+r"(temp)							\
	: "r"(&v->counter), "i"(asm_op)					\
	: "r2", "r3", "memory");					\
									\
	smp_mb();							\
									\
	temp c_op i;							\
									\
	return temp;							\
}

#define ATOMIC_FETCH_OP(op, c_op, asm_op)				\
static inline int atomic_fetch_##op(int i, atomic_t *v)			\
{									\
	unsigned int temp = i;						\
									\
	/* Explicit full memory barrier needed before/after */		\
	smp_mb();							\
									\
	__asm__ __volatile__(						\
	"	mov r2, %0\n"						\
	"	mov r3, %1\n"						\
	"       .word %2\n"						\
	"	mov %0, r2"						\
	: "+r"(temp)							\
	: "r"(&v->counter), "i"(asm_op)					\
	: "r2", "r3", "memory");					\
									\
	smp_mb();							\
									\
	return temp;							\
}

#define ATOMIC_OPS(op, c_op, asm_op)					\
	ATOMIC_OP(op, c_op, asm_op)					\
	ATOMIC_OP_RETURN(op, c_op, asm_op)				\
	ATOMIC_FETCH_OP(op, c_op, asm_op)

ATOMIC_OPS(add, +=, CTOP_INST_AADD_DI_R2_R2_R3)

#define atomic_sub(i, v) atomic_add(-(i), (v))
#define atomic_sub_return(i, v) atomic_add_return(-(i), (v))
#define atomic_fetch_sub(i, v) atomic_fetch_add(-(i), (v))

#undef ATOMIC_OPS
#define ATOMIC_OPS(op, c_op, asm_op)					\
	ATOMIC_OP(op, c_op, asm_op)					\
	ATOMIC_FETCH_OP(op, c_op, asm_op)

ATOMIC_OPS(and, &=, CTOP_INST_AAND_DI_R2_R2_R3)
ATOMIC_OPS(or, |=, CTOP_INST_AOR_DI_R2_R2_R3)
ATOMIC_OPS(xor, ^=, CTOP_INST_AXOR_DI_R2_R2_R3)

#endif
