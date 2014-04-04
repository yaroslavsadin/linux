/*
 * Copyright (C) 2014 Synopsys, Inc. (www.synopsys.com)
 *
 * Driver for NAND controller on Synopsys AXS development board.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License v2. See the file COPYING in the main directory of this archive for
 * more details.
 */

#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/partitions.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/io.h>

/*
 * There's an issue with DMA'd data if data buffer is cached.
 * So to make NAND storage available for now we'll map data buffer in
 * uncached memory.
 *
 * As soon as issue with cached buffer is resolved following define to be
 * removed as well as sources it enables.
 */
#define DATA_BUFFER_UNCACHED

#define BUS_WIDTH	8		/* AXI data bus width in bytes	*/

/* DMA buffer descriptor masks */
#define BD_STAT_OWN			(1 << 31)
#define BD_STAT_BD_FIRST		(1 << 3)
#define BD_STAT_BD_LAST			(1 << 2)
#define BD_SIZES_BUFFER1_MASK		0xfff

#define BD_STAT_BD_COMPLETE	(BD_STAT_BD_FIRST | BD_STAT_BD_LAST)

/* Controller command types */
#define B_CT_ADDRESS	(0x0 << 16)	/* Address operation		*/
#define B_CT_COMMAND	(0x1 << 16)	/* Command operation		*/
#define B_CT_WRITE	(0x2 << 16)	/* Write operation		*/
#define B_CT_READ	(0x3 << 16)	/* Read operation		*/

/* Controller command options */
#define B_WFR		(1 << 19)	/* 1b - Wait for ready		*/
#define B_LC		(1 << 18)	/* 1b - Last cycle		*/
#define B_IWC		(1 << 13)	/* 1b - Interrupt when complete	*/

enum {
	NAND_ISR_DATAREQUIRED = 0,
	NAND_ISR_TXUNDERFLOW,
	NAND_ISR_TXOVERFLOW,
	NAND_ISR_DATAAVAILABLE,
	NAND_ISR_RXUNDERFLOW,
	NAND_ISR_RXOVERFLOW,
	NAND_ISR_TXDMACOMPLETE,
	NAND_ISR_RXDMACOMPLETE,
	NAND_ISR_DESCRIPTORUNAVAILABLE,
	NAND_ISR_CMDDONE,
	NAND_ISR_CMDAVAILABLE,
	NAND_ISR_CMDERROR,
	NAND_ISR_DATATRANSFEROVER,
	NAND_ISR_NONE
};

enum {
	AC_FIFO = 0,		/* Address and command fifo */
	IDMAC_BDADDR = 0x18,	/* IDMAC descriptor list base address */
	INT_STATUS = 0x118,	/* Interrupt status register */
	INT_CLR_STATUS = 0x120	/* Interrupt clear status register */
};

struct asx_nand_bd {
	__le32 status;		/* DES0 */
	__le32 sizes;		/* DES1 */
	dma_addr_t buffer_ptr0;	/* DES2 */
	dma_addr_t buffer_ptr1;	/* DES3 */
};

struct axs_nand_host {
	struct nand_chip	nand_chip;
	struct mtd_info		mtd;
	void __iomem		*io_base;
	struct device		*dev;
	struct asx_nand_bd	*bd;	/* Buffer descriptor */
	dma_addr_t		bd_dma;	/* DMA handle for buffer descriptor */
	uint8_t		*db;	/* Data buffer */
	dma_addr_t		db_dma;	/* DMA handle for data buffer */
};

/**
 * reg_set - Sets register with provided value.
 * @host:	Pointer to private data structure.
 * @reg:	Register offset from base address.
 * @value:	Value to set in register.
 */
static inline void reg_set(struct axs_nand_host *host, int reg, int value)
{
	iowrite32(value, host->io_base + reg);
}

/**
 * reg_get - Gets value of specified register.
 * @host:	Pointer to private data structure.
 * @reg:	Register offset from base address.
 *
 * returns:	Value of requested register.
 */
static inline unsigned int reg_get(struct axs_nand_host *host, int reg)
{
	return ioread32(host->io_base + reg);
}

/* Maximum number of milliseconds we wait for flag to appear */
#define AXS_FLAG_WAIT_DELAY	1000

/**
 * axs_flag_wait_and_reset - Waits until requested flag in INT_STATUS register
 *              is set by HW and resets it by writing "1" in INT_CLR_STATUS.
 * @host:	Pointer to private data structure.
 * @flag:	Bit/flag offset in INT_STATUS register
 */
static void axs_flag_wait_and_reset(struct axs_nand_host *host, int flag)
{
	unsigned int i;

	for (i = 0; i < AXS_FLAG_WAIT_DELAY * 100; i++) {
		unsigned int status = reg_get(host, INT_STATUS);

		if (status & (1 << flag)) {
			reg_set(host, INT_CLR_STATUS, 1 << flag);
			return;
		}

		udelay(10);
	}

	/*
	 * Since we cannot report this problem any further than
	 * axs_nand_{write|read}_buf() letting user know there's a problem.
	 */
	dev_err(host->dev, "Waited too long (%d s.) for flag/bit %d\n",
		AXS_FLAG_WAIT_DELAY, flag);
}

/**
 * axs_nand_write_buf - write buffer to chip
 * @mtd:	MTD device structure
 * @buf:	Data buffer
 * @len:	Number of bytes to write
 */
static void axs_nand_write_buf(struct mtd_info *mtd,
		const uint8_t *buf, int len)
{
	struct nand_chip *this = mtd->priv;
	struct axs_nand_host *host = this->priv;

	memcpy(host->db, buf, len);
#ifndef DATA_BUFFER_UNCACHED
	dma_sync_single_for_device(host->dev, host->db_dma, len, DMA_TO_DEVICE);
#endif

	/* Setup buffer descriptor */
	host->bd->status = BD_STAT_OWN | BD_STAT_BD_COMPLETE;
	host->bd->sizes = cpu_to_le32(ALIGN(len, BUS_WIDTH) &
				      BD_SIZES_BUFFER1_MASK);
	host->bd->buffer_ptr0 = cpu_to_le32(host->db_dma);
	host->bd->buffer_ptr1 = 0;

	/* Issue "write" command */
	reg_set(host, AC_FIFO, B_CT_WRITE | B_WFR | B_IWC | B_LC | (len - 1));

	/* Wait for NAND command and DMA to complete */
	axs_flag_wait_and_reset(host, NAND_ISR_CMDDONE);
	axs_flag_wait_and_reset(host, NAND_ISR_TXDMACOMPLETE);
}

/**
 * axs_nand_read_buf -  read chip data into buffer
 * @mtd:	MTD device structure
 * @buf:	Buffer to store data
 * @len:	Number of bytes to read
 */
static void axs_nand_read_buf(struct mtd_info *mtd, uint8_t *buf, int len)
{
	struct nand_chip *this = mtd->priv;
	struct axs_nand_host *host = this->priv;

	/* Setup buffer descriptor */
	host->bd->status = BD_STAT_OWN | BD_STAT_BD_COMPLETE;
	host->bd->sizes = cpu_to_le32(ALIGN(len, BUS_WIDTH) &
				      BD_SIZES_BUFFER1_MASK);
	host->bd->buffer_ptr0 = cpu_to_le32(host->db_dma);
	host->bd->buffer_ptr1 = 0;

	/* Issue "read" command */
	reg_set(host, AC_FIFO, B_CT_READ | B_WFR | B_IWC | B_LC | (len - 1));

	/* Wait for NAND command and DMA to complete */
	axs_flag_wait_and_reset(host, NAND_ISR_CMDDONE);
	axs_flag_wait_and_reset(host, NAND_ISR_RXDMACOMPLETE);

#ifndef DATA_BUFFER_UNCACHED
	dma_sync_single_for_cpu(host->dev, host->db_dma, len, DMA_FROM_DEVICE);
#endif
	memcpy(buf, host->db, len);
}

/**
 * axs_nand_read_byte - read one byte from the chip
 * @mtd:	MTD device structure
 *
 * returns:	read data byte
 */
static uint8_t axs_nand_read_byte(struct mtd_info *mtd)
{
	uint8_t buffer;

	axs_nand_read_buf(mtd, (uint8_t *)&buffer, sizeof(buffer));
	return buffer;
}

/**
 * axs_nand_read_word - read one word from the chip
 * @mtd:	MTD device structure
 *
 * returns:	read data word
 */
static uint16_t axs_nand_read_word(struct mtd_info *mtd)
{
	uint16_t buffer;

	axs_nand_read_buf(mtd, (uint8_t *)&buffer, sizeof(buffer));
	return buffer;
}

/**
 * axs_nand_cmd_ctrl - hardware specific access to control-lines
 * @mtd:	MTD device structure
 * @cmd:	NAND command
 * @ctrl:	NAND control options
 */
static void axs_nand_cmd_ctrl(struct mtd_info *mtd, int cmd,
		unsigned int ctrl)
{
	struct nand_chip *nand_chip = mtd->priv;
	struct axs_nand_host *host = nand_chip->priv;

	if (cmd == NAND_CMD_NONE)
		return;

	cmd = cmd & 0xff;

	switch (ctrl & (NAND_ALE | NAND_CLE)) {
	/* Address */
	case NAND_ALE:
		cmd |= B_CT_ADDRESS;
		break;

	/* Command */
	case NAND_CLE:
		cmd |= B_CT_COMMAND | B_WFR;

		break;

	default:
		dev_err(host->dev, "Unknown ctrl %#x\n", ctrl);
		return;
	}

	reg_set(host, AC_FIFO, cmd | B_LC);
	axs_flag_wait_and_reset(host, NAND_ISR_CMDDONE);
}

static int axs_nand_probe(struct platform_device *pdev)
{
	struct mtd_part_parser_data ppdata;
	struct nand_chip *nand_chip;
	struct axs_nand_host *host;
	struct resource *res_regs;
	struct mtd_info *mtd;
	int err;

	host = devm_kzalloc(&pdev->dev, sizeof(*host), GFP_KERNEL);
	if (!host) {
		dev_err(&pdev->dev, "Failed to allocate device structure.\n");
		return -ENOMEM;
	}

	res_regs = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	host->io_base = devm_ioremap_resource(&pdev->dev, res_regs);
	if (IS_ERR(host->io_base))
		return PTR_ERR(host->io_base);

	dev_dbg(&pdev->dev, "Control registers: %pr\n", res_regs);

	host->bd = dmam_alloc_coherent(&pdev->dev, sizeof(*host->bd),
				       &host->bd_dma, GFP_KERNEL);
	if (!host->bd) {
		dev_err(&pdev->dev, "Failed to allocate buffer descriptor\n");
		return -ENOMEM;
	}

	/* Temporary allocate data buffer for reading NAND parameters */
	host->db = 
#ifdef DATA_BUFFER_UNCACHED
		   dmam_alloc_coherent
#else
		   dmam_alloc_noncoherent
#endif
				      (&pdev->dev, PAGE_SIZE, &host->db_dma,
				       GFP_KERNEL);
	if (!host->db) {
		dev_err(&pdev->dev, "Failed to allocate data buffer\n");
		return -ENOMEM;
	}
	dev_dbg(&pdev->dev, "Data buffer mapped @ %p, DMA @ %pad\n", host->db,
		host->db_dma);

	mtd = &host->mtd;
	nand_chip = &host->nand_chip;
	host->dev = &pdev->dev;

	nand_chip->priv = host;
	mtd->priv = nand_chip;
	mtd->name = dev_name(&pdev->dev);
	mtd->owner = THIS_MODULE;
	mtd->dev.parent = &pdev->dev;
	ppdata.of_node = pdev->dev.of_node;

	nand_chip->cmd_ctrl = axs_nand_cmd_ctrl;
	nand_chip->read_byte = axs_nand_read_byte;
	nand_chip->read_word = axs_nand_read_word;
	nand_chip->write_buf = axs_nand_write_buf;
	nand_chip->read_buf = axs_nand_read_buf;
	nand_chip->ecc.mode = NAND_ECC_SOFT;

	dev_set_drvdata(&pdev->dev, host);

	reg_set(host, IDMAC_BDADDR, host->bd_dma);

	err = nand_scan(mtd, 1);
	if (err)
		return err;

	/* Free temporary allocted data buffer and ... */
#ifdef DATA_BUFFER_UNCACHED
	dmam_free_coherent
#else
	dmam_free_noncoherent
#endif
			   (&pdev->dev, PAGE_SIZE, host->db, host->db_dma);

	/* ... allocate data buffer according to read NAND parameters */
	host->db = 
#ifdef DATA_BUFFER_UNCACHED
	dmam_alloc_coherent
#else
	dmam_alloc_noncoherent
#endif
			    (&pdev->dev, mtd->writesize + mtd->oobsize,
			     &host->db_dma, GFP_KERNEL);
	if (!host->db) {
		dev_err(&pdev->dev, "Failed to allocate data buffer\n");
		return -ENOMEM;
	}

	err = mtd_device_parse_register(mtd, NULL, &ppdata, NULL, 0);
	if (err) {
		nand_release(mtd);
		return err;
	}

	return 0;
}

static int axs_nand_remove(struct platform_device *ofdev)
{
	struct axs_nand_host *host = dev_get_drvdata(&ofdev->dev);
	struct mtd_info *mtd = &host->mtd;

	nand_release(mtd);

	return 0;
}

static const struct of_device_id axs_nand_dt_ids[] = {
	{ .compatible = "snps,axs-nand", },
	{ /* Sentinel */ }
};

MODULE_DEVICE_TABLE(of, axs_nand_match);

static struct platform_driver axs_nand_driver = {
	.probe		= axs_nand_probe,
	.remove		= axs_nand_remove,
	.driver = {
		.name = "asx-nand",
		.owner = THIS_MODULE,
		.of_match_table = axs_nand_dt_ids,
	},
};

module_platform_driver(axs_nand_driver);

MODULE_AUTHOR("Alexey Brodkin <abrodkin@synopsys.com>");
MODULE_DESCRIPTION("NAND driver for Synopsys AXS development board");
MODULE_LICENSE("GPL");
