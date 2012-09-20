/* drivers/media/video/s5k4ecgx_regset.h
 *
 * Driver for s5k4ecgx (5MP Camera) from SEC(LSI), firmware EVT1.1
 *
 * Copyright (C) 2012, Insignal Co,. Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef __S5K4ECGX_REGSET_H__
#define __S5K4ECGX_REGSET_H__

enum s5k4ecgx_reg_type {
	S5K4ECGX_REGTYPE_END = 0,
	S5K4ECGX_REGTYPE_CMD,
	S5K4ECGX_REGTYPE_WRITE,
	S5K4ECGX_REGTYPE_READ,
	S5K4ECGX_REGTYPE_DELAY,
};

struct s5k4ecgx_reg {
	enum s5k4ecgx_reg_type type;
	int data_len;
	union {
		u32 addr;
		int msec;
	};
};

#endif
