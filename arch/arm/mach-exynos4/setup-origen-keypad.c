/* linux/arch/arm/mach-exynos4/setup-origen-keypad.c
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * GPIO configuration for Origen KeyPad device
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/gpio.h>

#include <plat/gpio-cfg.h>
#include <plat/origen-keypad.h>

void origen_keypad_cfg_gpio(void)
{
	/* Set all the necessary GPX1 and GPX2 pins to special function 0 */
	s3c_gpio_cfgrange_nopull(EXYNOS4_GPX1(5), 3, S3C_GPIO_SFN(0));
	s3c_gpio_cfgrange_nopull(EXYNOS4_GPX2(0), 2, S3C_GPIO_SFN(0));
}
