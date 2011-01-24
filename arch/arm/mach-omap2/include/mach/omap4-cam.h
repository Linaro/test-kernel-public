#ifndef __OMAP4_CAM_H__
#define __OMAP4_CAM_H__

#include <linux/platform_device.h>
#include <plat/omap44xx.h>

#define OMAP44XX_ISS_TOP_SIZE				256
#define OMAP44XX_ISS_CSI2_A_REGS1_SIZE			368
#define OMAP44XX_ISS_CAMERARX_CORE1_SIZE		32

static struct resource omap4_camera_resources[] = {
	{
		.start		= OMAP44XX_ISS_TOP_BASE,
		.end		= OMAP44XX_ISS_TOP_BASE +
				  OMAP44XX_ISS_TOP_SIZE - 1,
		.flags		= IORESOURCE_MEM,
	},
	{
		.start		= OMAP44XX_ISS_CSI2_A_REGS1_BASE,
		.end		= OMAP44XX_ISS_CSI2_A_REGS1_BASE +
				  OMAP44XX_ISS_CSI2_A_REGS1_SIZE - 1,
		.flags		= IORESOURCE_MEM,
	},
	{
		.start		= OMAP44XX_ISS_CAMERARX_CORE1_BASE,
		.end		= OMAP44XX_ISS_CAMERARX_CORE1_BASE +
				  OMAP44XX_ISS_CAMERARX_CORE1_SIZE - 1,
		.flags		= IORESOURCE_MEM,
	},
	{
		.start		= OMAP44XX_IRQ_ISS_5,
		.flags		= IORESOURCE_IRQ,
	},
};

#endif
