/*
 * Copyright (C) 2013 Synopsys, Inc. (www.synopsys.com)
 *
 *   Mischa Jonker <mjonker@synopsys.com>
 *   Wayne Ren <wren@synopsys.com>
 *
 *   arcpgu.c
 *
 *  Frame buffer driver for the ARC pixel graphics unit, featured in
 *  the nSIM OSCI model and the ARC Xplorer System.
 *
 *  based on: linux/drivers/video/skeletonfb.c
 *
 *  This file is subject to the terms and conditions of the GNU General Public
 *  License v2. See the file COPYING in the main directory of this archive for
 *  more details.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/fb.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/of.h>


/* Register definitions */

#define PGU_CTRL_CONT_MASK      (0x1)
#define PGU_CTRL_ENABLE_MASK    (0x2)
#define PGU_CTRL_FORMAT_MASK    (0x4)

#define PGU_STAT_BUSY_MASK	(0x2)

#define CLK_CFG_REG_CLK_HIGH_MASK 0xff0000
#define GET_CLK_CFG_REG_CLK_HIGH_VAL(x) ((x & CLK_CFG_REG_CLK_HIGH_MASK) >> 16)
#define SET_CLK_CFG_REG_CLK_HIGH_VAL(x, y) ((x & !CLK_CFG_REG_CLK_HIGH_MASK) |\
		((y << 16) & CLK_CFG_REG_CLK_HIGH_MASK))

#define ENCODE_PGU_XY(x, y)	((((x) - 1) << 16) | ((y) - 1))

/*---------------------------------------------------------------------------*/
/* arc_pgu regs*/
struct arc_pgu_regs {
	uint32_t  ctrl;
	uint32_t  stat;
	uint32_t  padding1[2];
	uint32_t  fmt;
	uint32_t  hsync;
	uint32_t  vsync;
	uint32_t  frame;
	uint32_t  padding2[8];
	uint32_t  base0;
	uint32_t  base1;
	uint32_t  base2;
	uint32_t  padding3[1];
	uint32_t  stride;
	uint32_t  padding4[11];
	uint32_t  start_clr;
	uint32_t  start_set;
	uint32_t  padding5[206];
	uint32_t  int_en_clr;
	uint32_t  int_en_set;
	uint32_t  int_en;
	uint32_t  padding6[5];
	uint32_t  int_stat_clr;
	uint32_t  int_stat_set;
	uint32_t  int_stat;
	uint32_t  padding7[4];
	uint32_t  module_id;
};

/* display information */
struct known_displays {
	unsigned char	display_name[256];
	unsigned	hres, hsync_start, hsync_end, htotal;
	unsigned	vres, vsync_start, vsync_end, vtotal;
	bool		hsync_polarity, vsync_polarity;
	int		div;
};

/* parameters for arc pgu */
struct arcpgu_par {
	struct arc_pgu_regs __iomem *regs;
	void *fb;		/* frame buffer's virtual  address */
	dma_addr_t fb_phys;	/* frame buffer's physical address */
	uint32_t fb_size;	/* frame buffer's size */
	struct known_displays *display;	/* current display info */
/* the following comes from arc_pgu2_par */
	int line_length;
	int cmap_len;
	uint32_t main_mode;
	uint32_t overlay_mode;
	int num_rgbbufs;
	int rgb_bufno;
	int main_is_fb;
};


/* screen information */
struct known_displays dw_displays[] = {
	{
		"ADV7511 HDMI Transmitter 640x480@60, pixclk 25",
		640, 656, 720, 816, 480, 481, 484, 516, false, true, 6
	},
	{
		"ADV7511 HDMI Transmitter 1024x576@60, pixclk 50",
		1024, 1064, 1176, 1360, 576, 577, 580, 617, false, true, 3
	},
	{
		"ADV7511 HDMI Transmitter 1280x720@30, pixclk 75",
		1280, 1288, 1416, 1600, 720, 721, 724, 760, false, true, 2
	},
};

/* the screen parameters that can be modified by the user */
static struct fb_var_screeninfo arcpgufb_var = {
	.xres =			1280,
	.yres =			720,
	.xoffset =		0,
	.yoffset =		0,
	.xres_virtual =		1280,
	.yres_virtual =		720,
#ifdef CONFIG_ARCPGU_RGB888
/* RGB888 */
	.bits_per_pixel =	24,
	.red =			{.offset = 16, .length = 8},
	.green =		{.offset = 8, .length = 8},
	.blue =			{.offset = 0, .length = 8},
#else
/* RGB565 */
	.bits_per_pixel =	16,
	.red =			{.offset = 11, .length = 5},
	.green =		{.offset = 5, .length = 6},
	.blue =			{.offset = 0, .length = 5},
#endif
};

/* the screen parameters that is fixed */
static struct fb_fix_screeninfo arcpgufb_fix = {
	.id =		"arcpgufb",
	.type =		FB_TYPE_PACKED_PIXELS,
	.visual =	FB_VISUAL_TRUECOLOR,
	.xpanstep =	0,
	.ypanstep =	0,
	.ywrapstep =	0,
	.accel =	FB_ACCEL_NONE,	/* no hardware acceleration */
};


static void arcpgufb_disable(struct fb_info *info)
{
	unsigned int val;
	struct arcpgu_par *par = info->par;

	val = ioread32(&par->regs->ctrl);
	val &= ~PGU_CTRL_ENABLE_MASK;
	iowrite32(val, &par->regs->ctrl);

	while (ioread32(&par->regs->stat) & PGU_STAT_BUSY_MASK)
		;
}

#if 0
static void arcpgufb_enable(struct fb_info *info)
{
	unsigned int val;
	struct arcpgu_par *par = info->par;

	val = ioread32(&par->regs->ctrl);
	val |= PGU_CTRL_ENABLE_MASK;

	iowrite32(val, &par->regs->ctrl);

	while (ioread32(&par->regs->stat) & PGU_STAT_BUSY_MASK)
		;
}
#endif

static int arcpgufb_check_var(struct fb_var_screeninfo *var,
			      struct fb_info *info)
{
	/* always return OK */
	*var = info->var;
	return 0;
}

static int arcpgufb_set_par(struct fb_info *info)
{
	struct arcpgu_par *par = info->par;

	arcpgufb_disable(info);
	dev_dbg(info->dev, "display name %s, %dx%d\n",
		par->display->display_name,
		par->display->hres, par->display->vres);

	iowrite32(ENCODE_PGU_XY(par->display->htotal, par->display->vtotal),
		  &par->regs->fmt);
	dev_dbg(info->dev, "FORMAT:%x\n", ioread32(&par->regs->fmt));

	iowrite32(ENCODE_PGU_XY(par->display->hsync_start - par->display->hres,
				par->display->hsync_end - par->display->hres),
		  &par->regs->hsync);
	dev_dbg(info->dev, "HSYNC:%x\n", ioread32(&par->regs->hsync));

	iowrite32(ENCODE_PGU_XY(par->display->vsync_start - par->display->vres,
				par->display->vsync_end - par->display->vres),
		  &par->regs->vsync);
	dev_dbg(info->dev, "VSYNC:%x\n", ioread32(&par->regs->hsync));

	iowrite32(ENCODE_PGU_XY(par->display->htotal - par->display->hres,
				par->display->vtotal - par->display->vres),
		  &par->regs->frame);
	dev_dbg(info->dev, "FRAME:%x\n", ioread32(&par->regs->frame));

	iowrite32(par->fb_phys, &par->regs->base0);

	if (par->num_rgbbufs > 1) {
		iowrite32(par->fb_phys + (par->fb_size / par->num_rgbbufs),
		&par->regs->base1); /* base1, double buffer */
	}
	if (par->num_rgbbufs > 2) {
		iowrite32(par->fb_phys + 2 * (par->fb_size / par->num_rgbbufs),
		&par->regs->base2); /* base2, tripple buffer */
	}

	iowrite32(0, &par->regs->stride);	/* stride */

	iowrite32((par->display->div - 1) << 24 |
#ifdef CONFIG_ARCPGU_RGB888
		  0x67,
#else
		  0x63,
#endif
		  &par->regs->ctrl);

	/* start dma transfer for frame buffer 0  */
	iowrite32(1, &par->regs->start_set);
	dev_dbg(info->dev, "CTRL:%x", ioread32(&par->regs->ctrl));
	return 0;
}

/* This function is required for correct operation of frame buffer console */
static int arcpgufb_setcolreg(unsigned regno, unsigned red, unsigned green,
			   unsigned blue, unsigned transp,
			   struct fb_info *info)
{
	uint32_t v;

	if (regno >= 16)
		return -EINVAL;

	/* rgb 565 */
	red = red	>> (16 - info->var.red.length);
	green = green	>> (16 - info->var.green.length);
	blue = blue	>> (16 - info->var.blue.length);
	transp = transp	>> (16 - info->var.transp.length);

	v = (red    << info->var.red.offset)   |
	    (green  << info->var.green.offset) |
	    (blue   << info->var.blue.offset)  |
	    (transp << info->var.transp.offset);

	((uint32_t *)(info->pseudo_palette))[regno] = v;

	return 0;
}

static int arcpgufb_mmap(struct fb_info *info, struct vm_area_struct *vma)
{
	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
	return vm_iomap_memory(vma, info->fix.smem_start, info->fix.smem_len);
}

static struct fb_ops arcpgufb_ops = {
	.owner		= THIS_MODULE,
	.fb_check_var	= arcpgufb_check_var,
	.fb_set_par	= arcpgufb_set_par,
	.fb_setcolreg	= arcpgufb_setcolreg,
	.fb_fillrect	= cfb_fillrect,
	.fb_copyarea	= cfb_copyarea,
	.fb_imageblit	= cfb_imageblit,
	.fb_mmap	= arcpgufb_mmap,
};

/* ------------------------------------------------------------------------- */

static int arcpgufb_probe(struct platform_device *pdev)
{
	struct fb_info *info;
	struct arcpgu_par *par;
	struct device *device = &pdev->dev;
	struct resource *res;
	int retval;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(device, "no IO memory defined\n");
		return -EINVAL;
	}

	info = framebuffer_alloc(sizeof(struct arcpgu_par), device);
	if (!info) {
		dev_err(device, "could not allocate framebuffer\n");
		return -ENOMEM;
	}

	par = info->par;
/*	get config display info, here use retval as index */
	retval = CONFIG_ARCPGU_DISPTYPE;
/* check retval ?*/
	par->display = &dw_displays[retval];

	/* var setting according to display */
	/* only works for 8/16 bpp */
	par->num_rgbbufs = CONFIG_ARCPGU_RGBBUFS;
	par->line_length = par->display->hres *
		       ((arcpgufb_var.bits_per_pixel + 7) >> 3);
	par->main_mode = 1;
	par->overlay_mode = 1;
	par->rgb_bufno = 0;
	par->main_is_fb = 1;
	par->cmap_len = 16;

	par->regs = devm_ioremap_resource(device, res);
	if (IS_ERR(par->regs)) {
		dev_err(device, "Could not remap IO mem\n");
		return PTR_ERR(par->regs);
	}

	dev_info(device, "arc_pgu ID# 0x%x, using the: %s\n",
		ioread32(&par->regs->module_id), par->display->display_name);

	par->fb_size = CONFIG_ARCPGU_RGBBUFS * par->display->hres *
		       par->display->vres *
		       ((arcpgufb_var.bits_per_pixel + 7) >> 3);
	dev_dbg(device, "fb size:%x\n", par->fb_size);
	par->fb = dma_alloc_coherent(device, PAGE_ALIGN(par->fb_size),
				     &par->fb_phys, GFP_KERNEL);
	if (!par->fb) {
		retval = -ENOMEM;
		goto out2;
	}

	dev_dbg(device, "framebuffer at: 0x%p (logical), 0x%x (physical)\n",
		par->fb, par->fb_phys);

	arcpgufb_var.xres = par->display->hres;
	arcpgufb_var.yres = par->display->vres;
	arcpgufb_var.xres_virtual = par->display->hres;
	arcpgufb_var.yres_virtual = par->display->vres;

	/* encode_fix */
	arcpgufb_fix.smem_start = par->fb_phys;
	arcpgufb_fix.smem_len = par->fb_size;
	arcpgufb_fix.mmio_start = (uint32_t)par->regs;
	arcpgufb_fix.mmio_len = sizeof(struct arc_pgu_regs);
	arcpgufb_fix.visual = (arcpgufb_var.bits_per_pixel == 8) ?
		FB_VISUAL_PSEUDOCOLOR : FB_VISUAL_TRUECOLOR;
	arcpgufb_fix.line_length = par->line_length;

/* info setting */
	info->pseudo_palette = devm_kzalloc(device, sizeof(uint32_t) * 16,
					    GFP_KERNEL);
	if (!info->pseudo_palette) {
		retval = -ENOMEM;
		goto out;
	}
	memset(info->pseudo_palette, 0, sizeof(uint32_t) * 16);

	info->fbops = &arcpgufb_ops;
	info->flags = FBINFO_DEFAULT;

	if (fb_alloc_cmap(&info->cmap, 256, 0)) {
		dev_err(device, "could not allocate cmap\n");
		retval = -ENOMEM;
		goto out;
	}

	info->screen_base = par->fb;

	info->fix = arcpgufb_fix;
	info->var = arcpgufb_var;

	if (register_framebuffer(info) < 0) {
		retval = -EINVAL;
		goto out;
	}

	platform_set_drvdata(pdev, info);
	return 0;

out:
	dma_free_coherent(device, PAGE_ALIGN(par->fb_size),
			  par->fb, par->fb_size);
out2:
	fb_dealloc_cmap(&info->cmap);
	return retval;
}


static int arcpgufb_remove(struct platform_device *pdev)
{
	struct fb_info *info = platform_get_drvdata(pdev);
	struct arcpgu_par *par = info->par;

	if (info) {
		unregister_framebuffer(info);
		dma_free_coherent(&pdev->dev, PAGE_ALIGN(par->fb_size),
				  par->fb, par->fb_size);
		fb_dealloc_cmap(&info->cmap);
		framebuffer_release(info);
	}

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id arcpgufb_match[] = {
	{ .compatible = "snps,arcpgufb" },
	{},
};
#endif

static struct platform_driver arcpgufb_driver = {
	.probe = arcpgufb_probe,
	.remove = arcpgufb_remove,
	.driver = {
		.name = "arcpgufb",
		.of_match_table = of_match_ptr(arcpgufb_match),
	},
};

static int __init arcpgufb_init(void)
{
	return platform_driver_register(&arcpgufb_driver);
}

static void __exit arcpgufb_exit(void)
{
	platform_driver_unregister(&arcpgufb_driver);
}

/* ------------------------------------------------------------------------- */

module_init(arcpgufb_init);
module_exit(arcpgufb_exit);

MODULE_LICENSE("GPL");
