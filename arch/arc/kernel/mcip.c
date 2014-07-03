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

#define ARC_REG_MCIP_BCR	0x0d0
#define ARC_REG_MCIP_CMD	0x600
#define ARC_REG_MCIP_DATA	0x601
#define ARC_REG_MCIP_READBK	0x602

struct mcip_bcr {
#ifdef CONFIG_CPU_BIG_ENDIAN
#else
	unsigned int ver:8,
		     pad:1, ipi:1, sem:1, msg:1, pad2:1, dbg:1, grtc:1, iocoh:1,
		     num_cores:6, pad3:10;
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
};

static void inline __mcip_cmd(unsigned int cmd, unsigned int param)
{
	struct mcip_cmd buf;

	buf.pad = 0;
	buf.cmd = cmd;
	buf.param = param;

	WRITE_AUX(ARC_REG_MCIP_CMD, buf);
}

static DEFINE_RAW_SPINLOCK(mcip_lock);

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

	cpu = read_aux_reg(ARC_REG_MCIP_READBK);	/* 1,2,4,8... */

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

	sprintf(smp_cpuinfo_buf, "Extn [SMP]\t: MCIP (v%d): %d cores with %s %s\n",
		mp.ver, mp.num_cores,
		IS_AVAIL1(mp.ipi, "IPI"),
		IS_AVAIL1(mp.grtc, "Glb RTC"));

	plat_smp_ops.info = smp_cpuinfo_buf;

	plat_smp_ops.cpu_kick = mcip_wakeup_cpu;
	plat_smp_ops.ipi_send = mcip_ipi_send;
	plat_smp_ops.ipi_clear = mcip_ipi_clear;
}
