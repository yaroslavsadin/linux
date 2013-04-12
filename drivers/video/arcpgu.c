/*
 * Copyright (C) 2013 Synopsys, Inc. (www.synopsys.com)
 *
 *   Mischa Jonker <mjonker@synopsys.com>
 *
 *   arcpgu.c
 *
 *  Simple fb driver with hardcoded 640x480x24 resolution
 *  can be used with nSIM OSCI model
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

struct arcpgu_regs {
	uint32_t	ctrl;
	uint32_t	stat;
	uint32_t	padding1[2];
	uint32_t	fmt;
	uint32_t	hsync;
	uint32_t	vsync;
	uint32_t	frame;
	uint32_t	padding2[8];
	uint32_t	base0;
	uint32_t	base1;
	uint32_t	base2;
	uint32_t	padding3[1];
	uint32_t	stride;
};

struct arcpgu_par {
	struct arcpgu_regs	__iomem *regs;
	void			*fb;
	dma_addr_t		fb_phys;
	unsigned long		fb_size;
};

static struct fb_var_screeninfo arcpgufb_var __initdata = {
	.xres =			640,
	.yres =			480,
	.xoffset =		0,
	.yoffset =		0,
	.xres_virtual =		640,
	.yres_virtual =		480,
	.bits_per_pixel =	24,
	.red =			{.offset = 16, .length = 8},
	.green =		{.offset = 8, .length = 8},
	.blue =			{.offset = 0, .length = 8},
/*	.bits_per_pixel =	16,
	.red =			{.offset = 11, .length = 5},
	.green =		{.offset = 5, .length = 6},
	.blue =			{.offset = 0, .length = 5},*/
};

static struct fb_fix_screeninfo arcpgufb_fix = {
	.id =		"arcpgufb",
	.type =		FB_TYPE_PACKED_PIXELS,
	.visual =	FB_VISUAL_TRUECOLOR,
	.xpanstep =	0,
	.ypanstep =	0,
	.ywrapstep =	0,
	.accel =	FB_ACCEL_NONE,
};

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

	/* initialize HW here */
	iowrite32(par->fb_phys, &par->regs->base0);
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

static struct fb_ops arcpgufb_ops = {
	.owner		= THIS_MODULE,
	.fb_check_var	= arcpgufb_check_var,
	.fb_set_par	= arcpgufb_set_par,
	.fb_setcolreg	= arcpgufb_setcolreg,
	.fb_fillrect	= cfb_fillrect,
	.fb_copyarea	= cfb_copyarea,
	.fb_imageblit	= cfb_imageblit,
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

	info->fbops = &arcpgufb_ops;
	info->fix = arcpgufb_fix;
	info->flags = FBINFO_DEFAULT;

	if (fb_alloc_cmap(&info->cmap, 256, 0)) {
		dev_err(device, "could not allocate cmap\n");
		return -ENOMEM;
	}

	info->var = arcpgufb_var;

	par = info->par;

	par->regs = devm_request_and_ioremap(device, res);
	par->fb_size = info->var.xres * info->var.yres *
		       ((info->var.bits_per_pixel + 7) >> 3);
	par->fb = dma_alloc_coherent(device, PAGE_ALIGN(par->fb_size),
				     &par->fb_phys, GFP_KERNEL);
	if (!par->fb) {
		retval = -ENOMEM;
		goto out2;
	}

	info->pseudo_palette = devm_kzalloc(device, sizeof(uint32_t) * 16,
					    GFP_KERNEL);
	if (!info->pseudo_palette) {
		retval = -ENOMEM;
		goto out;
	}

	info->screen_base = par->fb;
	info->fix.line_length = info->var.xres *
				((info->var.bits_per_pixel + 7) >> 3);
	info->fix.mmio_start = (unsigned long) par->regs;
	info->fix.mmio_len = sizeof(struct arcpgu_regs);
	info->fix.smem_start = par->fb_phys;
	info->fix.smem_len = par->fb_size;

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
