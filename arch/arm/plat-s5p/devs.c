/* linux/arch/arm/plat-s5p/devs.c
 *
 * Copyright (c) 2010 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * Base S5P platform device definitions
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/list.h>
#include <linux/timer.h>
#include <linux/init.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <linux/io.h>

#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/mach/irq.h>
#include <mach/hardware.h>
#include <mach/map.h>
#include <mach/dma.h>

#include <plat/devs.h>
#include <plat/gpio-cfg.h>
#include <plat/irqs.h>
#include <plat/fimg2d.h>

#ifdef CONFIG_VIDEO_FIMG2D
static struct resource s5p_fimg2d_resource[] = {
	[0] = {
		.start	= S5P_PA_2D,
		.end	= S5P_PA_2D + S5P_SZ_2D - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= IRQ_2D,
		.end	= IRQ_2D,
		.flags	= IORESOURCE_IRQ,
	}
};

struct platform_device s5p_device_fimg2d = {
	.name		= "s5p-fimg2d",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(s5p_fimg2d_resource),
	.resource	= s5p_fimg2d_resource
};
EXPORT_SYMBOL(s5p_device_fimg2d);

static struct fimg2d_platdata default_fimg2d_data __initdata = {
	.parent_clkname = "mout_g2d0",
	.clkname = "sclk_fimg2d",
	.gate_clkname = "fimg2d",
	.clkrate = 250 * 1000000,
};
void __init s5p_fimg2d_set_platdata(struct fimg2d_platdata *pd)
{
	struct fimg2d_platdata *npd;

	if (!pd)
		pd = &default_fimg2d_data;

	npd = kmemdup(pd, sizeof(*pd), GFP_KERNEL);
	if (!npd)
		pr_err("no memory for fimg2d platform data\n");
	else
		s5p_device_fimg2d.dev.platform_data = npd;
}
#endif
