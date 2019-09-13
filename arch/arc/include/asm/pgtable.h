/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2004, 2007-2010, 2011-2012 Synopsys, Inc. (www.synopsys.com)
 */

#ifndef _ASM_ARC_PGTABLE_H
#define _ASM_ARC_PGTABLE_H

#include <linux/bits.h>

#include <asm/pgtable-levels.h>
#include <asm/pgtable-bits-arcv2.h>

#include <asm/page.h>
#include <asm/mmu.h>

/* Set of bits not changed in pte_modify */
#define _PAGE_CHG_MASK	(PAGE_MASK | _PAGE_ACCESSED | _PAGE_DIRTY | _PAGE_SPECIAL)

/* Abbrevaited helpers */
#define ___DEF		(_PAGE_PRESENT | _PAGE_CACHEABLE)

#define PAGE_U_NONE     __pgprot(___DEF)
#define PAGE_U_R        __pgprot(___DEF | _PAGE_READ)
#define PAGE_U_W_R      __pgprot(___DEF | _PAGE_READ | _PAGE_WRITE)
#define PAGE_U_X_R      __pgprot(___DEF | _PAGE_READ | _PAGE_EXECUTE)
#define PAGE_U_X_W_R    __pgprot(___DEF \
				| _PAGE_READ | _PAGE_WRITE | _PAGE_EXECUTE)
#define PAGE_KERNEL     __pgprot(___DEF | _PAGE_GLOBAL \
				| _PAGE_READ | _PAGE_WRITE | _PAGE_EXECUTE)

#define PAGE_SHARED	PAGE_U_W_R

#define pgprot_noncached(prot)	(__pgprot(pgprot_val(prot) & ~_PAGE_CACHEABLE))

/*
 * Mapping of vm_flags (Generic VM) to PTE flags (arch specific)
 *
 * Certain cases have 1:1 mapping
 *  e.g. __P101 means VM_READ, VM_EXEC and !VM_SHARED
 *       which directly corresponds to  PAGE_U_X_R
 *
 * Other rules which cause the divergence from 1:1 mapping
 *
 *  1. Although ARC700 can do exclusive execute/write protection (meaning R
 *     can be tracked independet of X/W unlike some other CPUs), still to
 *     keep things consistent with other archs:
 *      -Write implies Read:   W => R
 *      -Execute implies Read: X => R
 *
 *  2. Pvt Writable doesn't have Write Enabled initially: Pvt-W => !W
 *     This is to enable COW mechanism
 */
	/* xwr */
#define __P000  PAGE_U_NONE
#define __P001  PAGE_U_R
#define __P010  PAGE_U_R	/* Pvt-W => !W */
#define __P011  PAGE_U_R	/* Pvt-W => !W */
#define __P100  PAGE_U_X_R	/* X => R */
#define __P101  PAGE_U_X_R
#define __P110  PAGE_U_X_R	/* Pvt-W => !W and X => R */
#define __P111  PAGE_U_X_R	/* Pvt-W => !W */

#define __S000  PAGE_U_NONE
#define __S001  PAGE_U_R
#define __S010  PAGE_U_W_R	/* W => R */
#define __S011  PAGE_U_W_R
#define __S100  PAGE_U_X_R	/* X => R */
#define __S101  PAGE_U_X_R
#define __S110  PAGE_U_X_W_R	/* X => R */
#define __S111  PAGE_U_X_W_R


/*
 * Number of entries a user land program use.
 * TASK_SIZE is the maximum vaddr that can be used by a userland program.
 */
#define	USER_PTRS_PER_PGD	(TASK_SIZE / PGDIR_SIZE)

/*
 * No special requirements for lowest virtual address we permit any user space
 * mapping to be mapped at.
 */
#define FIRST_USER_ADDRESS      0UL

#ifndef __ASSEMBLY__

extern char empty_zero_page[PAGE_SIZE];
#define ZERO_PAGE(vaddr)	(virt_to_page(empty_zero_page))
extern pgd_t swapper_pg_dir[] __aligned(PAGE_SIZE);

/* Zoo of pte_xxx function */
#define pte_read(pte)		(pte_val(pte) & _PAGE_READ)
#define pte_write(pte)		(pte_val(pte) & _PAGE_WRITE)
#define pte_dirty(pte)		(pte_val(pte) & _PAGE_DIRTY)
#define pte_young(pte)		(pte_val(pte) & _PAGE_ACCESSED)
#define pte_special(pte)	(pte_val(pte) & _PAGE_SPECIAL)

#define PTE_BIT_FUNC(fn, op) \
	static inline pte_t pte_##fn(pte_t pte) { pte_val(pte) op; return pte; }

PTE_BIT_FUNC(mknotpresent,	&= ~(_PAGE_PRESENT));
PTE_BIT_FUNC(wrprotect,	&= ~(_PAGE_WRITE));
PTE_BIT_FUNC(mkwrite,	|= (_PAGE_WRITE));
PTE_BIT_FUNC(mkclean,	&= ~(_PAGE_DIRTY));
PTE_BIT_FUNC(mkdirty,	|= (_PAGE_DIRTY));
PTE_BIT_FUNC(mkold,	&= ~(_PAGE_ACCESSED));
PTE_BIT_FUNC(mkyoung,	|= (_PAGE_ACCESSED));
PTE_BIT_FUNC(exprotect,	&= ~(_PAGE_EXECUTE));
PTE_BIT_FUNC(mkexec,	|= (_PAGE_EXECUTE));
PTE_BIT_FUNC(mkspecial,	|= (_PAGE_SPECIAL));
PTE_BIT_FUNC(mkhuge,	|= (_PAGE_HW_SZ));

static inline pte_t pte_modify(pte_t pte, pgprot_t newprot)
{
	return __pte((pte_val(pte) & _PAGE_CHG_MASK) | pgprot_val(newprot));
}

static inline void set_pte_at(struct mm_struct *mm, unsigned long addr,
			      pte_t *ptep, pte_t pteval)
{
	set_pte(ptep, pteval);
}

void update_mmu_cache(struct vm_area_struct *vma, unsigned long address,
		      pte_t *ptep);

/* Encode swap {type,off} tuple into PTE
 * We reserve 13 bits for 5-bit @type, keeping bits 12-5 zero, ensuring that
 * PAGE_PRESENT is zero in a PTE holding swap "identifier"
 */
#define __swp_entry(type, off)		((swp_entry_t) \
					{ ((type) & 0x1f) | ((off) << 13) })

/* Decode a PTE containing swap "identifier "into constituents */
#define __swp_type(pte_lookalike)	(((pte_lookalike).val) & 0x1f)
#define __swp_offset(pte_lookalike)	((pte_lookalike).val >> 13)

#define __pte_to_swp_entry(pte)		((swp_entry_t) { pte_val(pte) })
#define __swp_entry_to_pte(x)		((pte_t) { (x).val })

#define kern_addr_valid(addr)	(1)

#ifdef CONFIG_TRANSPARENT_HUGEPAGE
#include <asm/hugepage.h>
#endif

#include <asm-generic/pgtable.h>

/* to cope with aliasing VIPT cache */
#define HAVE_ARCH_UNMAPPED_AREA

#endif /* __ASSEMBLY__ */

#endif
