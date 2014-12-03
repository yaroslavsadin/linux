/*
 * AXS101 Software Development Platform
 *
 * Copyright (C) 2013, 2014 Synopsys, Inc. (www.synopsys.com)
 *
 * Mischa Jonker <mjonker@synopsys.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/init.h>
#include <linux/clk-provider.h>
#include <linux/of_platform.h>
#include <asm/mach_desc.h>
#include <asm/asm-offsets.h>
#include <asm/io.h>

static void axs10x_plat_init(void)
{
	of_clk_init(NULL);
	of_platform_populate(NULL, of_default_bus_match_table, NULL, NULL);
}

#define AXC001_CREG	0xF0001000
#define AXS_MB_CREG	0xE0011000

#define AXC001_SLV_NONE		0
#define AXC001_SLV_DDR_PORT0	1
#define AXC001_SLV_SRAM		2
#define AXC001_SLV_AXI_TUNNEL	3
#define AXC001_SLV_EM6_ICCM	4
#define AXC001_SLV_EM6_DCCM	5
#define AXC001_SLV_AXI2APB	6
#define AXC001_SLV_DDR_PORT1	7

#define AXS_MB_SLV_NONE		0
#define AXS_MB_SLV_AXI_TUNNEL_1	1
#define AXS_MB_SLV_AXI_TUNNEL_2	2
#define AXS_MB_SLV_SRAM		3
#define AXS_MB_SLV_CONTROL	4

#define CREG_MB_ARC770_IRQ_MUX	0x114

static const int axc001_memmap[16][2] = {
	{AXC001_SLV_AXI_TUNNEL,		0x0},	/* 0x0000.0000 */
	{AXC001_SLV_AXI_TUNNEL,		0x1},	/* 0x1000.0000 */
	{AXC001_SLV_SRAM,		0x0},	/* 0x0000.0000 */
	{AXC001_SLV_NONE,		0x3},	/* 0x3000.0000 */
	{AXC001_SLV_NONE,		0x4},	/* 0x4000.0000 */
	{AXC001_SLV_NONE,		0x5},	/* 0x5000.0000 */
	{AXC001_SLV_NONE,		0x6},	/* 0x6000.0000 */
	{AXC001_SLV_NONE,		0x7},	/* 0x7000.0000 */
	{AXC001_SLV_DDR_PORT0,		0x0},	/* 0x0000.0000 */
	{AXC001_SLV_DDR_PORT0,		0x1},	/* 0x1000.0000 */
	{AXC001_SLV_DDR_PORT0,		0x2},	/* 0x2000.0000 */
	{AXC001_SLV_DDR_PORT0,		0x3},	/* 0x3000.0000 */
	{AXC001_SLV_NONE,		0x0},	/* 0x0000.0000 */
	{AXC001_SLV_AXI_TUNNEL,		0xD},	/* 0xD000.0000 */
	{AXC001_SLV_AXI_TUNNEL,		0xE},	/* 0xE000.0000 */
	{AXC001_SLV_AXI2APB,		0x0},	/* 0x0000.0000 */
};

static const int axc001_axi_tunnel_memmap[16][2] = {
	{AXC001_SLV_AXI_TUNNEL,		0x0},	/* 0x0000.0000 */
	{AXC001_SLV_AXI_TUNNEL,		0x1},	/* 0x1000.0000 */
	{AXC001_SLV_SRAM,		0x0},	/* 0x0000.0000 */
	{AXC001_SLV_NONE,		0x3},	/* 0x3000.0000 */
	{AXC001_SLV_NONE,		0x4},	/* 0x4000.0000 */
	{AXC001_SLV_NONE,		0x5},	/* 0x5000.0000 */
	{AXC001_SLV_NONE,		0x6},	/* 0x6000.0000 */
	{AXC001_SLV_NONE,		0x7},	/* 0x7000.0000 */
	{AXC001_SLV_DDR_PORT1,		0x0},	/* 0x0000.0000 */
	{AXC001_SLV_DDR_PORT1,		0x1},	/* 0x1000.0000 */
	{AXC001_SLV_DDR_PORT1,		0x2},	/* 0x2000.0000 */
	{AXC001_SLV_DDR_PORT1,		0x3},	/* 0x3000.0000 */
	{AXC001_SLV_NONE,		0x0},	/* 0x0000.0000 */
	{AXC001_SLV_AXI_TUNNEL,		0xD},	/* 0xD000.0000 */
	{AXC001_SLV_AXI_TUNNEL,		0xE},	/* 0xE000.0000 */
	{AXC001_SLV_AXI2APB,		0x0},	/* 0x0000.0000 */
};

static const int axs_mb_memmap[16][2] = {
	{AXS_MB_SLV_SRAM,		0x0},	/* 0x0000.0000 */
	{AXS_MB_SLV_SRAM,		0x0},	/* 0x0000.0000 */
	{AXS_MB_SLV_NONE,		0x0},	/* 0x0000.0000 */
	{AXS_MB_SLV_NONE,		0x0},	/* 0x0000.0000 */
	{AXS_MB_SLV_NONE,		0x0},	/* 0x0000.0000 */
	{AXS_MB_SLV_NONE,		0x0},	/* 0x0000.0000 */
	{AXS_MB_SLV_NONE,		0x0},	/* 0x0000.0000 */
	{AXS_MB_SLV_NONE,		0x0},	/* 0x0000.0000 */
	{AXS_MB_SLV_AXI_TUNNEL_1,	0x8},	/* 0x8000.0000 */
	{AXS_MB_SLV_AXI_TUNNEL_1,	0x9},	/* 0x9000.0000 */
	{AXS_MB_SLV_AXI_TUNNEL_1,	0xA},	/* 0xA000.0000 */
	{AXS_MB_SLV_AXI_TUNNEL_1,	0xB},	/* 0xB000.0000 */
	{AXS_MB_SLV_NONE,		0x0},	/* 0x0000.0000 */
	{AXS_MB_SLV_AXI_TUNNEL_2,	0xD},	/* 0xD000.0000 */
	{AXS_MB_SLV_CONTROL,		0x0},	/* 0x0000.0000 */
	{AXS_MB_SLV_AXI_TUNNEL_1,	0xF},	/* 0xF000.0000 */
};

/*
 * base + 0x00 : slave select (low 32 bits)
 * base + 0x04 : slave select (high 32 bits)
 * base + 0x08 : slave offset (low 32 bits)
 * base + 0x0C : slave offset (high 32 bits)
 */
static void axs101_set_memmap(void __iomem *base, const int memmap[16][2])
{
	int i;
	u64 slave_select, slave_offset;

	slave_select = slave_offset = 0;
	for (i = 0; i < 16; i++) {
		slave_select |= ((u64) memmap[i][0]) << (i << 2);
		slave_offset |= ((u64) memmap[i][1]) << (i << 2);
	}
	iowrite32(slave_select & 0xffffffff,	base + 0x0);
	iowrite32(slave_select >> 32,		base + 0x4);
	iowrite32(slave_offset & 0xffffffff,	base + 0x8);
	iowrite32(slave_offset >> 32,		base + 0xC);
}

static int wait_cgu_lock(void __iomem *lock_reg, uint32_t val)
{
	unsigned long timeout = jiffies + msecs_to_jiffies(100);
	while ((ioread32(lock_reg) & 1) == val) {
		if (time_after(jiffies, timeout))
			return -EBUSY;
		cpu_relax();
	}
	return 0;
}

static int write_cgu_reg(uint32_t value, void __iomem *reg,
			 void __iomem *lock_reg)
{
	int retval = 0;
	iowrite32(value, reg);
	retval |= wait_cgu_lock(lock_reg, 1);	/* wait for unlock */
	retval |= wait_cgu_lock(lock_reg, 0);	/* wait for re-lock */
	return retval;
}

static void axs101_early_init(void)
{
	int i;

	/* ARC 770D memory view */
	axs101_set_memmap((void __iomem *) AXC001_CREG + 0x20,
			      axc001_memmap);

	iowrite32(1, (void __iomem *) AXC001_CREG + 0x34);	/* Update */

	/* AXI tunnel memory view (incoming traffic from AXS_MB into AXC001 */
	axs101_set_memmap((void __iomem *) AXC001_CREG + 0x60,
			      axc001_axi_tunnel_memmap);

	iowrite32(1, (void __iomem *) AXC001_CREG + 0x74);	/* Update */

	/* AXS_MB DMA peripherals memory view
	   (incoming traffic from AXS_MB peripherals towards AXS_MB bus) */
	for (i = 0; i <= 10; i++)
		axs101_set_memmap((void __iomem *) AXS_MB_CREG + (i << 4),
				      axs_mb_memmap);

	iowrite32(0x3ff, (void __iomem *) AXS_MB_CREG + 0x100); /* Update */

	/* GPIO pins 18 and 19 are used as UART rx and tx, respectively. */
	iowrite32(0x01, (void __iomem *) AXC001_CREG + 0x120);

	/* Set up the AXS_MB interrupt system.*/
	/* AXS_MB mux interrupts to GPIO7) */
	iowrite32(0x01, (void __iomem *) AXS_MB_CREG + 0x214);

	/* reset ethernet and ULPI interfaces */
	iowrite32(0x18, (void __iomem *) AXS_MB_CREG + 0x220);


	/* map GPIO 14:10 to ARC 9:5 (IRQ mux change for rev 2 boards) */
	iowrite32(0x52, (void __iomem *) AXC001_CREG + CREG_MB_ARC770_IRQ_MUX);

	/* Set clock divider value depending on mother board version */
	if (ioread32((void __iomem *) AXS_MB_CREG + 0x234) & (1 << 28)) {
		/*
		 * 1 => HT-3 (rev3.0)
		 *
		 * Set clock for PGU, 74.25 Mhz
		 * to obtain 74.25MHz pixel clock, required for 720p60
		 * (27 * 22) / 8 == 74.25
		 */
		write_cgu_reg(0x2041, (void __iomem *) 0xe0010080,
			      (void __iomem *) 0xe0010110);
		write_cgu_reg((22 << 6) | 22, (void __iomem *) 0xe0010084,
			      (void __iomem *) 0xe0010110);
		write_cgu_reg((8 << 6) | 8, (void __iomem *) 0xe0010088,
			      (void __iomem *) 0xe0010110);
	}
	else {
		/*
		 * 0 => HT-2 (rev2.0)
		 *
		 * Set clock for PGU, 150 Mhz
		 * to obtain 75MHz pixel clock, required for 720p60
		 * (25 * 18) / 3 == 25 * 6 == 150
		 */

		write_cgu_reg(0x2000, (void __iomem *) 0xe0010080,
			      (void __iomem *) 0xe0010110);
		write_cgu_reg((18 << 6) | 18, (void __iomem *) 0xe0010084,
			      (void __iomem *) 0xe0010110);
		write_cgu_reg((3 << 6) | 3, (void __iomem *) 0xe0010088,
			      (void __iomem *) 0xe0010110);
	}
}

#define AXC003_CGU	0xF0000000
#define AXC003_CREG	0xF0001000
#define AXC003_MST_AXI_TUNNEL	0
#define AXC003_MST_HS38		1

#define CREG_MB_IRQ_MUX		0x214
#define CREG_CPU_AXI_M0_IRQ_MUX	0x440
#define CREG_CPU_GPIO_UART_MUX	0x480
#define CREG_CPU_TUN_IO_CTRL	0x494


union pll_reg {
	struct {
		unsigned int low:6, high:6, edge:1, bypass:1, noupd:1, pad:17;
	};
	unsigned int val;
};

unsigned int get_freq(void)
{
	union pll_reg idiv, fbdiv, odiv;
	unsigned int f = 33333333;

	idiv.val = ioread32((void __iomem *)AXC003_CGU + 0x80 + 0);
	fbdiv.val = ioread32((void __iomem *)AXC003_CGU + 0x80 + 4);
	odiv.val = ioread32((void __iomem *)AXC003_CGU + 0x80 + 8);

	if (idiv.bypass != 1)
		f = f / (idiv.low + idiv.high);

	if (fbdiv.bypass != 1)
		f = f * (fbdiv.low + fbdiv.high);

	if (odiv.bypass != 1)
		f = f / (odiv.low + odiv.high);

	f = (f + 500000) / 1000000; /* Rounding */
	return f;
}

unsigned int encode_pll(unsigned int id, int upd)
{
	union pll_reg div;

	div.val = 0;

	div.noupd = !upd;
	div.bypass = id == 1 ? 1 : 0;
	div.edge = (id%2 == 0) ? 0 : 1;  /* 0 = rising */
	div.low = (id%2 == 0) ? id >> 1 : (id >> 1)+1;
	div.high = id >> 1;

	return div.val;
}

void set_freq(unsigned int id, unsigned int fd, unsigned int od)
{
	write_cgu_reg(encode_pll(id, 0),
		(void __iomem *)AXC003_CGU + 0x80 + 0,
		(void __iomem *)AXC003_CGU + 0x110);

	write_cgu_reg(encode_pll(fd, 0),
		(void __iomem *)AXC003_CGU + 0x80 + 4,
		(void __iomem *)AXC003_CGU + 0x110);

	write_cgu_reg(encode_pll(od, 1),
		(void __iomem *)AXC003_CGU + 0x80 + 8,
		(void __iomem *)AXC003_CGU + 0x110);
}

void print_axs_ver(void)
{
	union ver {
		struct {
			unsigned int d:5, m:4, y:12, pad:11;
		};
		unsigned int val;
	} ver;

	ver.val = ioread32((void __iomem *) (AXS_MB_CREG + 0x230));
	printk("AXS103: MB FPGA Date: %u-%u-%u\n", ver.d, ver.m, ver.y);

	ver.val = ioread32((void __iomem *) (AXC003_CREG + 4088));
	printk("AXS103: CPU FPGA Date: %u-%u-%u\n", ver.d, ver.m, ver.y);
}

extern void mcip_init_smp(unsigned int cpu);
extern void mcip_init_early_smp(void);

static void axs103_early_init(void)
{
	//set_freq(1, 1, 1);

	print_axs_ver();

	printk("Freq is %dMHz\n", get_freq());

	/* Memory maps already config in pre-bootloader */

	// creg_axc003_initDevice
	/* set GPIO mux to UART */
	iowrite32(0x01, (void __iomem *) AXC003_CREG + CREG_CPU_GPIO_UART_MUX);

	// uint32_t TUN_IO_CTRL; //RW,   0x494, control for AXI tunnel Register
	iowrite32((0x00100000U | 0x000C0000U | 0x00003322U),
		  (void __iomem *) AXC003_CREG + CREG_CPU_TUN_IO_CTRL);

	/* Set up the AXS_MB interrupt system.*/

	//creg_axc003_setIrqMux (Creg_Axc003_Master_ArcHs38 (1),
	//			Creg_Axc003_IntSource_Gpio_a12 (12) );

	iowrite32(12, (void __iomem *) (AXC003_CREG + CREG_CPU_AXI_M0_IRQ_MUX
					 + (AXC003_MST_HS38 << 2)));

	/* connect ICTL - Main Board with GPIO line */
	// creg_axs1xx_setIrqMux(creg_mb, IRQ_MUX_SEL);
	iowrite32(0x01, (void __iomem *) AXS_MB_CREG + CREG_MB_IRQ_MUX);

#ifdef CONFIG_ARC_MCIP
	/* No Hardware init, but filling the smp ops callbacks */
	mcip_init_early_smp();
#endif
}

#ifdef CONFIG_AXS101

static const char *axs101_compat[] __initconst = {
	"snps,axs101",
	NULL,
};

MACHINE_START(AXS101, "axs101")
	.dt_compat	= axs101_compat,
	.init_early	= axs101_early_init,
	.init_machine	= axs10x_plat_init,
	.init_irq	= NULL,
MACHINE_END

#endif	/* CONFIG_AXS101 */

#ifdef CONFIG_AXS103

/*
 * For the VDK OS-kit, to get the offset to pid and command fields
 */
char coware_swa_pid_offset[TASK_PID];
char coware_swa_comm_offset[TASK_COMM];

static const char *axs103_compat[] __initconst = {
	"snps,axs103",
	NULL,
};

MACHINE_START(AXS103, "axs103")
	.dt_compat	= axs103_compat,
	.init_early	= axs103_early_init,
	.init_machine	= axs10x_plat_init,
	.init_irq	= NULL,
#ifdef CONFIG_ARC_MCIP
	.init_smp	= mcip_init_smp,
#endif
MACHINE_END

#endif	/* CONFIG_AXS103 */
