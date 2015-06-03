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
#include <linux/slab.h>

#include <asm/asm-offsets.h>
#include <asm/clk.h>
#include <asm/mach_desc.h>
#include <asm/mcip.h>
#include <asm/io.h>

static __initdata int mb_rev;

static void __init enable_gpio_intc_wire(void)
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

void __init noinline write_cgu_reg(uint32_t value, void __iomem *reg,
			 void __iomem *lock_reg)
{
	unsigned int loops = 128 * 1024, ctr;

	iowrite32(value, reg);

	ctr = loops;
	while (((ioread32(lock_reg) & 1) == 1) && ctr--) /* wait for unlock */
		cpu_relax();

	ctr = loops;
	while (((ioread32(lock_reg) & 1) == 0) && ctr--) /* wait for re-lock */
		cpu_relax();
}

#define AXS_MB_CGU	0xE0010000
#define AXS_MB_CREG	0xE0011000

static void __init setup_pgu_clk(void)
{
	/*
	 * PGU clock dividers settings for MB version
	 * MB v2:  720p: Input 25MHz: (25 * 18) / 3 == 150 => IDIV2 => 75 MHz
	 * MB v3: 1280p: Input 27MHz: (27 * 22) / 8 == 74.25 MHz (IDIV2 removed)
	 */
	unsigned int mb2[] = {0x2000, 18, 3};
	unsigned int mb3[] = {0x2000, 22, 8};
	const unsigned int *div;

	div = (mb_rev == 2) ? mb2 : mb3;

	write_cgu_reg(div[0], (void __iomem *) AXS_MB_CGU + 0x80 + 0,
			      (void __iomem *) AXS_MB_CGU + 0x110);
	write_cgu_reg((div[1] << 6) | div[1], (void __iomem *) AXS_MB_CGU + 0x80 + 4,
			      (void __iomem *) AXS_MB_CGU + 0x110);
	write_cgu_reg((div[2] << 6) | div[2], (void __iomem *) AXS_MB_CGU + 0x80 + 8,
			      (void __iomem *) AXS_MB_CGU + 0x110);
}

static void __init setup_nand_bus_width(void)
{
	/*
	 * There're 2 versions of motherboards that could be used in ARC SDP.
	 * Among other things different NAND ICs are in use:
	 * [1] v2 board sports MT29F4G08ABADAWP while
	 * [2] v3 board sports MT29F4G16ABADAWP
	 *
	 * They are almost the same except data bus width 8-bit in [1] and
	 * 16-bit in [2]. And for proper support of 16-bit data bus
	 * NAND_BUSWIDTH_16 option must be passed to NAND driver core.
	 *
	 * Here in platform init code we update device tree description with
	 * proper value of "nand-bus-width" property of "snps,axs-nand"
	 * compatible nodes so on real NAND driver probe it gets proper value.
	 */

	struct device_node *dn = of_find_compatible_node(NULL, NULL,
							 "snps,axs-nand");
	struct property *prop;
	u32 buswidth;

	if (!dn)
		return;

	prop = kzalloc(sizeof(*prop) + sizeof(u32), GFP_KERNEL);

	buswidth = (mb_rev == 2) ? 8 : 16;

	prop->length = sizeof(buswidth);
	prop->name = kstrdup("nand-bus-width", GFP_KERNEL);
	prop->value = prop + 1;
	*(u32 *)prop->value = cpu_to_be32(buswidth);

	if (of_find_property(dn, prop->name, NULL))
		of_update_property(dn, prop);
	else
		of_add_property(dn, prop);
}

static void __init axs10x_plat_init(void)
{
	setup_nand_bus_width();
	of_platform_populate(NULL, of_default_bus_match_table, NULL, NULL);
}

static void __init axs10x_print_board_ver(unsigned int creg, const char *str)
{
	union ver {
		struct {
#ifdef CONFIG_CPU_BIG_ENDIAN
			unsigned int pad:11, y:12, m:4, d:5;
#else
			unsigned int d:5, m:4, y:12, pad:11;
#endif
		};
		unsigned int val;
	} board;

	board.val = ioread32((void __iomem *)creg);
	pr_info("AXS: %s FPGA Date: %u-%u-%u\n", str, board.d, board.m, board.y);
}

static void __init axs10x_early_init(void)
{
	char mb[32];

	/* Determine motherboard version */
	if (ioread32((void __iomem *) AXS_MB_CREG + 0x234) & (1 << 28))
		/* 1 => HT-3 (rev3.0) */
		mb_rev = 3;
	else
		/* 0 => HT-2 (rev2.0) */
		mb_rev = 2;

	scnprintf(mb, 32, "MainBoard v%d", mb_rev);

	enable_gpio_intc_wire();
	setup_pgu_clk();

	axs10x_print_board_ver(AXS_MB_CREG + 0x230, mb);
}

#ifdef CONFIG_AXS101

#define AXC001_CREG	0xF0001000

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

static __initdata const int axc001_memmap[16][2] = {
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

static __initdata const int axc001_axi_tunnel_memmap[16][2] = {
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

static __initdata const int axs_mb_memmap[16][2] = {
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
static void __init axs101_set_memmap(void __iomem *base, const int memmap[16][2])
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

static void __init axs101_early_init(void)
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

#endif	/* CONFIG_AXS101 */

#ifdef CONFIG_AXS103

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
#ifdef CONFIG_CPU_BIG_ENDIAN
		unsigned int pad:17, noupd:1, bypass:1, edge:1, high:6,low:6;
#else
		unsigned int low:6, high:6, edge:1, bypass:1, noupd:1, pad:17;
#endif
	};
	unsigned int val;
};

static unsigned int __init axs103_get_freq(void)
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

static inline unsigned int __init encode_div(unsigned int id, int upd)
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

void __init axs103_set_freq(unsigned int id, unsigned int fd, unsigned int od)
{
	write_cgu_reg(encode_div(id, 0),
		(void __iomem *)AXC003_CGU + 0x80 + 0,
		(void __iomem *)AXC003_CGU + 0x110);

	write_cgu_reg(encode_div(fd, 0),
		(void __iomem *)AXC003_CGU + 0x80 + 4,
		(void __iomem *)AXC003_CGU + 0x110);

	write_cgu_reg(encode_div(od, 1),
		(void __iomem *)AXC003_CGU + 0x80 + 8,
		(void __iomem *)AXC003_CGU + 0x110);
}

static void __init axs103_early_init(void)
{
	switch (arc_get_core_freq()/1000000) {
	case 33:
		axs103_set_freq(1, 1, 1);
		break;
	case 50:
		axs103_set_freq(1, 30, 20);
		break;
	case 75:
		axs103_set_freq(2, 45, 10);
		break;
	case 90:
		axs103_set_freq(2, 54, 10);
		break;
	case 100:
		axs103_set_freq(1, 30, 10);
		break;
	case 125:
		axs103_set_freq(2, 45,  6);
		break;
	default:
		/*
		 * In this case, core_frequency derived from
		 * DT "clock-frequency" might not match with board value.
		 * Hence update it to match the board value.
		 */
		arc_set_core_freq(axs103_get_freq() * 1000000);
		break;
	}

	pr_info("Freq is %dMHz\n", axs103_get_freq());

	/* Memory maps already config in pre-bootloader */

	/* set GPIO mux to UART */
	iowrite32(0x01, (void __iomem *) AXC003_CREG + CREG_CPU_GPIO_UART_MUX);

	iowrite32((0x00100000U | 0x000C0000U | 0x00003322U),
		  (void __iomem *) AXC003_CREG + CREG_CPU_TUN_IO_CTRL);

	/* Set up the AXS_MB interrupt system.*/
	iowrite32(12, (void __iomem *) (AXC003_CREG + CREG_CPU_AXI_M0_IRQ_MUX
					 + (AXC003_MST_HS38 << 2)));

	/* connect ICTL - Main Board with GPIO line */
	iowrite32(0x01, (void __iomem *) AXS_MB_CREG + CREG_MB_IRQ_MUX);

	axs10x_print_board_ver(AXC003_CREG + 4088, "AXC003 CPU Card");

	axs10x_early_init();

#ifdef CONFIG_ARC_MCIP
	/* No Hardware init, but filling the smp ops callbacks */
	mcip_init_early_smp();
#endif
}
#endif

#ifdef CONFIG_AXS101

static const char *axs101_compat[] __initconst = {
	"snps,axs101",
	NULL,
};

MACHINE_START(AXS101, "axs101")
	.dt_compat	= axs101_compat,
	.init_early	= axs101_early_init,
	.init_machine	= axs10x_plat_init,
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
#ifdef CONFIG_ARC_MCIP
	.init_smp	= mcip_init_smp,
#endif
MACHINE_END

#endif	/* CONFIG_AXS103 */
