/*
 * Keypad platform data definitions for Insignal's Origen board.
 *
 * Copyright (C) 2011 Samsung Electronics Co.Ltd
 * Author: Sachin Kamat <sachin.kamat@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#ifndef __PLAT_ORIGEN_KEYPAD_H
#define __PLAT_ORIGEN_KEYPAD_H

#include <mach/map.h>

#define EXYNOS4_GPX1DAT		(S5P_VA_GPIO2 + 0x0c24)
#define EXYNOS4_GPX2DAT		(S5P_VA_GPIO2 + 0x0c44)

#define IRQ_KEY_MENU		EINT_NUMBER(13)
#define IRQ_KEY_HOME		EINT_NUMBER(14)
#define IRQ_KEY_BACK		EINT_NUMBER(15)
#define IRQ_KEY_VOLUMEUP	EINT_NUMBER(16)
#define IRQ_KEY_VOLUMEDOWN	EINT_NUMBER(17)

/**
 * struct samsung_keypad_platdata - Platform device data for Samsung Keypad.
 * @cfg_gpio: configure the GPIO.
 *
 * Initialisation data specific to either the machine or the platform
 * for the device driver to use or call-back when configuring gpio.
 */
struct origen_keypad_platdata {
	void (*cfg_gpio)(void);
};

/* defined by architecture to configure gpio. */
extern void origen_keypad_cfg_gpio(void);

#endif /* __PLAT_ORIGEN_KEYPAD_H */
