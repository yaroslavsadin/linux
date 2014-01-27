/*
 * AXS101 Software Development Platform
 *
 * Copyright (C) 2013-15 Synopsys, Inc. (www.synopsys.com)
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
#include <asm/io.h>

static void enable_gpio_intc_wire(void)
{
	/*
	 * Peripherals on CPU Card and Mother Board are wired to cpu intc via
	 * intermediate DW APB GPIO blocks (mainly for debouncing)
	 *
	 *         ---------------------
	 *        |  snps,arc700-intc |
	 *        ---------------------
	 *          | #7          | #15
	 * -------------------   -------------------
	 * | snps,dw-apb-gpio |  | snps,dw-apb-gpio |
	 * -------------------   -------------------
	 *        |                         |
	 *        |                 [ Debug UART on cpu card ]
	 *        |
	 * ------------------------
	 * | snps,dw-apb-intc (MB)|
	 * ------------------------
	 *  |      |       |      |
	 * [eth] [uart]        [... other perip on Main Board]
	 *
	 * Current implementation of "irq-dw-apb-ictl" driver doesn't work well
	 * with stacked INTCs. In particular problem happens if its master INTC
	 * not yet instantiated. See discussion here -
	 * https://lkml.org/lkml/2015/3/4/755
	 *
	 * So setup the first gpio block as a passive pass thru and hide it from
	 * DT hardware topology - connect MB intc directly to cpu intc
	 * The GPIO "wire" needs to be init nevertheless (here)
	 *
	 * One side adv is that peripheral interrupt handling avoids one nested
	 * intc ISR hop
	 */
#define GPIO_INTC		0xf0003000
#define GPIO_INTEN		0x30
#define GPIO_INTMASK		0x34
#define GPIO_INTTYPE_LEVEL	0x38
#define GPIO_INT_POLARITY	0x3c
#define MB_TO_GPIO_IRQ		12

	iowrite32(~(1 << MB_TO_GPIO_IRQ), (void __iomem *) (GPIO_INTC + GPIO_INTMASK));
	iowrite32(0, (void __iomem *) (GPIO_INTC + GPIO_INTTYPE_LEVEL));
	iowrite32(~0, (void __iomem *) (GPIO_INTC + GPIO_INT_POLARITY));
	iowrite32(1 << MB_TO_GPIO_IRQ, (void __iomem *) (GPIO_INTC + GPIO_INTEN));
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

#define AXS_MB_CREG	0xE0011000

static void setup_pgu_clk(void)
{
	/*
	 * Set clock for PGU, 150 Mhz
	 * to obtain 75MHz pixel clock, required for 720p60
	 * (25 * 18) / 3 == 25 * 6 == 150
	 */

	write_cgu_reg(0x2000,
		      (void __iomem *) 0xe0010080, (void __iomem *) 0xe0010110);
	write_cgu_reg((18 << 6) | 18,
		      (void __iomem *) 0xe0010084, (void __iomem *) 0xe0010110);
	write_cgu_reg((3 << 6) | 3,
		      (void __iomem *) 0xe0010088, (void __iomem *) 0xe0010110);
}

static void axs10x_early_init(void)
{
	enable_gpio_intc_wire();
	setup_pgu_clk();
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
#define CREG_MB_IRQ_MUX		0x214
#define CREG_MB_SW_RESET	0x220

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
	{AXC001_SLV_DDR_PORT1,		0x0},	/* 0x0000.0000 */
	{AXC001_SLV_DDR_PORT1,		0x1},	/* 0x1000.0000 */
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
	{AXC001_SLV_DDR_PORT0,		0x0},	/* 0x0000.0000 */
	{AXC001_SLV_DDR_PORT0,		0x1},	/* 0x1000.0000 */
	{AXC001_SLV_DDR_PORT1,		0x0},	/* 0x0000.0000 */
	{AXC001_SLV_DDR_PORT1,		0x1},	/* 0x1000.0000 */
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
	iowrite32(0x01, (void __iomem *) AXS_MB_CREG + CREG_MB_IRQ_MUX);

	/* reset ethernet and ULPI interfaces */
	iowrite32(0x18, (void __iomem *) AXS_MB_CREG + CREG_MB_SW_RESET);

	/* map GPIO 14:10 to ARC 9:5 (IRQ mux change for rev 2 boards) */
	iowrite32(0x52, (void __iomem *) AXC001_CREG + CREG_MB_ARC770_IRQ_MUX);

	axs10x_early_init();
}

static const char *axs101_compat[] __initconst = {
	"snps,axs101",
	NULL,
};

MACHINE_START(AXS101, "axs101")
	.dt_compat	= axs101_compat,
	.init_early	= axs101_early_init,
MACHINE_END
