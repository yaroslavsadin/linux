/*
 * Copyright (C) 2011 Imagination Technologies
 */

#ifndef _METAG_IRQ_INTERNAL_H_
#define _METAG_IRQ_INTERNAL_H_

#define HWSTATMETA_OFFSET_MAX	32

#ifdef CONFIG_META_PERFCOUNTER_IRQS
extern void init_internal_IRQ(void);
#else
#define init_internal_IRQ() do {} while (0)
#endif

#endif /* _METAG_IRQ_INTERNAL_H_ */
