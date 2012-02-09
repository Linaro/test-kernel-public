/* arch/arm/mach-exynos4/include/mach/regs-audss.h
 *
 * Copyright (c) 2011 Samsung Electronics
 *		http://www.samsung.com
 *
 * Exynos4 Audio SubSystem clock register definitions
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef __PLAT_REGS_AUDSS_H
#define __PLAT_REGS_AUDSS_H __FILE__

#define EXYNOS4_AUDSS_INT_MEM	(0x03000000)

#define S5P_AUDSSREG(x)		(S5P_VA_AUDSS + (x))

#define S5P_CLKSRC_AUDSS	S5P_AUDSSREG(0x0)
#define S5P_CLKDIV_AUDSS	S5P_AUDSSREG(0x4)
#define S5P_CLKGATE_AUDSS	S5P_AUDSSREG(0x8)

#define S5P_AUDSS_CLKGATE_I2SBUS	(1<<2)

#endif /* _PLAT_REGS_AUDSS_H */
