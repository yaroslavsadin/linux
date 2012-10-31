/*
 * Copyright (C) 2005,2006,2007,2008 Imagination Technologies Ltd.
 *
 * This file contains the Meta-specific time handling details.
 *
 */

#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/param.h>
#include <linux/time.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/clocksource.h>
#include <linux/clockchips.h>
#include <linux/interrupt.h>

#include <asm/clock.h>
#define METAG_ALL_VALUES
#define METAC_ALL_VALUES
#include <asm/tbx/machine.inc>
#include <asm/tbx/metagtbx.h>
#include <asm/hwthread.h>
#include <asm/core_reg.h>

#define HARDWARE_FREQ		1000000	/* 1MHz */
#define HARDWARE_DIV		1	/* divide by 1 = 1MHz clock */
#define HARDWARE_TO_NS_SHIFT	10	/* convert ticks to ns */

static unsigned int hwtimer_freq = HARDWARE_FREQ;
static DEFINE_PER_CPU(struct clock_event_device, local_clockevent);
static DEFINE_PER_CPU(char [11], local_clockevent_name);

static int metag_timer_set_next_event(unsigned long delta,
				      struct clock_event_device *dev)
{
	TBI_SETREG(TXTIMERI, -delta);
	return 0;
}

static void metag_timer_set_mode(enum clock_event_mode mode,
				 struct clock_event_device *evt)
{
	switch (mode) {
	case CLOCK_EVT_MODE_ONESHOT:
	case CLOCK_EVT_MODE_RESUME:
		break;

	case CLOCK_EVT_MODE_SHUTDOWN:
		/* We should disable the IRQ here */
		break;

	case CLOCK_EVT_MODE_PERIODIC:
	case CLOCK_EVT_MODE_UNUSED:
		WARN_ON(1);
		break;
	};
}

static inline cycle_t txtimer_read(void)
{
	unsigned int ret;

	__asm__ volatile ("MOV	%0, TXTIMER\n" : "=r" (ret));
	return ret;
}

static cycle_t metag_clocksource_read(struct clocksource *cs)
{
	return txtimer_read();
}

static struct clocksource clocksource_metag = {
	.name = "META",
	.rating = 200,
	.mask = CLOCKSOURCE_MASK(32),
	.read = metag_clocksource_read,
	.flags = CLOCK_SOURCE_IS_CONTINUOUS,
};

static irqreturn_t metag_timer_interrupt(int irq, void *dummy)
{
	struct clock_event_device *evt = &__get_cpu_var(local_clockevent);

	evt->event_handler(evt);

	return IRQ_HANDLED;
}

static struct irqaction metag_timer_irq = {
	.name = "META core timer",
	.handler = metag_timer_interrupt,
	.flags = IRQF_TIMER | IRQF_IRQPOLL | IRQF_PERCPU,
};

unsigned long long sched_clock(void)
{
	unsigned long long ticks = txtimer_read();
	return ticks << HARDWARE_TO_NS_SHIFT;
}

void __cpuinit local_timer_setup(unsigned int cpu)
{
	unsigned int txdivtime;
	struct clock_event_device *clk = &per_cpu(local_clockevent, cpu);
	char *name = per_cpu(local_clockevent_name, cpu);

	txdivtime = TBI_GETREG(TXDIVTIME);

	txdivtime &= ~TXDIVTIME_DIV_BITS;
	txdivtime |= (HARDWARE_DIV & TXDIVTIME_DIV_BITS);

	TBI_SETREG(TXDIVTIME, txdivtime);

	sprintf(name, "META %d", cpu);
	clk->name = name;
	clk->features = CLOCK_EVT_FEAT_ONESHOT,

	clk->rating = 200,
	clk->shift = 12,
	clk->irq = TBID_SIGNUM_TRT,
	clk->set_mode = metag_timer_set_mode,
	clk->set_next_event = metag_timer_set_next_event,

	clk->mult = div_sc(hwtimer_freq, NSEC_PER_SEC, clk->shift);
	clk->max_delta_ns = clockevent_delta2ns(0x7fffffff, clk);
	clk->min_delta_ns = clockevent_delta2ns(0xf, clk);
	clk->cpumask = cpumask_of(cpu);

	clockevents_register_device(clk);

	/*
	 * For all non-boot CPUs we need to synchronize our free
	 * running clock (TXTIMER) with the boot CPU's clock.
	 *
	 * While this won't be accurate, it should be close enough.
	 */
	if (cpu) {
		unsigned int thread0 = cpu_2_hwthread_id[0];
		unsigned long val;

		val = core_reg_read(TXUCT_ID, TXTIMER_REGNUM, thread0);

		asm volatile("MOV TXTIMER, %0\n" : : "r" (val));
	}
}

void __init time_init(void)
{
	/*
	 * On Meta 2 SoCs, the actual frequency of the timer is based on the
	 * Meta core clock speed divided by an integer, so it is only
	 * approximately 1MHz. Calculating the real frequency here drastically
	 * reduces clock skew on these SoCs.
	 */
#ifdef CONFIG_META21
	hwtimer_freq = get_coreclock() / (readl(EXPAND_TIMER_DIV) + 1);
#endif
	clocksource_register_hz(&clocksource_metag, hwtimer_freq);

	setup_irq(TBID_SIGNUM_TRT, &metag_timer_irq);

	local_timer_setup(smp_processor_id());
}
