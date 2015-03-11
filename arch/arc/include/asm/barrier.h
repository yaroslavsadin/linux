/*
 * Copyright (C) 2014-15 Synopsys, Inc. (www.synopsys.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __ASM_BARRIER_H
#define __ASM_BARRIER_H

#ifdef CONFIG_SMP

#ifdef CONFIG_ISA_ARCV2

/* DMB + SYNC semantics */
#define mb()		asm volatile("dsync\n": : : "memory")

#define smp_mb()	asm volatile("dmb 3\n": : : "memory")
#define smp_rmb()	asm volatile("dmb 1\n": : : "memory")
#define smp_wmb()	asm volatile("dmb 2\n": : : "memory")

#else	/* CONFIG_ISA_ARCOMPACT */

#define mb()		asm volatile("sync \n" : : : "memory")

#endif	/* CONFIG_ISA_ARCV2 */

#endif	/* CONFIG_SMP */

#include <asm-generic/barrier.h>

#endif
