#ifndef _ASM_METAG_IO_H
#define _ASM_METAG_IO_H

#ifdef __KERNEL__

#define IO_SPACE_LIMIT  0xFFFFFFFF

#define virt_to_bus virt_to_phys
#define bus_to_virt phys_to_virt
#define page_to_bus page_to_phys
#define bus_to_page phys_to_page

#include <asm-generic/io.h>

/*
 * Generic I/O
 */

/*
 * Despite being a 32bit architecture, Meta can do 64bit memory accesses
 * (assuming the bus supports it).
 */

static inline u64 __raw_readq(const volatile void __iomem *addr)
{
	return *(const volatile u64 __force *) addr;
}
#define readq(addr) __raw_readq(addr)

static inline void __raw_writeq(u64 b, volatile void __iomem *addr)
{
	*(volatile u64 __force *) addr = b;
}
#define writeq(b, addr) __raw_writeq(b, addr)

/*
 * A load of the architecture code uses read/write functions with raw physical
 * address numbers rather than __iomem pointers. Until these are fixed, do the
 * cast here to hide the warnings.
 */

#undef readb
#undef readw
#undef readl
#undef readq
#define readb(addr)	__raw_readb((volatile void __iomem *)(addr))
#define readw(addr)	__raw_readw((volatile void __iomem *)(addr))
#define readl(addr)	__raw_readl((volatile void __iomem *)(addr))
#define readq(addr)	__raw_readq((volatile void __iomem *)(addr))

#undef writeb
#undef writew
#undef writel
#undef writeq
#define writeb(b, addr)	__raw_writeb(b, (volatile void __iomem *)(addr))
#define writew(b, addr)	__raw_writew(b, (volatile void __iomem *)(addr))
#define writel(b, addr)	__raw_writel(b, (volatile void __iomem *)(addr))
#define writeq(b, addr)	__raw_writeq(b, (volatile void __iomem *)(addr))

/*
 * io remapping functions
 */

extern void __iomem *__ioremap(unsigned long offset,
			       size_t size, unsigned long flags);
extern void __iounmap(void __iomem *addr);

/**
 *	ioremap		-	map bus memory into CPU space
 *	@offset:	bus address of the memory
 *	@size:		size of the resource to map
 *
 *	ioremap performs a platform specific sequence of operations to
 *	make bus memory CPU accessible via the readb/readw/readl/writeb/
 *	writew/writel functions and the other mmio helpers. The returned
 *	address is not guaranteed to be usable directly as a virtual
 *	address.
 */
#define ioremap(offset, size)                   \
	__ioremap((offset), (size), 0)

#define ioremap_nocache(offset, size)           \
	__ioremap((offset), (size), 0)

#define ioremap_cached(offset, size)            \
	__ioremap((offset), (size), _PAGE_CACHEABLE)

#define ioremap_wc(offset, size)                \
	__ioremap((offset), (size), _PAGE_WR_COMBINE)

#define iounmap(addr)                           \
	__iounmap(addr)

#endif  /* __KERNEL__ */

#endif  /* _ASM_METAG_IO_H */
