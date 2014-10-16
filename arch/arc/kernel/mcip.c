/*
 * ARC MCIP (MultiCore IP) support
 *
 * Copyright (C) 2013 Synopsys, Inc. (www.synopsys.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/smp.h>
#include <linux/irq.h>
#include <linux/spinlock.h>
#include <linux/irqchip.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include "../../drivers/irqchip/irqchip.h"

#define ARC_REG_MCIP_BCR	0x0d0
#define ARC_REG_MCIP_CMD	0x600
#define ARC_REG_MCIP_WDATA	0x601
#define ARC_REG_MCIP_READBACK	0x602

struct mcip_bcr {
#ifdef CONFIG_CPU_BIG_ENDIAN
#else
	unsigned int ver:8,
		     pad:1, ipi:1, sem:1, msg:1, pad2:1, dbg:1, grtc:1, iocoh:1,
		     num_cores:6, llm:1, idu:1, pad3:8;
#endif
};

struct mcip_cmd {
#ifdef CONFIG_CPU_BIG_ENDIAN
#else
	unsigned int cmd:8, param:16, pad:8;
#endif

#define CMD_INTRPT_GENERATE_IRQ		0x01
#define CMD_INTRPT_GENERATE_ACK		0x02
#define CMD_INTRPT_CHECK_SOURCE		0x04

#define CMD_DEBUG_SET_MASK		0x34
#define CMD_DEBUG_SET_SELECT		0x36

#define CMD_IDU_ENABLE			0x71
#define CMD_IDU_DISABLE			0x72
#define CMD_IDU_SET_MODE		0x74
#define CMD_IDU_SET_DEST		0x76
#define CMD_IDU_SET_MASK		0x7C

#define IDU_M_TRIG_LEVEL		0x0
#define IDU_M_TRIG_EDGE			0x1

#define IDU_M_DISTRI_RR			0x0
#define IDU_M_DISTRI_DEST		0x2
};

static int idu_detected;

/*
 * MCIP programming model
 *
 * - Simple commands write {cmd:8,param:16} to MCIP_CMD aux reg
 *   (param could be irq, common_irq, core_id ...)
 * - More involved commands setup MCIP_WDATA with cmd specific data
 *   before invoking the simple command
 */
static void inline __mcip_cmd(unsigned int cmd, unsigned int param)
{
	struct mcip_cmd buf;

	buf.pad = 0;
	buf.cmd = cmd;
	buf.param = param;

	WRITE_AUX(ARC_REG_MCIP_CMD, buf);
}

static DEFINE_RAW_SPINLOCK(mcip_lock);

/*
 * Setup additional data for a cmd
 * Callers need to lock to ensure atomicity
 */
static void inline __mcip_cmd_data(unsigned int cmd, unsigned int param,
				   unsigned int data)
{
	write_aux_reg(ARC_REG_MCIP_WDATA, data);

	__mcip_cmd(cmd, param);
}

static char smp_cpuinfo_buf[128];

/*
 * Any SMP specific init any CPU does when it comes up.
 * Here we setup the CPU to enable Inter-Processor-Interrupts
 * Called for each CPU
 * -Master      : init_IRQ()
 * -Other(s)    : start_kernel_secondary()
 */
void mcip_init_smp(unsigned int cpu)
{
	smp_ipi_irq_setup(cpu, IPI_IRQ);
}

static void mcip_ipi_send(int cpu)
{
	unsigned long flags;

	raw_spin_lock_irqsave(&mcip_lock, flags);
	__mcip_cmd(CMD_INTRPT_GENERATE_IRQ, cpu);
	raw_spin_unlock_irqrestore(&mcip_lock, flags);
}

static void mcip_ipi_clear(int irq)
{
	unsigned int cpu;
	unsigned long flags;

	raw_spin_lock_irqsave(&mcip_lock, flags);

	/* Who sent the IPI */
	__mcip_cmd(CMD_INTRPT_CHECK_SOURCE, 0);

	cpu = read_aux_reg(ARC_REG_MCIP_READBACK);	/* 1,2,4,8... */

	__mcip_cmd(CMD_INTRPT_GENERATE_ACK, __ffs(cpu)); /* 0,1,2,3... */

	raw_spin_unlock_irqrestore(&mcip_lock, flags);
}

volatile int wake_flag;

static void mcip_wakeup_cpu(int cpu, unsigned long pc)
{
	BUG_ON(cpu == 0);
	wake_flag = cpu;
}

void arc_platform_smp_wait_to_boot(int cpu)
{
	while (wake_flag != cpu);
	wake_flag = 0;
	__asm__ __volatile__("j @first_lines_of_secondary	\n");
}

void mcip_init_early_smp(void)
{
#define IS_AVAIL1(var, str)    ((var) ? str : "")

	struct mcip_bcr mp;

	READ_BCR(ARC_REG_MCIP_BCR, mp);

	sprintf(smp_cpuinfo_buf, "Extn [SMP]\t: MCIP (v%d): %d cores with %s%s%s%s\n",
		mp.ver, mp.num_cores,
		IS_AVAIL1(mp.ipi, "IPI "),
		IS_AVAIL1(mp.idu, "IDU "),
		IS_AVAIL1(mp.dbg, "DEBUG "),
		IS_AVAIL1(mp.grtc, "Glb RTC"));

	plat_smp_ops.info = smp_cpuinfo_buf;

	plat_smp_ops.cpu_kick = mcip_wakeup_cpu;
	plat_smp_ops.ipi_send = mcip_ipi_send;
	plat_smp_ops.ipi_clear = mcip_ipi_clear;

	idu_detected = mp.idu;

	if (mp.dbg) {
		__mcip_cmd_data(CMD_DEBUG_SET_SELECT, 0, 0xf);
		__mcip_cmd_data(CMD_DEBUG_SET_MASK, 0xf, 0xf);
	}
}

/***************************************************************************
 * ARCv2 Interrupt Distribution Unit (IDU)
 *
 * Connects external "COMMON" IRQs to core intc, providing:
 *  -dynamic routing (IRQ affinity)
 *  -load balancing (Round Robin interrupt distribution)
 *  -1:N distribution
 *
 * It physically resides in the MCIP hw block
 */

/*
 * Set the DEST for @cmn_irq to @cpu_mask (1 bit per core)
 */
void noinline idu_set_dest(unsigned int cmn_irq, unsigned int cpu_mask)
{
	__mcip_cmd_data(CMD_IDU_SET_DEST, cmn_irq, cpu_mask);
}

void noinline idu_set_mode(unsigned int cmn_irq, unsigned int lvl,
			   unsigned int distr)
{
	union {
		unsigned int word;
		struct {
			unsigned int distr:2, pad:2, lvl:1, pad2:27;
		};
	} data;

	data.distr = distr;
	data.lvl = lvl;
	__mcip_cmd_data(CMD_IDU_SET_MODE, cmn_irq, data.word);
}

static inline unsigned int mcip_idu_irq(struct irq_data *d)
{
	BUG_ON(d==NULL);
	return d->hwirq;
}

static void idu_irq_mask(struct irq_data *data)
{
	unsigned long flags;
	unsigned long irq = mcip_idu_irq(data);

	raw_spin_lock_irqsave(&mcip_lock, flags);
	__mcip_cmd_data(CMD_IDU_SET_MASK, irq, 1);
	raw_spin_unlock_irqrestore(&mcip_lock, flags);
}

static void idu_irq_unmask(struct irq_data *data)
{
	unsigned long flags;
	unsigned long irq = mcip_idu_irq(data);

	raw_spin_lock_irqsave(&mcip_lock, flags);
	__mcip_cmd_data(CMD_IDU_SET_MASK, irq, 0);
	raw_spin_unlock_irqrestore(&mcip_lock, flags);
}

int idu_irq_set_affinity(struct irq_data *d, const struct cpumask *cpumask,
			    bool force)
{
	return IRQ_SET_MASK_OK;
}

static struct irq_chip idu_irq_chip = {
	.name			= "MCIP IDU Intc",
	.irq_mask		= idu_irq_mask,
	.irq_unmask		= idu_irq_unmask,
#ifdef CONFIG_SMP
	.irq_set_affinity       = idu_irq_set_affinity,
#endif

};

void idu_cascade_isr(unsigned int core_irq, struct irq_desc *desc)
{
	struct irq_domain *domain = irq_desc_get_handler_data(desc);
	unsigned int idu_irq;

	idu_irq = core_irq - 24;
	generic_handle_irq(irq_find_mapping(domain, idu_irq));
}

int idu_irq_map(struct irq_domain *d, unsigned int virq, irq_hw_number_t hwirq)
{
	irq_set_chip_and_handler(virq, &idu_irq_chip, handle_level_irq);
	irq_set_status_flags(virq, IRQ_MOVE_PCNTXT);

	return 0;
}

int idu_irq_xlate(struct irq_domain *d, struct device_node *n,
			 const u32 *intspec, unsigned int intsize,
			 irq_hw_number_t *out_hwirq, unsigned int *out_type)
{
	irq_hw_number_t hwirq = *out_hwirq = intspec[0];
	int distri = intspec[1];
	unsigned long flags;

	*out_type = IRQ_TYPE_NONE;

	/* XXX: validate distribution scheme again online cpu mask */
	if (distri == 0) {
		/* 0 - Round Robin to all cpus (in DEST ?) */
		raw_spin_lock_irqsave(&mcip_lock, flags);
		idu_set_dest(hwirq, BIT(NR_CPUS) - 1); /* 1 bit per core */
		idu_set_mode(hwirq, IDU_M_TRIG_LEVEL, IDU_M_DISTRI_RR);
		raw_spin_unlock_irqrestore(&mcip_lock, flags);
	} else {
		/*
		 * DEST based distribution for Level Triggered intr can only
		 * have 1 CPU, so generalize it to always contain 1 cpu
		 */
		int cpu = ffs(distri);
		if (cpu != fls(distri))
			pr_warn("IDU irq %lx distri mode set to cpu %x\n",
				hwirq, cpu);

		raw_spin_lock_irqsave(&mcip_lock, flags);
		idu_set_dest(hwirq, cpu);
		idu_set_mode(hwirq, IDU_M_TRIG_LEVEL, IDU_M_DISTRI_DEST);
		raw_spin_unlock_irqrestore(&mcip_lock, flags);
	}

	return 0;
}

static const struct irq_domain_ops idu_irq_ops = {
	.xlate	= idu_irq_xlate,
	.map	= idu_irq_map,
};

/*
 * [16, 23]: Statically assigned always private-per-core (Timers, WDT, IPI)
 * [24, 23+C]: If C > 0 then "C" common IRQs
 * [24+C, N]: Not statically assigned, private-per-core
 */


int __init idu_of_init(struct device_node *intc, struct device_node *parent)
{
	struct irq_domain *domain;
	/* Read IDU BCR to confirm nr_irqs */
	int nr_irqs = of_irq_count(intc);
	int i, irq;

	if (!idu_detected)
		panic("IDU not detected, but DeviceTree using it");

	pr_info("MCIP: IDU referenced from Devicetree %d irqs\n", nr_irqs);

	domain = irq_domain_add_linear(intc, nr_irqs, &idu_irq_ops, NULL);

	/* Parent interrupts (core-intc) are already mapped */

	for (i=0; i <nr_irqs; i++) {
		/* this step has been done before already
		 * however we need it to get the parent virq and set IDU handler
		 * as first level isr
		 */
		irq = irq_of_parse_and_map(intc, i);
		irq_set_handler_data(irq, domain);
		irq_set_chained_handler(irq, idu_cascade_isr);
	}

	__mcip_cmd(CMD_IDU_ENABLE, 0);

	return 0;
}
IRQCHIP_DECLARE(arcv2_idu_intc, "snps,arcv2-idu-intc", idu_of_init);
