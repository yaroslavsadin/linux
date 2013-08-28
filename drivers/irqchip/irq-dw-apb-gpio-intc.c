/*
 * Synopsys DW APB GPIO irqchip driver.
 *
 * Mischa Jonker <mjonker@synopsys.com>
 *
 * This is meant to be used for DW APB GPIO blocks that are used as
 * interrupt controller. 
 *
 * Based on DW APB ICTL driver by:
 *   Sebastian Hesselbarth <sebastian.hesselbarth@gmail.com>
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2.  This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#include <linux/io.h>
#include <linux/irq.h>
#include <linux/irqchip/chained_irq.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>

#include "irqchip.h"

#define APB_INT_ENABLE		0x30
#define APB_INT_MASK		0x34
#define APB_INT_TYPE		0x38
#define APB_INT_POLARITY	0x3C
#define APB_INT_FINALSTATUS	0x40

static void dw_apb_gpio_intc_handler(unsigned int irq, struct irq_desc *desc)
{
	struct irq_chip *chip = irq_get_chip(irq);
	struct irq_chip_generic *gc = irq_get_handler_data(irq);
	struct irq_domain *d = gc->private;
	u32 stat;

	chained_irq_enter(chip, desc);

	stat = readl_relaxed(gc->reg_base + APB_INT_FINALSTATUS);
	while (stat) {
		u32 hwirq = ffs(stat) - 1;
		generic_handle_irq(irq_find_mapping(d,
				    gc->irq_base + hwirq));
		stat &= ~(1 << hwirq);
	}

	chained_irq_exit(chip, desc);
}

static int __init dw_apb_gpio_intc_init(struct device_node *np,
					struct device_node *parent)
{
	unsigned int clr = IRQ_NOREQUEST | IRQ_NOPROBE | IRQ_NOAUTOEN;
	struct resource r;
	struct irq_domain *domain;
	struct irq_chip_generic *gc;
	void __iomem *iobase;
	int ret, nrirqs, irq, n;

	/* Map the parent interrupt for the chained handler */
	irq = irq_of_parse_and_map(np, 0);
	if (irq <= 0) {
		pr_err("%s: unable to parse irq\n", np->name);
		return -EINVAL;
	}

	ret = of_address_to_resource(np, 0, &r);
	if (ret) {
		pr_err("%s: unable to get resource\n", np->name);
		return ret;
	}

	if (!request_mem_region(r.start, resource_size(&r), np->name)) {
		pr_err("%s: unable to request mem region\n", np->name);
		return -ENOMEM;
	}

	iobase = ioremap(r.start, resource_size(&r));
	if (!iobase) {
		pr_err("%s: unable to map resource\n", np->name);
		return -ENOMEM;
	}

	/* mask and enable all interrupts */
	writel(~0, iobase + APB_INT_MASK);
	writel(~0, iobase + APB_INT_ENABLE);

	/* no support yet for edge interrupts, so set everything to level */
	writel( 0, iobase + APB_INT_TYPE);
	writel(~0, iobase + APB_INT_POLARITY);

	nrirqs = fls(readl(iobase + APB_INT_ENABLE));
	
	domain = irq_domain_add_linear(np, nrirqs,
				       &irq_generic_chip_ops, NULL);
	if (!domain) {
		pr_err("%s: unable to add irq domain\n", np->name);
		return -ENOMEM;
	}

	ret = irq_alloc_domain_generic_chips(domain, 32, 1,
					     np->name, handle_level_irq, clr, 0,
					     IRQ_GC_INIT_MASK_CACHE);
	if (ret) {
		pr_err("%s: unable to alloc irq domain gc\n", np->name);
		return ret;
	}

	gc = irq_get_domain_generic_chip(domain, 0);
	gc->private = domain;
	gc->reg_base = iobase;

	gc->chip_types[0].regs.mask = APB_INT_MASK;
	gc->chip_types[0].chip.irq_mask = irq_gc_mask_set_bit;
	gc->chip_types[0].chip.irq_unmask = irq_gc_mask_clr_bit;

	irq_set_handler_data(irq, gc);
	irq_set_chained_handler(irq, dw_apb_gpio_intc_handler);

	/* also map additional parent interrupts */
	n = 1;
	irq = irq_of_parse_and_map(np, n++);
	while (irq > 0) {
		irq_set_handler_data(irq, gc);
		irq_set_chained_handler(irq, dw_apb_gpio_intc_handler);
		irq = irq_of_parse_and_map(np, n++);
	}

	return 0;
}

IRQCHIP_DECLARE(dw_apb_gpio_intc,
		"snps,dw-apb-gpio-intc", dw_apb_gpio_intc_init);
