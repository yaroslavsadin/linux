/*
 * Copyright (C) 2014 Synopsys, Inc. (www.synopsys.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/irqdomain.h>
#include <linux/irqchip.h>
#include "../../drivers/irqchip/irqchip.h"
#include <asm/sections.h>
#include <asm/irq.h>
#include <asm/mach_desc.h>

/*
 * Early Hardware specific Interrupt setup
 * -Called very early (start_kernel -> setup_arch -> setup_processor)
 * -Platform Independent (must for any ARC Core)
 * -Needed for each CPU (hence not foldable into init_IRQ)
 */
void arc_init_IRQ(void)
{
	unsigned int tmp;

	struct aux_irq_ctrl {
#ifdef CONFIG_CPU_BIG_ENDIAN
		unsigned int res3:18, save_idx_regs:1, res2:1,
			     save_u_to_u:1, save_lp_regs:1, save_blink:1,
			     res:4, save_nr_gpr_pairs:5;
#else
		unsigned int save_nr_gpr_pairs:5, res:4,
			     save_blink:1, save_lp_regs:1, save_u_to_u:1,
			     res2:1, save_idx_regs:1, res3:18;
#endif
	} ictrl;

	*(unsigned int *)&ictrl = 0;

	ictrl.save_nr_gpr_pairs = 6;	/* r0 to r11 (r12 saved manually) */
	ictrl.save_blink = 1;
	ictrl.save_lp_regs = 1;		/* LP_COUNT, LP_START, LP_END */
	ictrl.save_u_to_u = 0;		/* user ctxt saved on kernel stack */
	ictrl.save_idx_regs = 1;	/* JLI, LDI, EI */

	WRITE_AUX(AUX_IRQ_CTRL, ictrl);

	/* setup status32, don't enable intr yet as kernel doesn't want */
	tmp = read_aux_reg(0xa);
	tmp |= ISA_INIT_STATUS_BITS;
	tmp &= ~STATUS_IE_MASK;
	asm volatile("flag %0	\n"::"r"(tmp));
}

static void arcv2_irq_mask(struct irq_data *data)
{
	write_aux_reg(AUX_IRQ_SELECT, data->irq);
	write_aux_reg(AUX_IRQ_ENABLE, 0);
}

static void arcv2_irq_unmask(struct irq_data *data)
{
	write_aux_reg(AUX_IRQ_SELECT, data->irq);
	write_aux_reg(AUX_IRQ_ENABLE, 1);
}

void arcv2_irq_enable(struct irq_data *data)
{
	/* XXX: OSCI LAN needs explicit IRQ_ENABLE */
/* #if ARCV2_IRQ_DEF_PRIO != 0 */
	/* set default priority */
	write_aux_reg(AUX_IRQ_SELECT, data->irq);
	write_aux_reg(AUX_IRQ_PRIORITY, ARCV2_IRQ_DEF_PRIO);

	/*
	 * hw auto enables (linux unmask) all by default
	 * So no need to do IRQ_ENABLE here
	 */
/* #endif */
	write_aux_reg(AUX_IRQ_ENABLE, 1);
}

static struct irq_chip arcv2_irq_chip = {
	.name           = "ARCv2 core Intc",
	.irq_mask	= arcv2_irq_mask,
	.irq_unmask	= arcv2_irq_unmask,
	.irq_enable	= arcv2_irq_enable
};

static int arcv2_irq_map(struct irq_domain *d, unsigned int irq,
			 irq_hw_number_t hw)
{
	/* For percpu devices, more efficient handler can be used */
	//desc = irq_to_desc(irq);
	//if (irqd_is_per_cpu(&desc->irq_data) &&

	if (irq == TIMER0_IRQ || irq == IPI_IRQ)
		irq_set_chip_and_handler(irq, &arcv2_irq_chip, handle_percpu_irq);
	else
		irq_set_chip_and_handler(irq, &arcv2_irq_chip, handle_level_irq);

	return 0;
}

static const struct irq_domain_ops arcv2_irq_ops = {
	.xlate = irq_domain_xlate_onecell,
	.map = arcv2_irq_map,
};

static struct irq_domain *root_domain;

static int __init
init_onchip_IRQ(struct device_node *intc, struct device_node *parent)
{
	if (parent)
		panic("DeviceTree incore intc not a root irq controller\n");

	root_domain = irq_domain_add_legacy(intc, NR_CPU_IRQS, 0, 0,
					    &arcv2_irq_ops, NULL);

	if (!root_domain)
		panic("root irq domain not avail\n");

	/* with this we don't need to export root_domain */
	irq_set_default_host(root_domain);

	return 0;
}

IRQCHIP_DECLARE(arc_intc, "snps,arcv2-intc", init_onchip_IRQ);

/*
 * Late Interrupt system init called from start_kernel for Boot CPU only
 *
 * Since slab must already be initialized, platforms can start doing any
 * needed request_irq( )s
 */
void __init init_IRQ(void)
{
	/* Any external intc can be setup here */
	if (machine_desc->init_irq)
		machine_desc->init_irq();

	/* process the entire interrupt tree in one go */
	irqchip_init();

#ifdef CONFIG_SMP
	/* Master CPU can initialize it's side of IPI */
	if (machine_desc->init_smp)
		machine_desc->init_smp(smp_processor_id());
#endif
}

/*
 * "C" Entry point for any ARC ISR, called from low level vector handler
 * @irq is the vector number read from ICAUSE reg of on-chip intc
 */
void arch_do_IRQ(unsigned int irq, struct pt_regs *regs)
{
	struct pt_regs *old_regs = set_irq_regs(regs);

	irq_enter();
	generic_handle_irq(irq);
	irq_exit();
	set_irq_regs(old_regs);
}

void arc_request_percpu_irq(int irq, int cpu,
                            irqreturn_t (*isr)(int irq, void *dev),
                            const char *irq_nm,
                            void *percpu_dev)
{
	if (!cpu) {
		int rc;

		/* These 2 calls are essential to making percpu IRQ APIs work */
		irq_set_percpu_devid(irq);
		irq_modify_status(irq, IRQ_NOAUTOEN, 0);  /* @irq, @clr, @set */

		rc = request_percpu_irq(irq, isr, irq_nm, percpu_dev);
		if (rc)
			panic("Percpu IRQ request failed for %d\n", irq);
	}

	enable_percpu_irq(irq, 0);
}
