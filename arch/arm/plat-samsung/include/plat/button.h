/*
 * Origen keypad platform data structure
 *
 * Copyright (C) 2011 Samsung Electronics Co.Ltd
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#ifndef __ORIGEN_BUTTON__
#define __ORIGEN_BUTTON__

struct origen_button_data {
	int (*cfg_gpio)(int enable);
};

#endif

