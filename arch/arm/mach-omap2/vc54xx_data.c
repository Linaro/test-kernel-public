/*
 * OMAP5 Voltage Controller (VC) data
 *
 * Copyright (C) 2007, 2010 Texas Instruments, Inc.
 * Rajendra Nayak <rnayak@ti.com>
 * Lesly A M <x0080970@ti.com>
 * Thara Gopinath <thara@ti.com>
 *
 * Copyright (C) 2008, 2011 Nokia Corporation
 * Kalle Jokiniemi
 * Paul Walmsley
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/io.h>
#include <linux/err.h>
#include <linux/init.h>

#include <plat/common.h>

#include "prm54xx.h"
#include "prm-regbits-54xx.h"
#include "voltage.h"

#include "vc.h"

/*
 * VC data common to 54xx chips
 * XXX This stuff presumably belongs in the vc3xxx.c or vc.c file.
 */
static const struct omap_vc_common omap5_vc_common = {
	.bypass_val_reg = OMAP54XX_PRM_VC_VAL_BYPASS_OFFSET,
	.data_shift = OMAP54XX_DATA_SHIFT,
	.slaveaddr_shift = OMAP54XX_SLAVEADDR_SHIFT,
	.regaddr_shift = OMAP54XX_REGADDR_SHIFT,
	.valid = OMAP54XX_VALID_MASK,
	.cmd_on_shift = OMAP54XX_ON_SHIFT,
	.cmd_on_mask = OMAP54XX_ON_MASK,
	.cmd_onlp_shift = OMAP54XX_ONLP_SHIFT,
	.cmd_ret_shift = OMAP54XX_RET_SHIFT,
	.cmd_off_shift = OMAP54XX_OFF_SHIFT,
	.i2c_cfg_reg = OMAP54XX_PRM_VC_CFG_I2C_MODE_OFFSET,
	.i2c_cfg_hsen_mask = OMAP54XX_HSMODEEN_MASK,
	.i2c_mcode_mask	 = OMAP54XX_HSMCODE_MASK,
};

/* VC instance data for each controllable voltage line */
struct omap_vc_channel omap5_vc_mpu = {
	.flags = OMAP_VC_CHANNEL_DEFAULT | OMAP_VC_CHANNEL_CFG_MUTANT,
	.common = &omap5_vc_common,
	.smps_sa_reg = OMAP54XX_PRM_VC_SMPS_MPU_CONFIG_OFFSET,
	.smps_volra_reg = OMAP54XX_PRM_VC_SMPS_MPU_CONFIG_OFFSET,
	.smps_cmdra_reg = OMAP54XX_PRM_VC_SMPS_MPU_CONFIG_OFFSET,
	.cfg_channel_reg = OMAP54XX_PRM_VC_SMPS_MPU_CONFIG_OFFSET,
	.cmdval_reg = OMAP54XX_PRM_VC_VAL_CMD_VDD_MPU_L_OFFSET,
	.smps_sa_mask = OMAP54XX_SA_VDD_MPU_L_MASK,
	.smps_volra_mask = OMAP54XX_VOLRA_VDD_MPU_L_MASK,
	.smps_cmdra_mask = OMAP54XX_CMDRA_VDD_MPU_L_MASK,
	.cfg_channel_sa_shift = OMAP54XX_SEL_SA_VDD_MPU_L_SHIFT,
	.voltctrl_shift = OMAP54XX_AUTO_CTRL_VDD_MPU_L_SHIFT,
	.voltctrl_mask = OMAP54XX_AUTO_CTRL_VDD_MPU_L_MASK,
};

struct omap_vc_channel omap5_vc_mm = {
	.common = &omap5_vc_common,
	.smps_sa_reg = OMAP54XX_PRM_VC_SMPS_MM_CONFIG_OFFSET,
	.smps_volra_reg = OMAP54XX_PRM_VC_SMPS_MM_CONFIG_OFFSET,
	.smps_cmdra_reg = OMAP54XX_PRM_VC_SMPS_MM_CONFIG_OFFSET,
	.cfg_channel_reg = OMAP54XX_PRM_VC_SMPS_MM_CONFIG_OFFSET,
	.cmdval_reg = OMAP54XX_PRM_VC_VAL_CMD_VDD_MM_L_OFFSET,
	.smps_sa_mask = OMAP54XX_SA_VDD_MM_L_MASK,
	.smps_volra_mask = OMAP54XX_VOLRA_VDD_MM_L_MASK,
	.smps_cmdra_mask = OMAP54XX_CMDRA_VDD_MM_L_MASK,
	.cfg_channel_sa_shift = OMAP54XX_SEL_SA_VDD_MM_L_SHIFT,
	.voltctrl_shift = OMAP54XX_AUTO_CTRL_VDD_MM_L_SHIFT,
	.voltctrl_mask = OMAP54XX_AUTO_CTRL_VDD_MM_L_MASK,
};

struct omap_vc_channel omap5_vc_core = {
	.common = &omap5_vc_common,
	.smps_sa_reg = OMAP54XX_PRM_VC_SMPS_CORE_CONFIG_OFFSET,
	.smps_volra_reg = OMAP54XX_PRM_VC_SMPS_CORE_CONFIG_OFFSET,
	.smps_cmdra_reg = OMAP54XX_PRM_VC_SMPS_CORE_CONFIG_OFFSET,
	.cfg_channel_reg = OMAP54XX_PRM_VC_SMPS_CORE_CONFIG_OFFSET,
	.cmdval_reg = OMAP54XX_PRM_VC_VAL_CMD_VDD_CORE_L_OFFSET,
	.smps_sa_mask = OMAP54XX_SA_VDD_CORE_L_MASK,
	.smps_volra_mask = OMAP54XX_VOLRA_VDD_CORE_L_MASK,
	.smps_cmdra_mask = OMAP54XX_CMDRA_VDD_CORE_L_MASK,
	.cfg_channel_sa_shift = OMAP54XX_SEL_SA_VDD_CORE_L_SHIFT,
	.voltctrl_shift = OMAP54XX_AUTO_CTRL_VDD_CORE_L_SHIFT,
	.voltctrl_mask = OMAP54XX_AUTO_CTRL_VDD_CORE_L_MASK,
};
