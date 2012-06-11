/*
 * OMAP4 CM1, CM2 module low-level functions
 *
 * Copyright (C) 2010 Nokia Corporation
 * Paul Walmsley
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * These functions are intended to be used only by the cminst44xx.c file.
 * XXX Perhaps we should just move them there and make them static.
 */

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/io.h>

#include "iomap.h"
#include "common.h"
#include "cm.h"
#include "cm44xx.h"
#include "cm1_44xx.h"
#include "cm2_44xx.h"
#include "cminst44xx.h"
#include "prcm44xx.h"
#include "cm-regbits-44xx.h"
#include "cm2_54xx.h"

/* CM1 hardware module low-level functions */

/* Read a register in CM1 */
u32 omap4_cm1_read_inst_reg(s16 inst, u16 reg)
{
	return __raw_readl(OMAP44XX_CM1_REGADDR(inst, reg));
}

/* Write into a register in CM1 */
void omap4_cm1_write_inst_reg(u32 val, s16 inst, u16 reg)
{
	__raw_writel(val, OMAP44XX_CM1_REGADDR(inst, reg));
}

/* Read a register in CM2 */
u32 omap4_cm2_read_inst_reg(s16 inst, u16 reg)
{
	return __raw_readl(OMAP44XX_CM2_REGADDR(inst, reg));
}

/* Write into a register in CM2 */
void omap4_cm2_write_inst_reg(u32 val, s16 inst, u16 reg)
{
	__raw_writel(val, OMAP44XX_CM2_REGADDR(inst, reg));
}

#define MAX_CM_REGISTERS 51

struct omap4_cm_reg {
	u16 offset;
	u32 val;
};

struct omap4_cm_regs {
	u32 mod_off;
	u32 no_reg;
	struct omap4_cm_reg reg[MAX_CM_REGISTERS];
};

static struct omap4_cm_regs omap4_cm1_regs[] = {
	/* OMAP4430_CM1_OCP_SOCKET_MOD */
	{ .mod_off = OMAP4430_CM1_OCP_SOCKET_INST, .no_reg = 1,
	{{.offset = OMAP4_CM_CM1_PROFILING_CLKCTRL_OFFSET} },
	},
	/* OMAP4430_CM1_CKGEN_MOD */
	{ .mod_off = OMAP4430_CM1_CKGEN_INST, .no_reg = 4,
	{{.offset = OMAP4_CM_CLKSEL_CORE_OFFSET},
	 {.offset = OMAP4_CM_CLKSEL_ABE_OFFSET},
	 {.offset = OMAP4_CM_DLL_CTRL_OFFSET},
	 {.offset = OMAP4_CM_DYN_DEP_PRESCAL_OFFSET} },
	},
	/* OMAP4430_CM1_MPU_MOD */
	{ .mod_off = OMAP4430_CM1_MPU_INST, .no_reg = 4,
	{{.offset = OMAP4_CM_MPU_CLKSTCTRL_OFFSET},
	 {.offset = OMAP4_CM_MPU_STATICDEP_OFFSET},
	 {.offset = OMAP4_CM_MPU_DYNAMICDEP_OFFSET},
	 {.offset = OMAP4_CM_MPU_MPU_CLKCTRL_OFFSET} },
	},
	/* OMAP4430_CM1_TESLA_MOD */
	{ .mod_off = OMAP4430_CM1_TESLA_INST, .no_reg = 4,
	{{.offset = OMAP4_CM_TESLA_CLKSTCTRL_OFFSET},
	 {.offset = OMAP4_CM_TESLA_STATICDEP_OFFSET},
	 {.offset = OMAP4_CM_TESLA_DYNAMICDEP_OFFSET},
	 {.offset = OMAP4_CM_TESLA_TESLA_CLKCTRL_OFFSET} },
	},
	/* OMAP4430_CM1_ABE_MOD */
	{ .mod_off = OMAP4430_CM1_ABE_INST, .no_reg = 15,
	{{.offset = OMAP4_CM1_ABE_CLKSTCTRL_OFFSET},
	 {.offset = OMAP4_CM1_ABE_L4ABE_CLKCTRL_OFFSET},
	 {.offset = OMAP4_CM1_ABE_AESS_CLKCTRL_OFFSET},
	 {.offset = OMAP4_CM1_ABE_PDM_CLKCTRL_OFFSET},
	 {.offset = OMAP4_CM1_ABE_DMIC_CLKCTRL_OFFSET},
	 {.offset = OMAP4_CM1_ABE_MCASP_CLKCTRL_OFFSET},
	 {.offset = OMAP4_CM1_ABE_MCBSP1_CLKCTRL_OFFSET},
	 {.offset = OMAP4_CM1_ABE_MCBSP2_CLKCTRL_OFFSET},
	 {.offset = OMAP4_CM1_ABE_MCBSP3_CLKCTRL_OFFSET},
	 {.offset = OMAP4_CM1_ABE_SLIMBUS_CLKCTRL_OFFSET},
	 {.offset = OMAP4_CM1_ABE_TIMER5_CLKCTRL_OFFSET},
	 {.offset = OMAP4_CM1_ABE_TIMER6_CLKCTRL_OFFSET},
	 {.offset = OMAP4_CM1_ABE_TIMER7_CLKCTRL_OFFSET},
	 {.offset = OMAP4_CM1_ABE_TIMER8_CLKCTRL_OFFSET},
	 {.offset = OMAP4_CM1_ABE_WDT3_CLKCTRL_OFFSET} },
	},
};

static struct omap4_cm_regs omap4_cm2_regs[] = {
	/* OMAP4430_CM2_OCP_SOCKET_MOD */
	{.mod_off = OMAP4430_CM2_OCP_SOCKET_INST, .no_reg = 1,
	{{.offset = OMAP4_CM_CM2_PROFILING_CLKCTRL_OFFSET} },
	},
	/* OMAP4430_CM2_CKGEN_MOD */
	{.mod_off = OMAP4430_CM2_CKGEN_INST, .no_reg = 12,
	{{.offset = OMAP4_CM_CLKSEL_DUCATI_ISS_ROOT_OFFSET},
	 {.offset = OMAP4_CM_CLKSEL_USB_60MHZ_OFFSET},
	 {.offset = OMAP4_CM_SCALE_FCLK_OFFSET},
	 {.offset = OMAP4_CM_CORE_DVFS_PERF1_OFFSET},
	 {.offset = OMAP4_CM_CORE_DVFS_PERF2_OFFSET},
	 {.offset = OMAP4_CM_CORE_DVFS_PERF3_OFFSET},
	 {.offset = OMAP4_CM_CORE_DVFS_PERF4_OFFSET},
	 {.offset = OMAP4_CM_CORE_DVFS_CURRENT_OFFSET},
	 {.offset = OMAP4_CM_IVA_DVFS_PERF_TESLA_OFFSET},
	 {.offset = OMAP4_CM_IVA_DVFS_PERF_IVAHD_OFFSET},
	 {.offset = OMAP4_CM_IVA_DVFS_PERF_ABE_OFFSET},
	 {.offset = OMAP4_CM_IVA_DVFS_CURRENT_OFFSET} },
	},
	/* OMAP4430_CM2_ALWAYS_ON_MOD */
	{.mod_off = OMAP4430_CM2_ALWAYS_ON_INST, .no_reg = 6,
	{{.offset = OMAP4_CM_ALWON_CLKSTCTRL_OFFSET},
	 {.offset = OMAP4_CM_ALWON_MDMINTC_CLKCTRL_OFFSET},
	 {.offset = OMAP4_CM_ALWON_SR_MPU_CLKCTRL_OFFSET},
	 {.offset = OMAP4_CM_ALWON_SR_IVA_CLKCTRL_OFFSET},
	 {.offset = OMAP4_CM_ALWON_SR_CORE_CLKCTRL_OFFSET},
	 {.offset = OMAP4_CM_ALWON_USBPHY_CLKCTRL_OFFSET} },
	},
	/* OMAP4430_CM2_CORE_MOD */
	{.mod_off = OMAP4430_CM2_CORE_INST, .no_reg = 41,
	{{.offset = OMAP4_CM_L3_1_CLKSTCTRL_OFFSET},
	 {.offset = OMAP4_CM_L3_1_DYNAMICDEP_OFFSET},
	 {.offset = OMAP4_CM_L3_1_L3_1_CLKCTRL_OFFSET},
	 {.offset = OMAP4_CM_L3_2_CLKSTCTRL_OFFSET},
	 {.offset = OMAP4_CM_L3_2_DYNAMICDEP_OFFSET},
	 {.offset = OMAP4_CM_L3_2_L3_2_CLKCTRL_OFFSET},
	 {.offset = OMAP4_CM_L3_2_GPMC_CLKCTRL_OFFSET},
	 {.offset = OMAP4_CM_L3_2_OCMC_RAM_CLKCTRL_OFFSET},
	 {.offset = OMAP4_CM_DUCATI_CLKSTCTRL_OFFSET},
	 {.offset = OMAP4_CM_DUCATI_STATICDEP_OFFSET},
	 {.offset = OMAP4_CM_DUCATI_DYNAMICDEP_OFFSET},
	 {.offset = OMAP4_CM_DUCATI_DUCATI_CLKCTRL_OFFSET},
	 {.offset = OMAP4_CM_SDMA_CLKSTCTRL_OFFSET},
	 {.offset = OMAP4_CM_SDMA_STATICDEP_OFFSET},
	 {.offset = OMAP4_CM_SDMA_DYNAMICDEP_OFFSET},
	 {.offset = OMAP4_CM_SDMA_SDMA_CLKCTRL_OFFSET},
	 {.offset = OMAP4_CM_MEMIF_CLKSTCTRL_OFFSET},
	 {.offset = OMAP4_CM_MEMIF_DMM_CLKCTRL_OFFSET},
	 {.offset = OMAP4_CM_MEMIF_EMIF_FW_CLKCTRL_OFFSET},
	 {.offset = OMAP4_CM_MEMIF_EMIF_1_CLKCTRL_OFFSET},
	 {.offset = OMAP4_CM_MEMIF_EMIF_2_CLKCTRL_OFFSET},
	 {.offset = OMAP4_CM_MEMIF_DLL_CLKCTRL_OFFSET},
	 {.offset = OMAP4_CM_MEMIF_EMIF_H1_CLKCTRL_OFFSET},
	 {.offset = OMAP4_CM_MEMIF_EMIF_H2_CLKCTRL_OFFSET},
	 {.offset = OMAP4_CM_MEMIF_DLL_H_CLKCTRL_OFFSET},
	 {.offset = OMAP4_CM_D2D_CLKSTCTRL_OFFSET},
	 {.offset = OMAP4_CM_D2D_STATICDEP_OFFSET},
	 {.offset = OMAP4_CM_D2D_DYNAMICDEP_OFFSET},
	 {.offset = OMAP4_CM_D2D_SAD2D_CLKCTRL_OFFSET},
	 {.offset = OMAP4_CM_D2D_MODEM_ICR_CLKCTRL_OFFSET},
	 {.offset = OMAP4_CM_D2D_SAD2D_FW_CLKCTRL_OFFSET},
	 {.offset = OMAP4_CM_L4CFG_CLKSTCTRL_OFFSET},
	 {.offset = OMAP4_CM_L4CFG_DYNAMICDEP_OFFSET},
	 {.offset = OMAP4_CM_L4CFG_L4_CFG_CLKCTRL_OFFSET},
	 {.offset = OMAP4_CM_L4CFG_HW_SEM_CLKCTRL_OFFSET},
	 {.offset = OMAP4_CM_L4CFG_MAILBOX_CLKCTRL_OFFSET},
	 {.offset = OMAP4_CM_L4CFG_SAR_ROM_CLKCTRL_OFFSET},
	 {.offset = OMAP4_CM_L3INSTR_CLKSTCTRL_OFFSET},
	 {.offset = OMAP4_CM_L3INSTR_L3_3_CLKCTRL_OFFSET},
	 {.offset = OMAP4_CM_L3INSTR_L3_INSTR_CLKCTRL_OFFSET},
	 {.offset = OMAP4_CM_L3INSTR_OCP_WP1_CLKCTRL_OFFSET} },
	},
	/* OMAP4430_CM2_IVAHD_MOD */
	{.mod_off = OMAP4430_CM2_IVAHD_INST, .no_reg = 5,
	{{.offset = OMAP4_CM_IVAHD_CLKSTCTRL_OFFSET},
	 {.offset = OMAP4_CM_IVAHD_STATICDEP_OFFSET},
	 {.offset = OMAP4_CM_IVAHD_DYNAMICDEP_OFFSET},
	 {.offset = OMAP4_CM_IVAHD_IVAHD_CLKCTRL_OFFSET},
	 {.offset = OMAP4_CM_IVAHD_SL2_CLKCTRL_OFFSET} },
	},
	/* OMAP4430_CM2_CAM_MOD */
	{.mod_off = OMAP4430_CM2_CAM_INST, .no_reg = 5,
	{{.offset = OMAP4_CM_CAM_CLKSTCTRL_OFFSET},
	 {.offset = OMAP4_CM_CAM_STATICDEP_OFFSET},
	 {.offset = OMAP4_CM_CAM_DYNAMICDEP_OFFSET},
	 {.offset = OMAP4_CM_CAM_ISS_CLKCTRL_OFFSET},
	 {.offset = OMAP4_CM_CAM_FDIF_CLKCTRL_OFFSET} },
	},
	/* OMAP4430_CM2_DSS_MOD */
	{.mod_off = OMAP4430_CM2_DSS_INST, .no_reg = 5,
	{{.offset = OMAP4_CM_DSS_CLKSTCTRL_OFFSET},
	 {.offset = OMAP4_CM_DSS_STATICDEP_OFFSET},
	 {.offset = OMAP4_CM_DSS_DYNAMICDEP_OFFSET},
	 {.offset = OMAP4_CM_DSS_DSS_CLKCTRL_OFFSET},
	 {.offset = OMAP4_CM_DSS_DEISS_CLKCTRL_OFFSET} },
	},
	/* OMAP4430_CM2_GFX_MOD */
	{.mod_off = OMAP4430_CM2_GFX_INST, .no_reg = 4,
	{{.offset = OMAP4_CM_GFX_CLKSTCTRL_OFFSET},
	 {.offset = OMAP4_CM_GFX_STATICDEP_OFFSET},
	 {.offset = OMAP4_CM_GFX_DYNAMICDEP_OFFSET},
	 {.offset = OMAP4_CM_GFX_GFX_CLKCTRL_OFFSET} },
	},
	/* OMAP4430_CM2_L3INIT_MOD */
	{.mod_off = OMAP4430_CM2_L3INIT_INST, .no_reg = 20,
	{{.offset = OMAP4_CM_L3INIT_CLKSTCTRL_OFFSET},
	 {.offset = OMAP4_CM_L3INIT_STATICDEP_OFFSET},
	 {.offset = OMAP4_CM_L3INIT_DYNAMICDEP_OFFSET},
	 {.offset = OMAP4_CM_L3INIT_MMC1_CLKCTRL_OFFSET},
	 {.offset = OMAP4_CM_L3INIT_MMC2_CLKCTRL_OFFSET},
	 {.offset = OMAP4_CM_L3INIT_HSI_CLKCTRL_OFFSET},
	 {.offset = OMAP4_CM_L3INIT_UNIPRO1_CLKCTRL_OFFSET},
	 {.offset = OMAP4_CM_L3INIT_USB_HOST_CLKCTRL_OFFSET},
	 {.offset = OMAP4_CM_L3INIT_USB_OTG_CLKCTRL_OFFSET},
	 {.offset = OMAP4_CM_L3INIT_USB_TLL_CLKCTRL_OFFSET},
	 {.offset = OMAP4_CM_L3INIT_P1500_CLKCTRL_OFFSET},
	 {.offset = OMAP4_CM_L3INIT_EMAC_CLKCTRL_OFFSET},
	 {.offset = OMAP4_CM_L3INIT_SATA_CLKCTRL_OFFSET},
	 {.offset = OMAP4_CM_L3INIT_TPPSS_CLKCTRL_OFFSET},
	 {.offset = OMAP4_CM_L3INIT_PCIESS_CLKCTRL_OFFSET},
	 {.offset = OMAP4_CM_L3INIT_CCPTX_CLKCTRL_OFFSET},
	 {.offset = OMAP4_CM_L3INIT_XHPI_CLKCTRL_OFFSET},
	 {.offset = OMAP4_CM_L3INIT_MMC6_CLKCTRL_OFFSET},
	 {.offset = OMAP4_CM_L3INIT_USB_HOST_FS_CLKCTRL_OFFSET},
	 {.offset = OMAP4_CM_L3INIT_USBPHYOCP2SCP_CLKCTRL_OFFSET} },
	},
	/* OMAP4430_CM2_L4PER_MOD */
	{.mod_off = OMAP4430_CM2_L4PER_INST, .no_reg = 51,
	{{.offset = OMAP4_CM_L4PER_CLKSTCTRL_OFFSET},
	 {.offset = OMAP4_CM_L4PER_DYNAMICDEP_OFFSET},
	 {.offset = OMAP4_CM_L4PER_ADC_CLKCTRL_OFFSET},
	 {.offset = OMAP4_CM_L4PER_DMTIMER10_CLKCTRL_OFFSET},
	 {.offset = OMAP4_CM_L4PER_DMTIMER11_CLKCTRL_OFFSET},
	 {.offset = OMAP4_CM_L4PER_DMTIMER2_CLKCTRL_OFFSET},
	 {.offset = OMAP4_CM_L4PER_DMTIMER3_CLKCTRL_OFFSET},
	 {.offset = OMAP4_CM_L4PER_DMTIMER4_CLKCTRL_OFFSET},
	 {.offset = OMAP4_CM_L4PER_DMTIMER9_CLKCTRL_OFFSET},
	 {.offset = OMAP4_CM_L4PER_ELM_CLKCTRL_OFFSET},
	 {.offset = OMAP4_CM_L4PER_GPIO2_CLKCTRL_OFFSET},
	 {.offset = OMAP4_CM_L4PER_GPIO3_CLKCTRL_OFFSET},
	 {.offset = OMAP4_CM_L4PER_GPIO4_CLKCTRL_OFFSET},
	 {.offset = OMAP4_CM_L4PER_GPIO5_CLKCTRL_OFFSET},
	 {.offset = OMAP4_CM_L4PER_GPIO6_CLKCTRL_OFFSET},
	 {.offset = OMAP4_CM_L4PER_HDQ1W_CLKCTRL_OFFSET},
	 {.offset = OMAP4_CM_L4PER_HECC1_CLKCTRL_OFFSET},
	 {.offset = OMAP4_CM_L4PER_HECC2_CLKCTRL_OFFSET},
	 {.offset = OMAP4_CM_L4PER_I2C1_CLKCTRL_OFFSET},
	 {.offset = OMAP4_CM_L4PER_I2C2_CLKCTRL_OFFSET},
	 {.offset = OMAP4_CM_L4PER_I2C3_CLKCTRL_OFFSET},
	 {.offset = OMAP4_CM_L4PER_I2C4_CLKCTRL_OFFSET},
	 {.offset = OMAP4_CM_L4PER_L4PER_CLKCTRL_OFFSET},
	 {.offset = OMAP4_CM_L4PER_MCASP2_CLKCTRL_OFFSET},
	 {.offset = OMAP4_CM_L4PER_MCASP3_CLKCTRL_OFFSET},
	 {.offset = OMAP4_CM_L4PER_MCBSP4_CLKCTRL_OFFSET},
	 {.offset = OMAP4_CM_L4PER_MGATE_CLKCTRL_OFFSET},
	 {.offset = OMAP4_CM_L4PER_MCSPI1_CLKCTRL_OFFSET},
	 {.offset = OMAP4_CM_L4PER_MCSPI2_CLKCTRL_OFFSET},
	 {.offset = OMAP4_CM_L4PER_MCSPI3_CLKCTRL_OFFSET},
	 {.offset = OMAP4_CM_L4PER_MCSPI4_CLKCTRL_OFFSET},
	 {.offset = OMAP4_CM_L4PER_MMCSD3_CLKCTRL_OFFSET},
	 {.offset = OMAP4_CM_L4PER_MMCSD4_CLKCTRL_OFFSET},
	 {.offset = OMAP4_CM_L4PER_MSPROHG_CLKCTRL_OFFSET},
	 {.offset = OMAP4_CM_L4PER_SLIMBUS2_CLKCTRL_OFFSET},
	 {.offset = OMAP4_CM_L4PER_UART1_CLKCTRL_OFFSET},
	 {.offset = OMAP4_CM_L4PER_UART2_CLKCTRL_OFFSET},
	 {.offset = OMAP4_CM_L4PER_UART3_CLKCTRL_OFFSET},
	 {.offset = OMAP4_CM_L4PER_UART4_CLKCTRL_OFFSET},
	 {.offset = OMAP4_CM_L4PER_MMCSD5_CLKCTRL_OFFSET},
	 {.offset = OMAP4_CM_L4PER_I2C5_CLKCTRL_OFFSET},
	 {.offset = OMAP4_CM_L4SEC_CLKSTCTRL_OFFSET},
	 {.offset = OMAP4_CM_L4SEC_STATICDEP_OFFSET},
	 {.offset = OMAP4_CM_L4SEC_DYNAMICDEP_OFFSET},
	 {.offset = OMAP4_CM_L4SEC_AES1_CLKCTRL_OFFSET},
	 {.offset = OMAP4_CM_L4SEC_AES2_CLKCTRL_OFFSET},
	 {.offset = OMAP4_CM_L4SEC_DES3DES_CLKCTRL_OFFSET},
	 {.offset = OMAP4_CM_L4SEC_PKAEIP29_CLKCTRL_OFFSET},
	 {.offset = OMAP4_CM_L4SEC_RNG_CLKCTRL_OFFSET},
	 {.offset = OMAP4_CM_L4SEC_SHA2MD51_CLKCTRL_OFFSET},
	 {.offset = OMAP4_CM_L4SEC_CRYPTODMA_CLKCTRL_OFFSET} },
	},
	/* OMAP4430_CM2_CEFUSE_MOD */
	{.mod_off = OMAP4430_CM2_CEFUSE_INST, .no_reg = 2,
	{{.offset = OMAP4_CM_CEFUSE_CLKSTCTRL_OFFSET},
	 {.offset = OMAP4_CM_CEFUSE_CEFUSE_CLKCTRL_OFFSET} },
	},
};

static void omap4_cm1_prepare_off(void)
{
	u32 i, j;
	struct omap4_cm_regs *cm_reg = omap4_cm1_regs;

	for (i = 0; i < ARRAY_SIZE(omap4_cm1_regs); i++, cm_reg++) {
		for (j = 0; j < cm_reg->no_reg; j++) {
			cm_reg->reg[j].val =
			    omap4_cminst_read_inst_reg(OMAP4430_CM1_PARTITION,
						       cm_reg->mod_off,
						       cm_reg->reg[j].offset);
		}
	}
}

static struct omap4_cm_regs omap5_cm2_regs[] = {
	/* OMAP54XX_CM2_OCP_SOCKET_MOD */
	{.mod_off = OMAP54XX_CM_CORE_OCP_SOCKET_INST, .no_reg = 1,
	{{.offset = OMAP54XX_CM_CM_CORE_PROFILING_CLKCTRL_OFFSET} },
	},
	/* OMAP54XX_CM2_CKGEN_MOD */
	{.mod_off = OMAP54XX_CM_CORE_CKGEN_INST, .no_reg = 1,
	{{.offset = OMAP54XX_CM_CLKSEL_USB_60MHZ_OFFSET} },
	},
	/* OMAP54XX_CM2_ALWAYS_ON_MOD */
	{.mod_off = OMAP54XX_CM_CORE_COREAON_INST, .no_reg = 7,
	{{.offset = OMAP54XX_CM_COREAON_CLKSTCTRL_OFFSET},
	 {.offset = OMAP54XX_CM_COREAON_SMARTREFLEX_MPU_CLKCTRL_OFFSET},
	 {.offset = OMAP54XX_CM_COREAON_SMARTREFLEX_MM_CLKCTRL_OFFSET},
	 {.offset = OMAP54XX_CM_COREAON_SMARTREFLEX_CORE_CLKCTRL_OFFSET},
	 {.offset = OMAP54XX_CM_COREAON_USB_PHY_CORE_CLKCTRL_OFFSET},
	 {.offset = OMAP54XX_CM_COREAON_BANDGAP_CLKCTRL_OFFSET},
	 {.offset = OMAP54XX_CM_COREAON_IO_SRCOMP_CLKCTRL_OFFSET} },
	},
	/* OMAP54XX_CM2_CORE_MOD */
	{.mod_off = OMAP54XX_CM_CORE_CORE_INST, .no_reg = 28,
	{{.offset = OMAP54XX_CM_L3MAIN1_CLKSTCTRL_OFFSET},
	 {.offset = OMAP54XX_CM_L3MAIN1_DYNAMICDEP_OFFSET},
	 {.offset = OMAP54XX_CM_L3MAIN2_CLKSTCTRL_OFFSET},
	 {.offset = OMAP54XX_CM_L3MAIN2_DYNAMICDEP_OFFSET},
	 {.offset = OMAP54XX_CM_L3MAIN2_GPMC_CLKCTRL_OFFSET},
	 {.offset = OMAP54XX_CM_IPU_CLKSTCTRL_OFFSET},
	 {.offset = OMAP54XX_CM_IPU_STATICDEP_OFFSET},
	 {.offset = OMAP54XX_CM_IPU_DYNAMICDEP_OFFSET},
	 {.offset = OMAP54XX_CM_IPU_IPU_CLKCTRL_OFFSET},
	 {.offset = OMAP54XX_CM_DMA_CLKSTCTRL_OFFSET},
	 {.offset = OMAP54XX_CM_DMA_STATICDEP_OFFSET},
	 {.offset = OMAP54XX_CM_EMIF_CLKSTCTRL_OFFSET},
	 {.offset = OMAP54XX_CM_EMIF_EMIF1_CLKCTRL_OFFSET},
	 {.offset = OMAP54XX_CM_EMIF_EMIF2_CLKCTRL_OFFSET},
	 {.offset = OMAP54XX_CM_EMIF_EMIF_DLL_CLKCTRL_OFFSET},
	 {.offset = OMAP54XX_CM_C2C_CLKSTCTRL_OFFSET},
	 {.offset = OMAP54XX_CM_C2C_STATICDEP_OFFSET},
	 {.offset = OMAP54XX_CM_C2C_DYNAMICDEP_OFFSET},
	 {.offset = OMAP54XX_CM_MIPIEXT_CLKSTCTRL_OFFSET},
	 {.offset = OMAP54XX_CM_MIPIEXT_STATICDEP_OFFSET},
	 {.offset = OMAP54XX_CM_MIPIEXT_DYNAMICDEP_OFFSET},
	 {.offset = OMAP54XX_CM_MIPIEXT_UNIPRO1_CLKCTRL_OFFSET},
	 {.offset = OMAP54XX_CM_MIPIEXT_LLI_CLKCTRL_OFFSET},
	 {.offset = OMAP54XX_CM_MIPIEXT_MPHY_CLKCTRL_OFFSET},
	 {.offset = OMAP54XX_CM_L4CFG_CLKSTCTRL_OFFSET},
	 {.offset = OMAP54XX_CM_L4CFG_DYNAMICDEP_OFFSET},
	 {.offset = OMAP54XX_CM_L3INSTR_L3_INSTR_CLKCTRL_OFFSET},
	 {.offset = OMAP54XX_CM_L3INSTR_OCP_WP_NOC_CLKCTRL_OFFSET} },
	},
	/* OMAP54XX_CM2_IVAHD_MOD */
	{.mod_off = OMAP54XX_CM_CORE_IVA_INST, .no_reg = 4,
	{{.offset = OMAP54XX_CM_IVA_CLKSTCTRL_OFFSET},
	 {.offset = OMAP54XX_CM_IVA_STATICDEP_OFFSET},
	 {.offset = OMAP54XX_CM_IVA_IVA_CLKCTRL_OFFSET},
	 {.offset = OMAP54XX_CM_IVA_SL2_CLKCTRL_OFFSET} },
	},
	/* OMAP54XX_CM2_CAM_MOD */
	{.mod_off = OMAP54XX_CM_CORE_CAM_INST, .no_reg = 5,
	{{.offset = OMAP54XX_CM_CAM_CLKSTCTRL_OFFSET},
	 {.offset = OMAP54XX_CM_CAM_STATICDEP_OFFSET},
	 {.offset = OMAP54XX_CM_CAM_ISS_CLKCTRL_OFFSET},
	 {.offset = OMAP54XX_CM_CAM_FDIF_CLKCTRL_OFFSET},
	 {.offset = OMAP54XX_CM_CAM_CAL_CLKCTRL_OFFSET} },
	},
	/* OMAP54XX_CM2_DSS_MOD */
	{.mod_off = OMAP54XX_CM_CORE_DSS_INST, .no_reg = 3,
	{{.offset = OMAP54XX_CM_DSS_CLKSTCTRL_OFFSET},
	 {.offset = OMAP54XX_CM_DSS_STATICDEP_OFFSET},
	 {.offset = OMAP54XX_CM_DSS_DSS_CLKCTRL_OFFSET} },
	},
	/* OMAP54XX_CM2_GFX_MOD */
	{.mod_off = OMAP54XX_CM_CORE_GPU_INST, .no_reg = 3,
	{{.offset = OMAP54XX_CM_GPU_CLKSTCTRL_OFFSET},
	 {.offset = OMAP54XX_CM_GPU_STATICDEP_OFFSET},
	 {.offset = OMAP54XX_CM_GPU_GPU_CLKCTRL_OFFSET} },
	},
	/* OMAP54XX_CM2_L3INIT_MOD */
	{.mod_off = OMAP54XX_CM_CORE_L3INIT_INST, .no_reg = 13,
	{{.offset = OMAP54XX_CM_L3INIT_CLKSTCTRL_OFFSET},
	 {.offset = OMAP54XX_CM_L3INIT_STATICDEP_OFFSET},
	 {.offset = OMAP54XX_CM_L3INIT_MMC1_CLKCTRL_OFFSET},
	 {.offset = OMAP54XX_CM_L3INIT_MMC2_CLKCTRL_OFFSET},
	 {.offset = OMAP54XX_CM_L3INIT_HSI_CLKCTRL_OFFSET},
	 {.offset = OMAP54XX_CM_L3INIT_UNIPRO2_CLKCTRL_OFFSET},
	 {.offset = OMAP54XX_CM_L3INIT_MPHY_UNIPRO2_CLKCTRL_OFFSET},
	 {.offset = OMAP54XX_CM_L3INIT_USB_HOST_HS_CLKCTRL_OFFSET},
	 {.offset = OMAP54XX_CM_L3INIT_USB_TLL_HS_CLKCTRL_OFFSET},
	 {.offset = OMAP54XX_CM_L3INIT_SATA_CLKCTRL_OFFSET},
	 {.offset = OMAP54XX_CM_L3INIT_OCP2SCP1_CLKCTRL_OFFSET},
	 {.offset = OMAP54XX_CM_L3INIT_OCP2SCP3_CLKCTRL_OFFSET},
	 {.offset = OMAP54XX_CM_L3INIT_USB_OTG_SS_CLKCTRL_OFFSET} },
	},
	/* OMAP54XX_CM2_L4PER_MOD */
	{.mod_off = OMAP54XX_CM_CORE_L4PER_INST, .no_reg = 49,
	{{.offset = OMAP54XX_CM_L4PER_CLKSTCTRL_OFFSET},
	 {.offset = OMAP54XX_CM_L4PER_DYNAMICDEP_OFFSET},
	 {.offset = OMAP54XX_CM_L4PER_TIMER10_CLKCTRL_OFFSET},
	 {.offset = OMAP54XX_CM_L4PER_TIMER11_CLKCTRL_OFFSET},
	 {.offset = OMAP54XX_CM_L4PER_TIMER2_CLKCTRL_OFFSET},
	 {.offset = OMAP54XX_CM_L4PER_TIMER3_CLKCTRL_OFFSET},
	 {.offset = OMAP54XX_CM_L4PER_TIMER4_CLKCTRL_OFFSET},
	 {.offset = OMAP54XX_CM_L4PER_TIMER9_CLKCTRL_OFFSET},
	 {.offset = OMAP54XX_CM_L4PER_ELM_CLKCTRL_OFFSET},
	 {.offset = OMAP54XX_CM_L4PER_GPIO2_CLKCTRL_OFFSET},
	 {.offset = OMAP54XX_CM_L4PER_GPIO3_CLKCTRL_OFFSET},
	 {.offset = OMAP54XX_CM_L4PER_GPIO4_CLKCTRL_OFFSET},
	 {.offset = OMAP54XX_CM_L4PER_GPIO5_CLKCTRL_OFFSET},
	 {.offset = OMAP54XX_CM_L4PER_GPIO6_CLKCTRL_OFFSET},
	 {.offset = OMAP54XX_CM_L4PER_HDQ1W_CLKCTRL_OFFSET},
	 {.offset = OMAP54XX_CM_L4PER_I2C1_CLKCTRL_OFFSET},
	 {.offset = OMAP54XX_CM_L4PER_I2C2_CLKCTRL_OFFSET},
	 {.offset = OMAP54XX_CM_L4PER_I2C3_CLKCTRL_OFFSET},
	 {.offset = OMAP54XX_CM_L4PER_I2C4_CLKCTRL_OFFSET},
	 {.offset = OMAP54XX_CM_L4PER_L4_PER_CLKCTRL_OFFSET},
	 {.offset = OMAP54XX_CM_L4PER_MCSPI1_CLKCTRL_OFFSET},
	 {.offset = OMAP54XX_CM_L4PER_MCSPI2_CLKCTRL_OFFSET},
	 {.offset = OMAP54XX_CM_L4PER_MCSPI3_CLKCTRL_OFFSET},
	 {.offset = OMAP54XX_CM_L4PER_MCSPI4_CLKCTRL_OFFSET},
	 {.offset = OMAP54XX_CM_L4PER_GPIO7_CLKCTRL_OFFSET},
	 {.offset = OMAP54XX_CM_L4PER_GPIO8_CLKCTRL_OFFSET},
	 {.offset = OMAP54XX_CM_L4PER_MMC3_CLKCTRL_OFFSET},
	 {.offset = OMAP54XX_CM_L4PER_MMC4_CLKCTRL_OFFSET},
	 {.offset = OMAP54XX_CM_L4PER_SLIMBUS2_CLKCTRL_OFFSET},
	 {.offset = OMAP54XX_CM_L4PER_UART1_CLKCTRL_OFFSET},
	 {.offset = OMAP54XX_CM_L4PER_UART2_CLKCTRL_OFFSET},
	 {.offset = OMAP54XX_CM_L4PER_UART3_CLKCTRL_OFFSET},
	 {.offset = OMAP54XX_CM_L4PER_UART4_CLKCTRL_OFFSET},
	 {.offset = OMAP54XX_CM_L4PER_MMC5_CLKCTRL_OFFSET},
	 {.offset = OMAP54XX_CM_L4PER_I2C5_CLKCTRL_OFFSET},
	 {.offset = OMAP54XX_CM_L4PER_UART5_CLKCTRL_OFFSET},
	 {.offset = OMAP54XX_CM_L4PER_UART6_CLKCTRL_OFFSET},
	 {.offset = OMAP54XX_CM_L4SEC_CLKSTCTRL_OFFSET},
	 {.offset = OMAP54XX_CM_L4SEC_STATICDEP_OFFSET},
	 {.offset = OMAP54XX_CM_L4SEC_DYNAMICDEP_OFFSET},
	 {.offset = OMAP54XX_CM_L4SEC_AES1_CLKCTRL_OFFSET},
	 {.offset = OMAP54XX_CM_L4SEC_AES2_CLKCTRL_OFFSET},
	 {.offset = OMAP54XX_CM_L4SEC_DES3DES_CLKCTRL_OFFSET},
	 {.offset = OMAP54XX_CM_L4SEC_FPKA_CLKCTRL_OFFSET},
	 {.offset = OMAP54XX_CM_L4SEC_RNG_CLKCTRL_OFFSET},
	 {.offset = OMAP54XX_CM_L4SEC_SHA2MD5_CLKCTRL_OFFSET},
	 {.offset = OMAP54XX_CM_L4SEC_DMA_CRYPTO_CLKCTRL_OFFSET} },
	},
	/* OMAP54XX_CM2_CEFUSE_MOD */
	{.mod_off = OMAP54XX_CM_CORE_CUSTEFUSE_INST, .no_reg = 2,
	{{.offset = OMAP54XX_CM_CUSTEFUSE_CLKSTCTRL_OFFSET},
	 {.offset = OMAP54XX_CM_CUSTEFUSE_EFUSE_CTRL_CUST_CLKCTRL_OFFSET} },
	},
};

static void omap4_cm_part_save(struct omap4_cm_regs *cm_reg, int size,
	u8 partition)
{
	u32 i, j;

	for (i = 0; i < size; i++, cm_reg++) {
		for (j = 0; j < cm_reg->no_reg; j++) {
			cm_reg->reg[j].val =
			    omap4_cminst_read_inst_reg(partition,
						       cm_reg->mod_off,
						       cm_reg->reg[j].offset);
		}
	}
}

static void omap4_cm_part_restore(struct omap4_cm_regs *cm_reg, int size,
	u8 partition)
{
	u32 i, j;

	for (i = 0; i < size; i++, cm_reg++) {
		for (j = 0; j < cm_reg->no_reg; j++) {
			omap4_cminst_write_inst_reg(cm_reg->reg[j].val,
						    OMAP4430_CM1_PARTITION,
						    cm_reg->mod_off,
						    cm_reg->reg[j].offset);
		}
	}
}

static void omap4_cm2_resume_off(void)
{
	u32 i, j;
	struct omap4_cm_regs *cm_reg = omap4_cm2_regs;
	int size = ARRAY_SIZE(omap4_cm2_regs);

	if (cpu_is_omap54xx()) {
		cm_reg = omap5_cm2_regs;
		size = ARRAY_SIZE(omap4_cm2_regs);
	}

	for (i = 0; i < size; i++, cm_reg++) {
		for (j = 0; j < cm_reg->no_reg; j++) {
			omap4_cminst_write_inst_reg(cm_reg->reg[j].val,
						    OMAP4430_CM2_PARTITION,
						    cm_reg->mod_off,
						    cm_reg->reg[j].offset);
		}
	}
}

void omap4_cm_prepare_off(void)
{
	if (cpu_is_omap44xx()) {
		omap4_cm_part_save(omap4_cm1_regs,
			ARRAY_SIZE(omap4_cm1_regs), OMAP4430_CM1_PARTITION);
		omap4_cm_part_save(omap4_cm2_regs,
			ARRAY_SIZE(omap4_cm2_regs), OMAP4430_CM2_PARTITION);
	} else {
		/* OMAP5 can use omap4 cm1 registers as the layout is same */
		omap4_cm_part_save(omap4_cm1_regs,
			ARRAY_SIZE(omap4_cm1_regs),
			OMAP54XX_CM_CORE_AON_PARTITION);
		omap4_cm_part_save(omap5_cm2_regs,
			ARRAY_SIZE(omap5_cm2_regs), OMAP54XX_CM_CORE_PARTITION);
	}
}

void omap4_cm_resume_off(void)
{
	if (cpu_is_omap44xx()) {
		omap4_cm_part_restore(omap4_cm1_regs,
			ARRAY_SIZE(omap4_cm1_regs), OMAP4430_CM1_PARTITION);
		omap4_cm_part_restore(omap4_cm2_regs,
			ARRAY_SIZE(omap4_cm2_regs), OMAP4430_CM2_PARTITION);
	} else {
		/* omap5 can use omap4 cm1 registers as the layout is same */
		omap4_cm_part_restore(omap4_cm1_regs,
			ARRAY_SIZE(omap4_cm1_regs),
			OMAP54XX_CM_CORE_AON_PARTITION);
		omap4_cm_part_restore(omap5_cm2_regs,
			ARRAY_SIZE(omap5_cm2_regs),
			OMAP54XX_CM_CORE_PARTITION);
	}
}
