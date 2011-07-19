/*
 * Origen keypad platform data structures
 *
 * Copyright (C) 2011 Samsung Electronics Co.Ltd
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/gpio.h>

#include <plat/gpio-cfg.h>
#include <plat/button.h>

#include <mach/map.h>
#include <mach/dma.h>
#include <mach/irqs.h>

static int origen_cfg_button(int enable)
{
	int i, err;
	switch(enable) {
		case 1:
			for(i = 5; i < 8; i++){
				err = gpio_request(EXYNOS4_GPX1(i), "GPX1");
				if (err) {
					printk(KERN_INFO "gpio request error : %d\n", err);
					goto error;
				}
			}

			for(i = 0; i < 2; i++){
				err = gpio_request(EXYNOS4_GPX2(i), "GPX2");
				if (err) {
					printk(KERN_INFO "gpio request error : %d\n", err);
					goto error;
				}
			}
			s3c_gpio_cfgrange_nopull(EXYNOS4_GPX1(5), 3, 0xF);
			s3c_gpio_cfgrange_nopull(EXYNOS4_GPX2(0), 2, 0xF);
			return 0;
			error:
			return -1;
	case 2:
			for(i = 5; i < 8; i++)
				gpio_free(EXYNOS4_GPX1(5));
			for(i = 0; i < 2; i++)
				gpio_free(EXYNOS4_GPX2(0));
			return 0;
	}
	return -1;
}

static struct origen_button_data origen_button_pdata = {
    .cfg_gpio = origen_cfg_button,
};

static struct resource origen_button_resource[] = {
	{
		.start = IRQ_EINT(13),
		.end = IRQ_EINT(17),
		.flags = IORESOURCE_IRQ,
	},
};

struct platform_device origen_device_button = {
	.name = "origen-button",
	.id = 0,
	.num_resources = ARRAY_SIZE(origen_button_resource),
	.resource = origen_button_resource,
	.dev = {
		.platform_data = &origen_button_pdata,
	},
};

