// SPDX-License-Identifier: GPL-2.0-only

#include <linux/mm.h>
#include <linux/highmem.h>
#include <linux/syscalls.h>

#include <asm/cluster.h>
#include <asm/cacheflush.h>
#include <asm/setup.h>
#include <asm/event-log.h>
#include <asm/cachectl.h>

int l2_enable = IS_ENABLED(CONFIG_ARC_HAS_SCM);

static struct cpuinfo_arc_cache {
	unsigned int sz_k, line_len, colors;
} ic, dc, l2_info;

#define ARC_L2_CONFIGURED	(l2_info.sz_k && l2_enable)

void (*__dma_cache_wback_inv)(phys_addr_t start, unsigned long sz);
void (*__dma_cache_inv)(phys_addr_t start, unsigned long sz);
void (*__dma_cache_wback)(phys_addr_t start, unsigned long sz);

static int read_decode_cache_bcr_arcv3(int c, char *buf, int len)
{
	struct cpuinfo_arc_cache *p_l2 = &l2_info;
	struct bcr_clustv3_cfg cbcr;
	struct bcr_cln_0_cfg cln0;
	struct bcr_cln_scm_0_cfg scm0;
	int n = 0;

	READ_BCR(ARC_REG_CLUSTER_BCR, cbcr);
	if (cbcr.ver_maj == 0)
		return n;

	arc_cluster_mumbojumbo();

	READ_BCR(ARC_REG_CLNR_BCR_0, cln0);

	if (cln0.has_scm) {
		READ_BCR(ARC_REG_CLNR_SCM_BCR_0, scm0);

		p_l2->sz_k = (1 << scm0.data_bank_sz) * (1 << scm0.data_banks);
		/* Fixed to 64. */
		p_l2->line_len = 64;

		n += scnprintf(buf + n, len - n,
			       "L2\t\t: %uK, %uB Line%s\n",
			       p_l2->sz_k, p_l2->line_len,
			       IS_USED_RUN(l2_enable));
	}

	return n;
}

int arc_cache_mumbojumbo(int c, char *buf, int len)
{
	struct cpuinfo_arc_cache *p_ic = &ic, *p_dc = &dc;
	struct bcr_cache ibcr, dbcr;
	struct bcr_hw_pf_ctrl {
		unsigned int en:1, rd_st:2, wr_st:2, outs:2, ag:1, pad:24;
	} hwpf;
	int assoc, n = 0;

	READ_BCR(ARC_REG_IC_BCR, ibcr);
	if (!ibcr.ver)
		goto dc_chk;

	BUG_ON(ibcr.ver < 4);
	assoc = 1 << ibcr.config;	/* 1,2,4,8 */
	p_ic->line_len = 8 << ibcr.line_len;
	p_ic->sz_k = 1 << (ibcr.sz - 1);
	p_ic->colors = p_ic->sz_k/assoc/TO_KB(PAGE_SIZE);

	n += scnprintf(buf + n, len - n,
		       "I-Cache\t\t: %uK, %dway/set, %uB Line, VIPT%s%s\n",
		       p_ic->sz_k, assoc, p_ic->line_len,
		       p_ic->colors > 1 ? " aliasing" : "",
		       IS_USED_CFG(CONFIG_ARC_HAS_ICACHE));

dc_chk:
	READ_BCR(ARC_REG_DC_BCR, dbcr);
	if (!dbcr.ver)
		goto slc_chk;

	BUG_ON(dbcr.ver < 4);
	assoc = 1 << dbcr.config;	/* 1,2,4,8 */
	p_dc->line_len = 16 << dbcr.line_len;
	p_dc->sz_k = 1 << (dbcr.sz - 1);

	n += scnprintf(buf + n, len - n,
		       "D-Cache\t\t: %uK, %dway/set, %uB Line, PIPT%s\n",
		       p_dc->sz_k, assoc, p_dc->line_len,
		       IS_USED_CFG(CONFIG_ARC_HAS_DCACHE));

	if (dbcr.hwpf) {
		READ_BCR(ARC_REG_HW_PF_CTRL, hwpf);
		n += scnprintf(buf + n, len - n,
			       "HW PF\t\t: RD %d WR %d OUTS %d AG %d %s\n",
			       1 << hwpf.rd_st, 1 << hwpf.wr_st,
			       1 << hwpf.outs, hwpf.ag,
			       IS_DISABLED_RUN(hwpf.en));
	}

slc_chk:
	n += read_decode_cache_bcr_arcv3(c, buf + n, len - n);

	return n;
}

#define OP_INV		0x1
#define OP_FLUSH	0x2
#define OP_FLUSH_N_INV	0x3

static inline void __dc_op_before(const int op)
{
	const unsigned int ctl = ARC_REG_DC_CTRL;
	unsigned int val = read_aux_reg(ctl);

	if (op == OP_INV) {
		val &= ~DC_CTRL_INV_MODE_FLUSH;
	} else {
		val |= DC_CTRL_INV_MODE_FLUSH;
	}

	/*
	 *  op		DC_CTL.RGN_OP	DC_CTRL.IM
	 * Flush              0           n/a
	 * Inv                1            0
	 * Flush-n-Inv        1            1
	 */
	val &= ~DC_CTRL_RGN_OP_MSK;
	if (op & OP_INV)
		val |= DC_CTRL_RGN_OP_INV;

	// Set shareability attribute for DC operation: Inner-shareable.
	// The same as for a page shareability attributes: __SHR_INNER.
	val &= ~DC_CTRL_SH_ATTR_MASK;
	val |= DC_CTRL_SH_ATTR_INNER;

	write_aux_reg(ctl, val);
}

static inline void __dc_op_after(const int op)
{
	if (op & OP_FLUSH) {
		const unsigned int ctl = ARC_REG_DC_CTRL;
		unsigned int reg;

		/* flush / flush-n-inv both wait */
		while ((reg = read_aux_reg(ctl)) & DC_CTRL_FLUSH_STATUS)
			;

		/* Switch back to default Invalidate mode */
		if (op == OP_FLUSH)
			write_aux_reg(ctl, reg | DC_CTRL_INV_MODE_FLUSH);
	}
}

/* For kernel mappings cache operation: index is same as paddr */
#define __dc_line_op_k(p, sz, op)	__flush_dcache_range(p, p, sz, op)

// L1D$ region operations
static void __flush_dcache_range(phys_addr_t paddr, unsigned long vaddr, int len, const int op)
{
#ifdef CONFIG_ARC_HAS_DCACHE
	unsigned long end;
	unsigned long flags;

	local_irq_save(flags);

	end = paddr + len + L1_CACHE_BYTES - 1;

    __dc_op_before(op);
#if defined(CONFIG_64BIT)
        write_aux_64(ARC_REG_DC_ENDR, end);
        write_aux_64(ARC_REG_DC_STARTR, paddr);
#else
        write_aux_reg(ARC_REG_DC_ENDR, end);
        write_aux_reg(ARC_REG_DC_STARTR, paddr);
#endif
    __dc_op_after(op);
	local_irq_restore(flags);
#endif
}

// L2D$ region operations
#define scm_op(paddr, sz, op)	scm_op_rgn(paddr, sz, op)

noinline void scm_op_rgn(phys_addr_t paddr, unsigned long sz, const int op)
{
	/*
	 * SCM is shared between all cores and concurrent aux operations from
	 * multiple cores need to be serialized using a spinlock
	 * A concurrent operation can be silently ignored and/or the old/new
	 * operation can remain incomplete forever (lockup in SLC_CTRL_BUSY loop
	 * below)
	 */
	static DEFINE_SPINLOCK(lock);
	unsigned long flags;
	unsigned int cmd;
	phys_addr_t end;

	spin_lock_irqsave(&lock, flags);

	cmd = ARC_CLN_CACHE_CMD_INCR;
	if (op == OP_INV) {
		cmd |= ARC_CLN_CACHE_CMD_OP_ADDR_INV;
	} else if (op == OP_FLUSH) {
		cmd |= ARC_CLN_CACHE_CMD_OP_ADDR_CLN;
	} else {
		cmd |= ARC_CLN_CACHE_CMD_OP_ADDR_CLN_INV;
	}

	/*
	 * Lower bits are ignored, no need to clip
	 * END can't be same as START, so add (l2_info.line_len - 1) to sz
	 */
	end = paddr + sz + l2_info.line_len - 1;

	arc_cln_write_reg(ARC_CLN_CACHE_ADDR_LO0, (u32)paddr);
	arc_cln_write_reg(ARC_CLN_CACHE_ADDR_LO1, (u64)paddr >> 32ULL);

	arc_cln_write_reg(ARC_CLN_CACHE_ADDR_HI0, (u32)end);
	arc_cln_write_reg(ARC_CLN_CACHE_ADDR_HI1, (u64)end >> 32ULL);

	arc_cln_write_reg(ARC_CLN_CACHE_CMD, cmd);
	while (arc_cln_read_reg(ARC_CLN_CACHE_STATUS) & ARC_CLN_CACHE_STATUS_BUSY);

	spin_unlock_irqrestore(&lock, flags);
}

/*
 * DMA ops for systems with L1 cache only
 * Make memory coherent with L1 cache by flushing/invalidating L1 lines
 */
static void __dma_cache_wback_l1(phys_addr_t start, unsigned long sz)
{
	__flush_dcache_range(start, start, sz, OP_FLUSH);
}

static void __dma_cache_inv_l1(phys_addr_t start, unsigned long sz)
{
	__flush_dcache_range(start, start, sz, OP_INV);
}

static void __dma_cache_wback_inv_l1(phys_addr_t start, unsigned long sz)
{
	__flush_dcache_range(start, start, sz, OP_FLUSH_N_INV);
}

/*
 * DMA ops for systems with both L1 and L2 caches
 * Both L1 and L2 lines need to be explicitly flushed/invalidated
 */
static void __dma_cache_wback_scm(phys_addr_t start, unsigned long sz)
{
	__flush_dcache_range(start, start, sz, OP_FLUSH);
	scm_op(start, sz, OP_FLUSH);
}

static void __dma_cache_inv_scm(phys_addr_t start, unsigned long sz)
{
	__flush_dcache_range(start, start, sz, OP_INV);
	scm_op(start, sz, OP_INV);
}

static void __dma_cache_wback_inv_scm(phys_addr_t start, unsigned long sz)
{
	__flush_dcache_range(start, start, sz, OP_FLUSH_N_INV);
	scm_op(start, sz, OP_FLUSH_N_INV);
}

void __ref arc_cache_init(void)
{
	unsigned int __maybe_unused cpu = smp_processor_id();

	if(cpu != 0)
		return;

	if (ARC_L2_CONFIGURED) {
		arc_cluster_scm_enable();

		__dma_cache_wback = __dma_cache_wback_scm;
		__dma_cache_inv = __dma_cache_inv_scm;
		__dma_cache_wback_inv = __dma_cache_wback_inv_scm;
	} else {
		__dma_cache_wback = __dma_cache_wback_l1;
		__dma_cache_inv = __dma_cache_inv_l1;
		__dma_cache_wback_inv = __dma_cache_wback_inv_l1;
	}
}

/*
 * Note:
 *    - This function would never straddle the MMU page
 */
static void __inv_icache_range(phys_addr_t paddr, unsigned long vaddr, int len)
{
        unsigned long start, end;

        /*
         * I$ is VIPT.
         * For aliasing config, vaddr is needed to index and paddr to match tag
         * for non-aliasing paddr suffices for both
         */
        if (likely(ic.colors > 1)) {
#if defined(CONFIG_64BIT)
                write_aux_64(ARC_REG_IC_PTAG, paddr);
#else
                write_aux_reg(ARC_REG_IC_PTAG, paddr);
#endif
                start = vaddr;
        } else {
                start = paddr;
        }

        /*
         * ENDR is exclusive so needs to point past the end of the last line
         * being flushed. To do this unconditionally add line size - 1
         */
	end = start + len + L1_CACHE_BYTES - 1;

	/* ENDR needs to be set ahead of START */
#if defined(CONFIG_64BIT)
        write_aux_64(ARC_REG_IC_ENDR, end);
#else
        write_aux_reg(ARC_REG_IC_ENDR, end);
#endif

        start &= ~0x3;
#if defined(CONFIG_64BIT)
        write_aux_64(ARC_REG_IC_IVIR, start);
#else
        write_aux_reg(ARC_REG_IC_IVIR, start);
#endif
}

void __sync_icache_dcache(phys_addr_t paddr, unsigned long vaddr, int len)
{
	unsigned long flags;

	local_irq_save(flags);

        __flush_dcache_range(paddr, vaddr, len, OP_FLUSH_N_INV);
        __inv_icache_range(paddr, vaddr, len);

	local_irq_restore(flags);
}

#ifdef CONFIG_ARC_HAS_ICACHE
static inline void __ic_entire_inv(void)
{
	write_aux_reg(ARC_REG_IC_IVIC, 1);
	read_aux_reg(ARC_REG_IC_CTRL);	/* blocks */
}
#else
	#define __ic_entire_inv()
#endif

#ifdef CONFIG_ARC_HAS_DCACHE
static inline void __dc_entire_op(const int op)
{
	int aux;

	__dc_op_before(op);

	if (op & OP_INV)	/* Inv or flush-n-inv use same cmd reg */
		aux = ARC_REG_DC_IVDC;
	else
		aux = ARC_REG_DC_FLSH;

	write_aux_reg(aux, 0x1);

	__dc_op_after(op);
}
#else
	#define __dc_entire_op(op)
#endif

static void __dma_cache_wback_inv_all_scm(void)
{
	static DEFINE_SPINLOCK(lock);
	unsigned long flags;
	unsigned int vv;

	if(ARC_L2_CONFIGURED) {
		spin_lock_irqsave(&lock, flags);

		vv = arc_cln_read_reg(ARC_CLN_CACHE_STATUS);
		//arc_cln_write_reg(ARC_CLN_CACHE_STATUS, 0); // disable the L2 cache (if enabled)
		arc_cln_write_reg(ARC_CLN_CACHE_CMD, ARC_CLN_CACHE_CMD_OP_REG_CLN_INV |
			  ARC_CLN_CACHE_CMD_INCR); // flush + invalidate
		while (arc_cln_read_reg(ARC_CLN_CACHE_STATUS) &
	       ARC_CLN_CACHE_STATUS_BUSY)
			;
		//arc_cln_write_reg(ARC_CLN_CACHE_STATUS, vv & 0xFFFF0000); // enable the L2 cache

		spin_unlock_irqrestore(&lock, flags);
	}
}

noinline void flush_cache_all(void)
{
	unsigned long flags;

	local_irq_save(flags);
	__ic_entire_inv();					// L1I$
	__dc_entire_op(OP_FLUSH_N_INV);		// L1D$
	local_irq_restore(flags);
	__dma_cache_wback_inv_all_scm();		// L2$
}

#define cacheflush_pr pr_debug

int aaa; //delete me!!!!!!!!!!!!!!!

#if 1
#define SZ_BUF (1024)
static unsigned int big_buf[SZ_BUF];

uint32_t crc32_v1(const void *buf, size_t size) 
{
    const uint8_t *p = buf;
    uint32_t crc = ~0U;
	int i;

    while (size--) {
        crc ^= *p++;
        for (i = 0; i < 8; i++)
            crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
    }
    return ~crc;
}

void check_me(uint32_t start, uint32_t sz) /*__attribute__ ((noinline))*/
{
	int ii;
	int N = sz/4;
	unsigned int crc;
	int *pA, *pB;

	for(ii = N-1; ii >= 0; ii--) {
		pA = (int *)start + ii;
		pB = (int *)big_buf + ii;

		//*pB = *pA;
		*pB = arc_read_uncached_32(pA);
	}

	crc = crc32_v1(big_buf, sz-4);
	if(crc == big_buf[N-1]){
		printk("CRC ok. crc=%X", crc);
	}else{
		printk("CRC error. %X != %X", crc, big_buf[N-1]);
	}
}
#endif

static int cacheflush_user(uint32_t start, uint32_t sz, uint32_t flags, unsigned op)
{
	struct mm_struct * const mm = current->mm;
	struct vm_area_struct *vma;
	struct page *pages[1];
	unsigned long pfn;
	unsigned long phys;
	const unsigned start_page = start & PAGE_MASK;
	const unsigned end_page = (start + sz - 1) & PAGE_MASK;
	int n_pages = ((end_page - start_page) >> PAGE_SHIFT) + 1;

	//cacheflush_pr("[%d] %s:%d start=0x%x size=%u flags=0x%x pages=%u\n", aaa++, __func__, __LINE__,
		//start, sz, flags, n_pages);

	if (n_pages > 1) {
		return -EINVAL;
	}

	if (!mm) {
		cacheflush_pr("%s:%d No mm_struct\n", __func__, __LINE__);
		return -ENOMEM;
	}
	down_write(&mm->mmap_lock);
	vma = find_vma(mm, start);
	up_write(&mm->mmap_lock);
	if (!vma) {
		cacheflush_pr("%s:%d No vm_area_struct\n", __func__, __LINE__);
		return -ENOMEM;
	}
	//cacheflush_pr("%s:%d vma: start=0x%lx end=0x%lx flags=0x%lx\n", __func__, __LINE__,
		//vma->vm_start, vma->vm_end, vma->vm_flags);

	n_pages = get_user_pages_fast(start, 1, 0, pages);
	if (n_pages == 1) {
		pfn = page_to_pfn(pages[0]);
		put_page(pages[0]);
	} else if ((vma->vm_flags & VM_PFNMAP) && vma->vm_pgoff) {
		// Deal with mmaping from remap_pfn_range. See vm_normal_page doc
		pfn = vma->vm_pgoff + ((start - vma->vm_start) >> PAGE_SHIFT);
	} else {
		cacheflush_pr("%s:%d No page. ret=%d\n", __func__, __LINE__, n_pages);
		return -ENOMEM;
	}
	phys = (pfn << PAGE_SHIFT) + (start & ~PAGE_MASK);
	//cacheflush_pr("%s:%d page: pfn=0x%lx phys=0x%lx\n", __func__, __LINE__,
		//pfn, phys);

	if (flags & CF_D_L1) {
		//cacheflush_pr("%s:%d cache op-%u L1 page\n", __func__, __LINE__, op);
		// This code assums cache v4 that is not using the virtual address 'start'
		__dc_line_op_k(phys, sz, op);
	}
	if (flags & CF_D_L2) {
		//cacheflush_pr("%s:%d cache op-%u L2 range\n", __func__, __LINE__, op);
		scm_op(phys, sz, op);
	}
	check_me(start, sz);
	return 0;
}

static int cacheflush_phys(uint32_t start, uint32_t sz, uint32_t flags, unsigned op)
{
	const unsigned start_page = start & PAGE_MASK;
	const unsigned end_page = (start + sz - 1) & PAGE_MASK;
	int n_pages = ((end_page - start_page) >> PAGE_SHIFT) + 1;

	cacheflush_pr("[%d] %s:%d start=0x%x size=%u flags=0x%x pages=%u\n", aaa++, __func__, __LINE__,
		start, sz, flags, n_pages);

	if (flags & CF_D_L1) {
		cacheflush_pr("%s:%d cache op-%u L1 page\n", __func__, __LINE__, op);
		__dc_line_op_k(start, sz, op);
	}
	if (flags & CF_D_L2) {
		cacheflush_pr("%s:%d cache op-%u L2 range\n", __func__, __LINE__, op);
		scm_op(start, sz, op);
	}

	return 0;
}

static inline unsigned cacheflush_flag_to_line_op(unsigned flags)
{
	if ((flags & CF_D_FLUSH) && !(flags & CF_D_INV)) return OP_FLUSH;
	if (!(flags & CF_D_FLUSH) && (flags & CF_D_INV)) return OP_INV;
	return OP_FLUSH_N_INV;
}

// CF_D_NAVARRO_PHY  is always set
SYSCALL_DEFINE3(cacheflush, uint32_t, start, uint32_t, sz, uint32_t, flags)
{
	unsigned long irqflags;
	int err;
	const bool navarro_opt = (flags & (CF_D_L1 | CF_D_L2)) != 0;
	const bool is_phys = (flags & CF_D_PHY) != 0;
	unsigned op =  cacheflush_flag_to_line_op(flags);

	if (!navarro_opt) {
		err = cacheflush_user(start, sz, flags | CF_D_L1, OP_FLUSH_N_INV);
		if (err) {
			printk("--->11. err = %d", err);
			flush_cache_all();
		}else {
			printk("-~~>1 ok");
		}
		return 0;
	}
	//printk("--->2");

	err = is_phys ? cacheflush_phys(start, sz, flags, op)
		      : cacheflush_user(start, sz, flags, op);
	if (!err) {
		//printk("--->22 ok");
		return 0;
	}
	printk("--->3");
	local_irq_save(irqflags);

	if (flags & CF_D_L1) {
		printk("--->4");
		cacheflush_pr("%s:%d cache op-%u ALL L1\n", __func__, __LINE__, op);
		__dc_entire_op(op);
	}
	if (flags & CF_D_L2) {
		printk("--->5");
		cacheflush_pr("%s:%d cache op-%u ALL L2\n", __func__, __LINE__, op);
		__dma_cache_wback_inv_all_scm();
	}

	local_irq_restore(irqflags);

	return 0;
}

void dma_cache_wback_inv(phys_addr_t start, unsigned long sz)
{
	take_snap2(SNAP_CACHE_OP_WB_INV, start, sz);
	__dma_cache_wback_inv(start, sz);
}
EXPORT_SYMBOL(dma_cache_wback_inv);

void dma_cache_inv(phys_addr_t start, unsigned long sz)
{
	take_snap2(SNAP_CACHE_OP_INV, start, sz);
	__dma_cache_inv(start, sz);
}
EXPORT_SYMBOL(dma_cache_inv);

void dma_cache_wback(phys_addr_t start, unsigned long sz)
{
	take_snap2(SNAP_CACHE_OP_WB, start, sz);
	__dma_cache_wback(start, sz);
}
EXPORT_SYMBOL(dma_cache_wback);

void copy_user_highpage(struct page *to, struct page *from,
	unsigned long u_vaddr, struct vm_area_struct *vma)
{
	void *kfrom = kmap_atomic(from);
	void *kto = kmap_atomic(to);

	clear_bit(PG_dc_clean, &to->flags);
	clear_bit(PG_dc_clean, &from->flags);

	copy_page(kto, kfrom);

	kunmap_atomic(kto);
	kunmap_atomic(kfrom);
}

void clear_user_page(void *to, unsigned long u_vaddr, struct page *page)
{
	clear_page(to);
	clear_bit(PG_dc_clean, &page->flags);
}

void flush_dcache_page(struct page *page)
{
	clear_bit(PG_dc_clean, &page->flags);
}

void ptw_flush(void *xp)
{
#ifdef CONFIG_ARC_PTW_UNCACHED
#if defined(CONFIG_64BIT)
        write_aux_64(ARC_REG_DC_IVDL, __pa(xp));
#else
        write_aux_reg(ARC_REG_DC_IVDL, __pa(xp));
#endif
	if (ARC_L2_CONFIGURED)
		arc_cluster_scm_flush_range(__pa(xp), __pa(xp));
#endif
}

void set_pte_at(struct mm_struct *mm, unsigned long vaddr, pte_t *ptep, pte_t pte)
{
	if (pte_present(pte) && pte_exec(pte)) {

		struct page *page = pte_page(pte);
		int dirty;

		dirty = !test_and_set_bit(PG_dc_clean, &page->flags);
		if (dirty) {
			phys_addr_t paddr = pte_val(pte) & PAGE_MASK;
			__sync_icache_dcache(paddr, vaddr & PAGE_MASK, PAGE_SIZE);
		}
	}

	set_pte(ptep, pte);
}

/*
 * Make I/D Caches consistent for kernel code (modules, kprobes, kgdb)
 */
void flush_icache_range(unsigned long kvaddr, unsigned long kvend)
{
	unsigned int tot_sz = kvend - kvaddr;

	BUG_ON((kvaddr < VMALLOC_START) || (kvend > VMALLOC_END));

	while (tot_sz > 0) {
		unsigned int off, sz;
		unsigned long paddr, pfn;

		off = kvaddr % PAGE_SIZE;
		pfn = vmalloc_to_pfn((void *)kvaddr);
		paddr = (pfn << PAGE_SHIFT) + off;
		sz = min_t(unsigned int, tot_sz, PAGE_SIZE - off);
		__sync_icache_dcache(paddr, kvaddr, sz);
		kvaddr += sz;
		tot_sz -= sz;
	}
}
