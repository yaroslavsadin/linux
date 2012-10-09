/*
 * Meta internal (HWSTATMETA) interrupt code.
 *
 * Copyright (C) 2011 Imagination Technologies Ltd.
 *
 * This code is based on the code in SoC/common/irq.c and SoC/comet/irq.c
 * The code base could be generalised/merged as a lot of the functionality is
 * similar. Until this is done, we try to keep the code simple here.
 */

#include <linux/interrupt.h>
#include <linux/io.h>

#include <asm/irq.h>
#include <asm/hwthread.h>

#define PERF0VECINT		0x04820580
#define PERF1VECINT		0x04820588
#define PERF0TRIG_OFFSET	16
#define PERF1TRIG_OFFSET	17

static unsigned int mapped; /* bit mask of mapped triggers */

static unsigned int metag_internal_irq_startup_edge(struct irq_data *data);
static void metag_internal_irq_shutdown(struct irq_data *data);
static void metag_internal_irq_ack(struct irq_data *data);
#ifdef CONFIG_SMP
static int metag_internal_irq_set_affinity(struct irq_data *data,
			const struct cpumask *cpumask, bool force);
#endif

static struct irq_chip internal_irq_edge_chip = {
	.name = "HWSTATMETA-IRQ",
	.irq_startup = metag_internal_irq_startup_edge,
	.irq_shutdown = metag_internal_irq_shutdown,
	.irq_ack = metag_internal_irq_ack,
#ifdef CONFIG_SMP
	.irq_set_affinity = metag_internal_irq_set_affinity,
#endif
};

/*
 *	metag_hwvec_addr - get the address of *VECINT regs of irq
 *
 *	This function is a table of supported triggers on HWSTATMETA
 *	Could do with a structure, but better keep it simple. Changes
 *	in this code should be rare.
 */
static inline void __iomem *metag_hwvec_addr(unsigned int irq)
{
	void __iomem *addr;
	unsigned int offset = IRQ_TO_OFFSET(irq);

	switch (offset) {
	case PERF0TRIG_OFFSET:
		addr = (void __iomem *)PERF0VECINT;
		break;
	case PERF1TRIG_OFFSET:
		addr = (void __iomem *)PERF1VECINT;
		break;
	default:
		addr = NULL;
		break;
	}
	return addr;
}

/*
 *	metag_internal_startup_edge_irq - setup an edge-triggered irq
 *	@irq:	the irq to startup
 *
 *	Multiplex interrupts for @irq onto TR1. Clear any pending
 *	interrupts.
 */
static unsigned int metag_internal_irq_startup_edge(struct irq_data *data)
{
	unsigned int irq = data->irq;
	int thread = hard_processor_id();

	/* Enable the interrupt by vectoring it */
	writel(TBI_TRIG_VEC(TBID_SIGNUM_TR1(thread)), metag_hwvec_addr(irq));

	/* Clear (toggle) the bit in HWSTATx for our interrupt. */
	metag_internal_irq_ack(data);

	return 0;
}

/*
 *	metag_internal_irq_shutdown - turn off the irq
 *	@irq:	the irq number to turn off
 *
 *	Mask @irq and clear any pending interrupts.
 *	Stop muxing @irq onto TR1.
 */
static void metag_internal_irq_shutdown(struct irq_data *data)
{
	unsigned int irq = data->irq;
	/*
	 * Disable the IRQ at the core by removing the interrupt from
	 * the HW vector mapping.
	 */
	writel(0, metag_hwvec_addr(irq));

	/* Clear (toggle) the bit in HWSTATx for our interrupt. */
	metag_internal_irq_ack(data);
}

/*
 *	metag_internal_irq_ack - acknowledge irq
 *	@irq:	the irq to ack
 */
static void metag_internal_irq_ack(struct irq_data *data)
{
	unsigned int irq = data->irq;
	unsigned int offset = IRQ_TO_OFFSET(irq);
	if (readl(HWSTATMETA) & (1 << offset))
		writel(1 << offset, HWSTATMETA);
}

/*
 * metag_internal_irq_status - returns the status of the mapped triggers
 *
 */

static inline u32 metag_internal_irq_status(void)
{
	return readl(HWSTATMETA) & mapped;
}

#ifdef CONFIG_SMP
/*
 *	metag_internal_irq_set_affinity - set the affinity for an interrupt
 */
static int metag_internal_irq_set_affinity(struct irq_data *data,
			const struct cpumask *cpumask, bool force)
{
	unsigned int cpu, thread;
	unsigned int irq = data->irq;
	/*
	 * Wire up this interrupt from *VECINT to the Meta core.
	 *
	 * Note that we can't wire up *VECINT to interrupt more than
	 * one cpu (the interrupt code doesn't support it), so we just
	 * pick the first cpu we find in 'cpumask'.
	 */
	cpu = cpumask_any(cpumask);
	thread = cpu_2_hwthread_id[cpu];

	writel(TBI_TRIG_VEC(TBID_SIGNUM_TR1(thread)), metag_hwvec_addr(irq));

	return 0;
}
#endif

/*
 *	metag_internal_irq_demux - irq de-multiplexer
 *	@irq:	the interrupt number
 *	@desc:	the interrupt description structure for this irq (unused)
 *
 *	The cpu receives an interrupt on TR1 when an interrupt has
 *	occurred. It is this function's job to demux this irq and
 *	figure out exactly which trigger needs servicing.
 */
static void metag_internal_irq_demux(unsigned int irq, struct irq_desc *desc)
{
	unsigned int irq_no;
	u32 status;

recalculate:
	status = metag_internal_irq_status();

	irq_no = HWSTATMETA_TO_IRQ(0);
	while (status != 0) {
		if (status & 0x1) {
			/*
			 * Only fire off interrupts that are
			 * registered to be handled by the kernel.
			 * Other interrupts are probably being
			 * handled by other Meta hardware threads.
			 */
			generic_handle_irq(irq_no);

			/*
			 * The handler may have re-enabled interrupts
			 * which could have caused a nested invocation
			 * of this code and make the copy of the
			 * status register we are using invalid.
			 */
			goto recalculate;
		}
		status >>= 1;
		irq_no++;
	}
}

/**
 *	metag_internal_irq_init_cpu - regsister with the Meta cpu
 *	@cpu:	the CPU to register on
 *
 *	Configure @cpu's TR1 irq so that we can demux irqs.
 */
static void metag_internal_irq_init_cpu(int cpu)
{
	unsigned int thread = cpu_2_hwthread_id[cpu];

	/* Register the multiplexed IRQ handler */
	irq_set_chained_handler(TBID_SIGNUM_TR1(thread),
				metag_internal_irq_demux);
	irq_set_irq_type(TBID_SIGNUM_TR1(thread), IRQ_TYPE_LEVEL_LOW);
}

/**
 *	metag_internal_irq_register - register internal IRQs
 *
 *	Register the irq chip and handler function for all internal IRQs
 */
void __init init_internal_IRQ(void)
{
	unsigned int cpu;
	unsigned int i;

	for (i = HWSTATMETA_TO_IRQ(0); i < HWSTATEXT_TO_IRQ(0); i++) {
		/* only register interrupt if it is mapped */
		if (metag_hwvec_addr(i)) {
			mapped |= (1 << IRQ_TO_OFFSET(i));
			irq_set_chip_and_handler(i, &internal_irq_edge_chip,
							handle_edge_irq);
		}
	}

	/* Setup TR1 for all cpus. */
	for_each_possible_cpu(cpu)
		metag_internal_irq_init_cpu(cpu);
};
