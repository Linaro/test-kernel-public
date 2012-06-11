/*
 * OMAP3/OMAP4 Voltage Management Routines
 *
 * Author: Thara Gopinath	<thara@ti.com>
 *
 * Copyright (C) 2007 Texas Instruments, Inc.
 * Rajendra Nayak <rnayak@ti.com>
 * Lesly A M <x0080970@ti.com>
 *
 * Copyright (C) 2008 Nokia Corporation
 * Kalle Jokiniemi
 *
 * Copyright (C) 2010 Texas Instruments, Inc.
 * Thara Gopinath <thara@ti.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/init.h>

#include <plat/common.h>

#include "prm-regbits-44xx.h"
#include "prm44xx.h"
#include "prm54xx.h"
#include "prcm44xx.h"
#include "prminst44xx.h"
#include "voltage.h"
#include "omap_opp_data.h"
#include "vc.h"
#include "vp.h"
#include "abb.h"

static const struct omap_vfsm_instance omap5_vdd_mpu_vfsm = {
	.voltsetup_reg = OMAP54XX_PRM_VOLTSETUP_MPU_RET_SLEEP_OFFSET,
};
static struct omap_vdd_info omap5_vdd_mpu_info;


static const struct omap_vfsm_instance omap5_vdd_mm_vfsm = {
	.voltsetup_reg = OMAP54XX_PRM_VOLTSETUP_MM_RET_SLEEP_OFFSET,
};
static struct omap_vdd_info omap5_vdd_mm_info;

static const struct omap_vfsm_instance omap5_vdd_core_vfsm = {
	.voltsetup_reg = OMAP54XX_PRM_VOLTSETUP_CORE_RET_SLEEP_OFFSET,
};
static struct omap_vdd_info omap5_vdd_core_info;

static struct voltagedomain omap5_voltdm_mpu = {
	.name = "mpu",
	.scalable = true,
	.read = omap4_prm_vcvp_read,
	.write = omap4_prm_vcvp_write,
	.rmw = omap4_prm_vcvp_rmw,
	.vc = &omap5_vc_mpu,
	.vfsm = &omap5_vdd_mpu_vfsm,
	.vp = &omap5_vp_mpu,
	.abb = &omap5_abb_mpu,
	.vdd = &omap5_vdd_mpu_info,
};

static struct voltagedomain omap5_voltdm_mm = {
	.name = "mm",
	.scalable = true,
	.read = omap4_prm_vcvp_read,
	.write = omap4_prm_vcvp_write,
	.rmw = omap4_prm_vcvp_rmw,
	.vc = &omap5_vc_mm,
	.vfsm = &omap5_vdd_mm_vfsm,
	.vp = &omap5_vp_mm,
	.abb = &omap5_abb_mm,
	.vdd = &omap5_vdd_mm_info,
};

static struct voltagedomain omap5_voltdm_core = {
	.name = "core",
	.scalable = true,
	.read = omap4_prm_vcvp_read,
	.write = omap4_prm_vcvp_write,
	.rmw = omap4_prm_vcvp_rmw,
	.vc = &omap5_vc_core,
	.vfsm = &omap5_vdd_core_vfsm,
	.vp = &omap5_vp_core,
	.vdd = &omap5_vdd_core_info,
};

static struct voltagedomain omap5_voltdm_wkup = {
	.name = "wakeup",
};

static struct voltagedomain *voltagedomains_omap5[] __initdata = {
	&omap5_voltdm_mpu,
	&omap5_voltdm_mm,
	&omap5_voltdm_core,
	&omap5_voltdm_wkup,
	NULL,
};

static const char *sys_clk_name __initdata = "sys_clkin_ck";

void __init omap54xx_voltagedomains_init(void)
{
	struct voltagedomain *voltdm;
	int i;

	/*
	 * XXX Will depend on the process, validation, and binning
	 * for the currently-running IC
	 */
	omap5_voltdm_mpu.volt_data = omap54xx_vdd_mpu_volt_data;
	omap5_vdd_mpu_info.dep_vdd_info = omap54xx_vddmpu_dep_info;

	omap5_voltdm_mm.volt_data = omap54xx_vdd_mm_volt_data;
	omap5_vdd_mm_info.dep_vdd_info = omap54xx_vddmm_dep_info;

	omap5_voltdm_core.volt_data = omap54xx_vdd_core_volt_data;

	for (i = 0; voltdm = voltagedomains_omap5[i], voltdm; i++)
		voltdm->sys_clk.name = sys_clk_name;

	voltdm_init(voltagedomains_omap5);
};
