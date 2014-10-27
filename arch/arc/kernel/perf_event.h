/*
 * Linux performance counter support for ARC
 *
 * Copyright (C) 2014-2015 Synopsys, Inc. (www.synopsys.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#ifndef __ARC_PERF_EVENT_H
#define __ARC_PERF_EVENT_H

/* Max number of counters that CPU may have */
#define ARC_PERF_MAX_COUNTERS	64

/* Max number of countable events that CPU may have */
#define ARC_PERF_MAX_EVENTS	256

#define ARC_REG_CC_BUILD	0xF6
#define ARC_REG_CC_INDEX	0x240
#define ARC_REG_CC_NAME0	0x241
#define ARC_REG_CC_NAME1	0x242

#define ARC_REG_PCT_BUILD	0xF5
#define ARC_REG_PCT_COUNTL	0x250
#define ARC_REG_PCT_COUNTH	0x251
#define ARC_REG_PCT_SNAPL	0x252
#define ARC_REG_PCT_SNAPH	0x253
#define ARC_REG_PCT_CONFIG	0x254
#define ARC_REG_PCT_CONTROL	0x255
#define ARC_REG_PCT_INDEX	0x256
#define ARC_REG_PCT_INT_CNTL	0x25C
#define ARC_REG_PCT_INT_CNTH	0x25D
#define ARC_REG_PCT_INT_CTRL	0x25E
#define ARC_REG_PCT_INT_ACT	0x25F

#define ARC_REG_PCT_CONFIG_USER	(1 << 18)	/* count in user mode */
#define ARC_REG_PCT_CONFIG_KERN	(1 << 19)	/* count in kernel mode */

#define ARC_REG_PCT_CONTROL_CC	(1 << 16)	/* clear counts */
#define ARC_REG_PCT_CONTROL_SN	(1 << 17)	/* snapshot */

struct arc_reg_pct_build {
#ifdef CONFIG_CPU_BIG_ENDIAN
	unsigned int m:8, c:8, r:5, i:1, s:2, v:8;
#else
	unsigned int v:8, s:2, i:1, r:5, c:8, m:8;
#endif
};

struct arc_reg_cc_build {
#ifdef CONFIG_CPU_BIG_ENDIAN
	unsigned int c:16, r:8, v:8;
#else
	unsigned int v:8, r:8, c:16;
#endif
};

#define PERF_COUNT_ARC_DCLM	(PERF_COUNT_HW_MAX + 0)
#define PERF_COUNT_ARC_DCSM	(PERF_COUNT_HW_MAX + 1)
#define PERF_COUNT_ARC_ICM	(PERF_COUNT_HW_MAX + 2)
#define PERF_COUNT_ARC_BPOK	(PERF_COUNT_HW_MAX + 3)
#define PERF_COUNT_ARC_EDTLB	(PERF_COUNT_HW_MAX + 4)
#define PERF_COUNT_ARC_EITLB	(PERF_COUNT_HW_MAX + 5)
#define PERF_COUNT_ARC_LDC	(PERF_COUNT_HW_MAX + 6)
#define PERF_COUNT_ARC_STC	(PERF_COUNT_HW_MAX + 7)
#define PERF_COUNT_ARC_HW_MAX	(PERF_COUNT_HW_MAX + 8)

struct arc_pmu {
	struct pmu	pmu;
	unsigned int	irq:31, has_interrupts:1;
	int		n_counters;
	int		ev_hw_idx[PERF_COUNT_ARC_HW_MAX];
	int		raw_events_count;
	u64		max_period;
	u64		raw_events[ARC_PERF_MAX_EVENTS];
};

struct arc_pmu_cpu {
	/*
	 * The events that are active on the PMU for the given index.
	 */
	struct perf_event *events[PERF_COUNT_ARC_HW_MAX];

	/*
	 * A 1 bit for an index indicates that the counter is being used for
	 * an event. A 0 means that the counter can be used.
	 */
	unsigned long	used_mask[BITS_TO_LONGS(ARC_PERF_MAX_COUNTERS)];
};

#endif
