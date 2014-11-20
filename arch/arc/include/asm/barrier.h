/*
 * Copyright (C) 2004, 2007-2010, 2011-2012 Synopsys, Inc. (www.synopsys.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __ASM_BARRIER_H
#define __ASM_BARRIER_H

#ifndef __ASSEMBLY__

#ifdef CONFIG_SMP
#define mb() __asm__ __volatile__ ("sync" : : : "memory")
#else
#define mb() __asm__ __volatile__ ("" : : : "memory")
#endif

#define rmb() mb()
#define wmb() mb()

/* TBD: can this be made smp_mb */
#define set_mb(var, value)  do { var = value; mb(); } while (0)

/* TBD: Not needed except for Alpha */
#define read_barrier_depends()  mb()

/* TODO-vineetg verify the correctness of macros here */
#ifdef CONFIG_SMP
#define smp_mb()        mb()
#define smp_rmb()       rmb()
#define smp_wmb()       wmb()
#else
#define smp_mb()        barrier()
#define smp_rmb()       barrier()
#define smp_wmb()       barrier()
#endif

#define smp_mb__before_atomic_dec()	smp_mb()
#define smp_mb__after_atomic_dec()	smp_mb()
#define smp_mb__before_atomic_inc()	smp_mb()
#define smp_mb__after_atomic_inc()	smp_mb()

/* TBD: Not needed except for Alpha */
#define smp_read_barrier_depends()      smp_mb()

#endif

#endif
