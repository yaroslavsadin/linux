#ifndef __ASM_METAG_IRQ_H
#define __ASM_METAG_IRQ_H

#define HWSTATEXT_OFFSET_MAX	0
#define HWSTATEXT2_OFFSET_MAX	0
#define HWSTATEXT4_OFFSET_MAX	0
#define HWSTATEXT6_OFFSET_MAX	0
#define init_soc_IRQ NULL
#include <asm/irq_internal.h>
#ifndef HW_IRQS
#define HW_IRQS  (HWSTATMETA_OFFSET_MAX + HWSTATEXT_OFFSET_MAX + \
		HWSTATEXT2_OFFSET_MAX + HWSTATEXT4_OFFSET_MAX + \
		HWSTATEXT6_OFFSET_MAX)
#endif

#define META_IRQS 32

#define NR_IRQS (META_IRQS + HW_IRQS)

/*
 * Automatic IRQ numbering
 *
 * Maps triggers into IRQs depending on which triggers are used.
 * HWSTATEXT*_OFFSET_MAX must be defined in SoC's irq.h file.
 */
#define HWSTATMETA_TO_IRQ(offset)	(META_IRQS+(offset))

#define HWSTATEXT_TO_HWIRQ(offset)	(offset)
#define HWSTATEXT2_TO_HWIRQ(offset)	(HWSTATEXT_TO_HWIRQ(HWSTATEXT_OFFSET_MAX)+(offset))
#define HWSTATEXT4_TO_HWIRQ(offset)	(HWSTATEXT2_TO_HWIRQ(HWSTATEXT2_OFFSET_MAX)+(offset))
#define HWSTATEXT6_TO_HWIRQ(offset)	(HWSTATEXT4_TO_HWIRQ(HWSTATEXT4_OFFSET_MAX)+(offset))

#define HWSTATEXT_TO_IRQ(offset)	(HWSTATMETA_TO_IRQ(HWSTATMETA_OFFSET_MAX)+(offset))
#define HWSTATEXT2_TO_IRQ(offset)	(HWSTATEXT_TO_IRQ(HWSTATEXT_OFFSET_MAX)+(offset))
#define HWSTATEXT4_TO_IRQ(offset)	(HWSTATEXT2_TO_IRQ(HWSTATEXT2_OFFSET_MAX)+(offset))
#define HWSTATEXT6_TO_IRQ(offset)	(HWSTATEXT4_TO_IRQ(HWSTATEXT4_OFFSET_MAX)+(offset))

static inline unsigned int IRQ_TO_OFFSET(unsigned int irq)
{
	if (irq < HWSTATMETA_TO_IRQ(0))
		return irq;
	else if (irq < HWSTATEXT_TO_IRQ(0))
		return irq - HWSTATMETA_TO_IRQ(0);
	else if (irq < HWSTATEXT2_TO_IRQ(0))
		return irq - HWSTATEXT_TO_IRQ(0);
	else if (irq < HWSTATEXT4_TO_IRQ(0))
		return irq - HWSTATEXT2_TO_IRQ(0);
	else /* if (irq < HWSTATEXT6_TO_IRQ(0)) */
		return irq - HWSTATEXT6_TO_IRQ(0);
}

#ifdef CONFIG_4KSTACKS
extern void irq_ctx_init(int cpu);
extern void irq_ctx_exit(int cpu);
# define __ARCH_HAS_DO_SOFTIRQ
#else
# define irq_ctx_init(cpu) do { } while (0)
# define irq_ctx_exit(cpu) do { } while (0)
#endif

void tbi_startup_interrupt(int);
void tbi_shutdown_interrupt(int);

struct pt_regs;

extern void do_IRQ(int irq, struct pt_regs *regs);

#ifdef CONFIG_METAG_SUSPEND_MEM
int traps_save_context(void);
int traps_restore_context(void);
#endif

#include <asm-generic/irq.h>

#ifdef CONFIG_HOTPLUG_CPU
extern void migrate_irqs(void);
#endif

#endif /* __ASM_METAG_IRQ_H */
