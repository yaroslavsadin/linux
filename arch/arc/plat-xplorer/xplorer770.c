/*
 * ARC Xplorer System 770D/EM6/AS221 platform specific code
 *
 * Copyright (C) 2013 Synopsys, Inc. (www.synopsys.com)
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 */

#include <linux/init.h>
#include <linux/clk-provider.h>
#include <linux/of_platform.h>
#include <asm/mach_desc.h>
#include <asm/io.h>

#define CPUTILE_CREG	0xf0001000
#define FPGA_CREG	0xe0011000

static const char *xplorer770_compat[] __initdata = {
	"snps,xplorer770",
	NULL,
};

#define CPUTILE_SLV_NONE	0
#define CPUTILE_SLV_DDR_PORT0	1
#define CPUTILE_SLV_SRAM	2
#define CPUTILE_SLV_AXI_TUNNEL	3
#define CPUTILE_SLV_EM6_ICCM	4
#define CPUTILE_SLV_EM6_DCCM	5
#define CPUTILE_SLV_AXI2APB	6
#define CPUTILE_SLV_DDR_PORT1	7

#define FPGA_SLV_AXI_TUNNEL_1	1
#define FPGA_SLV_AXI_TUNNEL_2	2
#define FPGA_SLV_SRAM		3
#define FPGA_SLV_CONTROL	4

static const int cputile_memmap[16][2] = {
	{CPUTILE_SLV_EM6_ICCM,		0x0},	/* 0x0000.0000 */
	{CPUTILE_SLV_SRAM,		0x0},	/* 0x1000.0000 */
	{CPUTILE_SLV_AXI_TUNNEL,	0x2},	/* 0x2000.0000 */
	{CPUTILE_SLV_AXI_TUNNEL,	0x3},	/* 0x3000.0000 */
	{CPUTILE_SLV_AXI_TUNNEL,	0x4},	/* 0x4000.0000 */
	{CPUTILE_SLV_AXI_TUNNEL,	0x5},	/* 0x5000.0000 */
	{CPUTILE_SLV_AXI_TUNNEL,	0x6},	/* 0x6000.0000 */
	{CPUTILE_SLV_AXI_TUNNEL,	0x7},	/* 0x7000.0000 */
	{CPUTILE_SLV_DDR_PORT0,		0x0},	/* 0x8000.0000 */
	{CPUTILE_SLV_DDR_PORT0,		0x1},	/* 0x9000.0000 */
	{CPUTILE_SLV_DDR_PORT0,		0x2},	/* 0xA000.0000 */
	{CPUTILE_SLV_DDR_PORT0,		0x3},	/* 0xB000.0000 */
	{CPUTILE_SLV_AXI_TUNNEL,	0xC},	/* 0xC000.0000 */
	{CPUTILE_SLV_AXI_TUNNEL,	0xD},	/* 0xD000.0000 */
	{CPUTILE_SLV_AXI_TUNNEL,	0xE},	/* 0xE000.0000 */
	{CPUTILE_SLV_AXI2APB,		0x0},	/* 0xF000.0000 */
};

static const int cputile_axi_tunnel_memmap[16][2] = {
	{CPUTILE_SLV_EM6_ICCM,		0x0},	/* 0x0000.0000 */
	{CPUTILE_SLV_SRAM,		0x0},	/* 0x1000.0000 */
	{CPUTILE_SLV_NONE,		0x0},	/* 0x2000.0000 */
	{CPUTILE_SLV_NONE,		0x0},	/* 0x3000.0000 */
	{CPUTILE_SLV_NONE,		0x0},	/* 0x4000.0000 */
	{CPUTILE_SLV_NONE,		0x0},	/* 0x5000.0000 */
	{CPUTILE_SLV_NONE,		0x0},	/* 0x6000.0000 */
	{CPUTILE_SLV_NONE,		0x0},	/* 0x7000.0000 */
	{CPUTILE_SLV_DDR_PORT0,		0x0},	/* 0x8000.0000 */
	{CPUTILE_SLV_DDR_PORT0,		0x1},	/* 0x9000.0000 */
	{CPUTILE_SLV_DDR_PORT0,		0x2},	/* 0xA000.0000 */
	{CPUTILE_SLV_DDR_PORT0,		0x3},	/* 0xB000.0000 */
	{CPUTILE_SLV_NONE,		0x0},	/* 0xC000.0000 */
	{CPUTILE_SLV_NONE,		0x0},	/* 0xD000.0000 */
	{CPUTILE_SLV_NONE,		0x0},	/* 0xE000.0000 */
	{CPUTILE_SLV_AXI2APB,		0x0},	/* 0xF000.0000 */
};

static const int fpga_memmap[16][2] = {
	{FPGA_SLV_SRAM,			0x0},	/* 0x0000.0000 */
	{FPGA_SLV_AXI_TUNNEL_1,		0x1},	/* 0x1000.0000 */
	{FPGA_SLV_AXI_TUNNEL_1,		0x2},	/* 0x2000.0000 */
	{FPGA_SLV_AXI_TUNNEL_1,		0x3},	/* 0x3000.0000 */
	{FPGA_SLV_AXI_TUNNEL_1,		0x4},	/* 0x4000.0000 */
	{FPGA_SLV_AXI_TUNNEL_1,		0x5},	/* 0x5000.0000 */
	{FPGA_SLV_AXI_TUNNEL_1,		0x6},	/* 0x6000.0000 */
	{FPGA_SLV_AXI_TUNNEL_1,		0x7},	/* 0x7000.0000 */
	{FPGA_SLV_AXI_TUNNEL_1,		0x8},	/* 0x8000.0000 */
	{FPGA_SLV_AXI_TUNNEL_1,		0x9},	/* 0x9000.0000 */
	{FPGA_SLV_AXI_TUNNEL_1,		0xA},	/* 0xA000.0000 */
	{FPGA_SLV_AXI_TUNNEL_1,		0xB},	/* 0xB000.0000 */
	{FPGA_SLV_AXI_TUNNEL_1,		0xC},	/* 0xC000.0000 */
	{FPGA_SLV_AXI_TUNNEL_1,		0xD},	/* 0xD000.0000 */
	{FPGA_SLV_CONTROL,		0x0},	/* 0xE000.0000 */
	{FPGA_SLV_AXI_TUNNEL_1,		0xF},	/* 0xF000.0000 */
};

/*
 * base + 0x00 : slave select (low 32 bits)
 * base + 0x04 : slave select (high 32 bits)
 * base + 0x08 : slave offset (low 32 bits)
 * base + 0x0C : slave offset (high 32 bits)
 */
static void xplorer770_set_memmap(void __iomem *base, const int memmap[16][2])
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

static void xplorer770_early_init(void)
{
	int i;

	/* ARC 770D memory view */
	xplorer770_set_memmap((void __iomem *) CPUTILE_CREG + 0x20,
			      cputile_memmap);

	iowrite32(1, (void __iomem *) CPUTILE_CREG + 0x34);	/* Update */

	/* AXI tunnel memory view (incoming traffic from FPGA into CPU tile) */
	xplorer770_set_memmap((void __iomem *) CPUTILE_CREG + 0x60,
			      cputile_axi_tunnel_memmap);

	iowrite32(1, (void __iomem *) CPUTILE_CREG + 0x74);	/* Update */

	/* FPGA DMA peripherals memory view
	   (incoming traffic from FPGA peripherals towards FPGA bus) */
	for (i = 0; i <= 10; i++)
		xplorer770_set_memmap((void __iomem *) FPGA_CREG + (i << 4),
				      fpga_memmap);

	iowrite32(0x3ff, (void __iomem *) FPGA_CREG + 0x100); /* Update */

	/* GPIO pins 18 and 19 are used as UART rx and tx, respectively. */
	iowrite32(0x01, (void __iomem *) CPUTILE_CREG + 0x120);

	/* Set up the FPGA interrupt system.*/
	/* FPGA mux interrupts to GPIO7) */
	iowrite32(0x01, (void __iomem *) FPGA_CREG + 0x214);

	/* reset ethernet and ULPI interfaces */
	iowrite32(0x18, (void __iomem *) FPGA_CREG + 0x220);

	/* map GPIO 14:10 to ARC 9:5 (IRQ mux change for rev 2 boards) */
	iowrite32(0x52, (void __iomem *) CPUTILE_CREG + 0x114);
}

static void xplorer770_plat_init(void)
{
	of_clk_init(NULL);
	of_platform_populate(NULL, of_default_bus_match_table, NULL, NULL);
}

MACHINE_START(XPLORER770, "xplorer770")
	.dt_compat	= xplorer770_compat,
	.init_early	= xplorer770_early_init,
	.init_machine	= xplorer770_plat_init,
	.init_irq	= NULL,
MACHINE_END
