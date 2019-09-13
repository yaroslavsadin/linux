/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2004, 2007-2010, 2011-2012 Synopsys, Inc. (www.synopsys.com)
 */

/*
 * page table flags for software walked/managed MMUv3 (ARC700) and MMUv4 (HS)
 * There correspond to the corresponding bits in the TLB
 */

#ifndef _ASM_ARC_PGTABLE_BITS_ARCV2_H
#define _ASM_ARC_PGTABLE_BITS_ARCV2_H

#ifndef CONFIG_ARC_CACHE_PAGES
#define _PAGE_CACHEABLE		(1 << 0)  /* Cached (H) */
#else
#define _PAGE_CACHEABLE		0
#endif

#define _PAGE_EXECUTE		(1 << 1)  /* User Execute  (H) */
#define _PAGE_WRITE		(1 << 2)  /* User Write    (H) */
#define _PAGE_READ		(1 << 3)  /* User Read     (H) */
#define _PAGE_ACCESSED		(1 << 4)  /* Accessed      (s) */
#define _PAGE_DIRTY		(1 << 5)  /* Modified      (s) */
#define _PAGE_SPECIAL		(1 << 6)
#define _PAGE_GLOBAL		(1 << 8)  /* ASID agnostic (H) */
#define _PAGE_PRESENT		(1 << 9)  /* PTE/TLB Valid (H) */

#ifdef CONFIG_ARC_MMU_V4
#define _PAGE_HW_SZ		(1 << 10)  /* Normal/super (H) */
#else
#define _PAGE_HW_SZ		0
#endif

#endif
