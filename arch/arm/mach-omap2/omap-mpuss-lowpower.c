/*
 * OMAP MPUSS low power code
 *
 * Copyright (C) 2011 Texas Instruments, Inc.
 *	Santosh Shilimkar <santosh.shilimkar@ti.com>
 *
 * OMAP4430 MPUSS mainly consists of dual Cortex-A9 with per-CPU
 * Local timer and Watchdog, GIC, SCU, PL310 L2 cache controller,
 * CPU0 and CPU1 LPRM modules.
 * CPU0, CPU1 and MPUSS each have there own power domain and
 * hence multiple low power combinations of MPUSS are possible.
 *
 * The CPU0 and CPU1 can't support Closed switch Retention (CSWR)
 * because the mode is not supported by hw constraints of dormant
 * mode. While waking up from the dormant mode, a reset  signal
 * to the Cortex-A9 processor must be asserted by the external
 * power controller.
 *
 * With architectural inputs and hardware recommendations, only
 * below modes are supported from power gain vs latency point of view.
 *
 *	CPU0		CPU1		MPUSS
 *	----------------------------------------------
 *	ON		ON		ON
 *	ON(Inactive)	OFF		ON(Inactive)
 *	OFF		OFF		CSWR
 *	OFF		OFF		OSWR
 *	OFF		OFF		OFF(Device OFF *TBD)
 *	----------------------------------------------
 *
 * Note: CPU0 is the master core and it is the last CPU to go down
 * and first to wake-up when MPUSS low power states are excercised
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/errno.h>
#include <linux/linkage.h>
#include <linux/smp.h>
#include <linux/clk.h>

#include <asm/cacheflush.h>
#include <asm/tlbflush.h>
#include <asm/smp_scu.h>
#include <asm/pgalloc.h>
#include <asm/suspend.h>
#include <asm/hardware/cache-l2x0.h>

#include <plat/omap44xx.h>
#include <mach/omap-secure.h>

#include <mach/omap-secure.h>

#include "iomap.h"
#include "common.h"
#include "omap4-sar-layout.h"
#include "pm.h"
#include "prcm_mpu44xx.h"
#include "prminst44xx.h"
#include "prcm44xx.h"
#include "prm44xx.h"
#include "prm-regbits-44xx.h"

#include "prcm_mpu54xx.h"
#include "prm54xx.h"
#include "prm-regbits-54xx.h"
#include "cm44xx.h"

#ifdef CONFIG_SMP
#define NUM_DEN_MASK			0xfffff000

struct omap4_cpu_pm_info {
	struct powerdomain *pwrdm;
	void __iomem *scu_sar_addr;
	void __iomem *wkup_sar_addr;
	void __iomem *l2x0_sar_addr;
	void (*secondary_startup)(void);
};

struct cpu_pm_ops {
	int (*finish_suspend)(unsigned long cpu_state);
	void (*resume)(void);
	void (*scu_prepare)(unsigned int cpu_id, unsigned int cpu_state);
	void (*hotplug_restart)(void);
};

extern int omap4_finish_suspend(unsigned long cpu_state);
extern void omap4_cpu_resume(void);
extern int omap5_finish_suspend(unsigned long cpu_state);
extern void omap5_cpu_resume(void);

static DEFINE_PER_CPU(struct omap4_cpu_pm_info, omap4_pm_info);
static struct powerdomain *mpuss_pd;
static void __iomem *sar_base;
static struct voltagedomain *mpu_voltdm;

static int default_finish_suspend(unsigned long cpu_state)
{
	omap_do_wfi();
	return 0;
}

static void dummy_cpu_resume(void)
{}

void dummy_scu_prepare(unsigned int cpu_id, unsigned int cpu_state)
{}

struct cpu_pm_ops omap_pm_ops = {
	.finish_suspend		= default_finish_suspend,
	.resume			= dummy_cpu_resume,
	.scu_prepare		= dummy_scu_prepare,
	.hotplug_restart	= dummy_cpu_resume,
};

struct reg_tuple {
	void __iomem *addr;
	u32 val;
};

static struct reg_tuple tesla_reg[] = {
	{.addr = OMAP4430_CM_TESLA_CLKSTCTRL},
	{.addr = OMAP4430_CM_TESLA_TESLA_CLKCTRL},
	{.addr = OMAP4430_PM_TESLA_PWRSTCTRL},
};

static struct reg_tuple ivahd_reg[] = {
	{.addr = OMAP4430_CM_IVAHD_CLKSTCTRL},
	{.addr = OMAP4430_CM_IVAHD_IVAHD_CLKCTRL},
	{.addr = OMAP4430_CM_IVAHD_SL2_CLKCTRL},
	{.addr = OMAP4430_PM_IVAHD_PWRSTCTRL}
};

static struct reg_tuple l3instr_reg[] = {
	{.addr = OMAP4430_CM_L3INSTR_L3_3_CLKCTRL},
	{.addr = OMAP4430_CM_L3INSTR_L3_INSTR_CLKCTRL},
	{.addr = OMAP4430_CM_L3INSTR_OCP_WP1_CLKCTRL},
};

/*
 * Program the wakeup routine address for the CPU0 and CPU1
 * used for OFF or DORMANT wakeup.
 */
static inline void set_cpu_wakeup_addr(unsigned int cpu_id, u32 addr)
{
	struct omap4_cpu_pm_info *pm_info = &per_cpu(omap4_pm_info, cpu_id);

	__raw_writel(addr, pm_info->wkup_sar_addr);
}

/*
 * Set the CPUx powerdomain's previous power state
 */
static inline void set_cpu_next_pwrst(unsigned int cpu_id,
				unsigned int power_state)
{
	struct omap4_cpu_pm_info *pm_info = &per_cpu(omap4_pm_info, cpu_id);

	pwrdm_set_next_pwrst(pm_info->pwrdm, power_state);
}

/*
 * Read CPU's previous power state
 */
static inline unsigned int read_cpu_prev_pwrst(unsigned int cpu_id)
{
	struct omap4_cpu_pm_info *pm_info = &per_cpu(omap4_pm_info, cpu_id);

	return pwrdm_read_prev_pwrst(pm_info->pwrdm);
}

/*
 * Clear the CPUx powerdomain's previous power state
 */
static inline void clear_cpu_prev_pwrst(unsigned int cpu_id)
{
	struct omap4_cpu_pm_info *pm_info = &per_cpu(omap4_pm_info, cpu_id);

	pwrdm_clear_all_prev_pwrst(pm_info->pwrdm);
}

/*
 * Enable/disable the CPUx powerdomain FORCE OFF mode.
 */
static inline void set_cpu_force_off(unsigned int cpu_id, bool on)
{
	struct omap4_cpu_pm_info *pm_info = &per_cpu(omap4_pm_info, cpu_id);

	if (on)
		pwrdm_enable_force_off(pm_info->pwrdm);
	else
		pwrdm_disable_force_off(pm_info->pwrdm);
}

/*
 * Store the SCU power status value to scratchpad memory
 */
static void scu_pwrst_prepare(unsigned int cpu_id, unsigned int cpu_state)
{
	struct omap4_cpu_pm_info *pm_info = &per_cpu(omap4_pm_info, cpu_id);
	u32 scu_pwr_st;

	switch (cpu_state) {
	case PWRDM_POWER_RET:
		scu_pwr_st = SCU_PM_DORMANT;
		break;
	case PWRDM_POWER_OFF:
		scu_pwr_st = SCU_PM_POWEROFF;
		break;
	case PWRDM_POWER_ON:
	case PWRDM_POWER_INACTIVE:
	default:
		scu_pwr_st = SCU_PM_NORMAL;
		break;
	}

	__raw_writel(scu_pwr_st, pm_info->scu_sar_addr);
}

/* Helper functions for MPUSS OSWR */
static inline void mpuss_clear_prev_logic_pwrst(void)
{
	u32 reg;

	reg = omap4_prminst_read_inst_reg(OMAP4430_PRM_PARTITION,
		OMAP4430_PRM_MPU_INST, OMAP4_RM_MPU_MPU_CONTEXT_OFFSET);
	omap4_prminst_write_inst_reg(reg, OMAP4430_PRM_PARTITION,
		OMAP4430_PRM_MPU_INST, OMAP4_RM_MPU_MPU_CONTEXT_OFFSET);
}

static inline void cpu_clear_prev_logic_pwrst(unsigned int cpu_id)
{
	u32 reg;

	if (cpu_id) {
		reg = omap4_prcm_mpu_read_inst_reg(OMAP4430_PRCM_MPU_CPU1_INST,
					OMAP4_RM_CPU1_CPU1_CONTEXT_OFFSET);
		omap4_prcm_mpu_write_inst_reg(reg, OMAP4430_PRCM_MPU_CPU1_INST,
					OMAP4_RM_CPU1_CPU1_CONTEXT_OFFSET);
	} else {
		reg = omap4_prcm_mpu_read_inst_reg(OMAP4430_PRCM_MPU_CPU0_INST,
					OMAP4_RM_CPU0_CPU0_CONTEXT_OFFSET);
		omap4_prcm_mpu_write_inst_reg(reg, OMAP4430_PRCM_MPU_CPU0_INST,
					OMAP4_RM_CPU0_CPU0_CONTEXT_OFFSET);
	}
}

/**
 * omap4_mpuss_read_prev_context_state:
 * Function returns the MPUSS previous context state
 */
u32 omap_mpuss_read_prev_context_state(void)
{
	u32 reg;

	reg = omap4_prminst_read_inst_reg(OMAP4430_PRM_PARTITION,
		OMAP4430_PRM_MPU_INST, OMAP4_RM_MPU_MPU_CONTEXT_OFFSET);
	reg &= OMAP4430_LOSTCONTEXT_DFF_MASK;
	return reg;
}

/*
 * Store the CPU cluster state for L2X0 low power operations.
 */
static void l2x0_pwrst_prepare(unsigned int cpu_id, unsigned int save_state)
{
	struct omap4_cpu_pm_info *pm_info = &per_cpu(omap4_pm_info, cpu_id);

	__raw_writel(save_state, pm_info->l2x0_sar_addr);
}

/*
 * Save the L2X0 AUXCTRL and POR value to SAR memory. Its used to
 * in every restore MPUSS OFF path.
 */
#ifdef CONFIG_CACHE_L2X0
static void save_l2x0_context(void)
{
	u32 val;
	void __iomem *l2x0_base = omap4_get_l2cache_base();

	val = __raw_readl(l2x0_base + L2X0_AUX_CTRL);
	__raw_writel(val, sar_base + L2X0_AUXCTRL_OFFSET);
	val = __raw_readl(l2x0_base + L2X0_PREFETCH_CTRL);
	__raw_writel(val, sar_base + L2X0_PREFETCH_CTRL_OFFSET);
}
#else
static void save_l2x0_context(void)
{}
#endif

static inline void save_ivahd_tesla_regs(void)
{
	int i;

	if (!IS_PM44XX_ERRATUM(PM_OMAP4_ROM_IVAHD_TESLA_ERRATUM))
		return;

	for (i = 0; i < ARRAY_SIZE(tesla_reg); i++)
		tesla_reg[i].val = __raw_readl(tesla_reg[i].addr);

	for (i = 0; i < ARRAY_SIZE(ivahd_reg); i++)
		ivahd_reg[i].val = __raw_readl(ivahd_reg[i].addr);
}

static inline void restore_ivahd_tesla_regs(void)
{
	int i;

	if (!IS_PM44XX_ERRATUM(PM_OMAP4_ROM_IVAHD_TESLA_ERRATUM))
		return;

	for (i = 0; i < ARRAY_SIZE(tesla_reg); i++)
		__raw_writel(tesla_reg[i].val, tesla_reg[i].addr);

	for (i = 0; i < ARRAY_SIZE(ivahd_reg); i++)
		__raw_writel(ivahd_reg[i].val, ivahd_reg[i].addr);
}

static inline void save_l3instr_regs(void)
{
	int i;

	if (!IS_PM44XX_ERRATUM(PM_OMAP4_ROM_L3INSTR_ERRATUM))
		return;

	for (i = 0; i < ARRAY_SIZE(l3instr_reg); i++)
		l3instr_reg[i].val = __raw_readl(l3instr_reg[i].addr);
}

static inline void restore_l3instr_regs(void)
{
	int i;

	if (!IS_PM44XX_ERRATUM(PM_OMAP4_ROM_L3INSTR_ERRATUM))
		return;

	for (i = 0; i < ARRAY_SIZE(l3instr_reg); i++)
		__raw_writel(l3instr_reg[i].val, l3instr_reg[i].addr);
}

/**
 * omap_enter_lowpower: OMAP4 MPUSS Low Power Entry Function
 * The purpose of this function is to manage low power programming
 * of OMAP MPUSS subsystem
 * @cpu : CPU ID
 * @power_state: Low power state.
 *
 * MPUSS states for the context save:
 * save_state =
 *	0 - Nothing lost and no need to save: MPUSS INACTIVE
 *	1 - CPUx L1 and logic lost: MPUSS CSWR
 *	2 - CPUx L1 and logic lost + GIC lost: MPUSS OSWR
 *	3 - CPUx L1 and logic lost + GIC + L2 lost: DEVICE OFF
 *
 * OMAP5 MPUSS states for the context save:
 * save_state =
 *	0 - Nothing lost and no need to save: MPUSS INA/CSWR
 *	1 - CPUx L1 and logic lost: CPU OFF, MPUSS INA/CSWR
 *	2 - CPUx L1 and logic lost + GIC lost: MPUSS OSWR
 *	3 - CPUx L1 and logic lost + GIC + L2 lost: DEVICE OFF
 */
int omap_enter_lowpower(unsigned int cpu, unsigned int power_state)
{
	unsigned int save_state = 0;
	unsigned int wakeup_cpu;
	int ret;

	if (omap_rev() == OMAP4430_REV_ES1_0)
		return -ENXIO;

	switch (power_state) {
	case PWRDM_POWER_ON:
	case PWRDM_POWER_INACTIVE:
		save_state = 0;
		break;
	case PWRDM_POWER_OFF:
		save_state = 1;
		break;
	case PWRDM_POWER_RET:
		if (cpu_is_omap54xx()) {
			save_state = 0;
			break;
		}
	default:
		/*
		 * CPUx CSWR is invalid hardware stateon OMAP4. Also CPUx
		 * OSWR doesn't make much scense, since logic is lost and $L1
		 * needs to be cleaned because of coherency. This makes
		 * CPUx OSWR equivalent to CPUX OFF and hence not supported
		 */
		WARN_ON(1);
		return -ENXIO;
	}

	pwrdm_pre_transition();

	/*
	 * Check MPUSS next state and save interrupt controller if needed.
	 * In MPUSS OSWR or device OFF, interrupt controller  contest is lost.
	 */
	mpuss_clear_prev_logic_pwrst();
	if (omap4_device_next_state_off()) {
		/* Save the device context to SAR RAM */
		ret = omap_sar_save();
		if (ret)
			return ret;
		omap_sar_overwrite();
		omap4_cm_prepare_off();
		omap4_dpll_prepare_off();
		save_ivahd_tesla_regs();
		save_l3instr_regs();
		save_state = 3;
	} else if ((pwrdm_read_next_pwrst(mpuss_pd) == PWRDM_POWER_RET) &&
		(pwrdm_read_logic_retst(mpuss_pd) == PWRDM_POWER_OFF)) {
		save_ivahd_tesla_regs();
		save_l3instr_regs();
		save_state = 2;
	} else if (pwrdm_read_next_pwrst(mpuss_pd) == PWRDM_POWER_OFF) {
		save_ivahd_tesla_regs();
		save_l3instr_regs();
		save_state = 3;
	}

	cpu_clear_prev_logic_pwrst(cpu);
	set_cpu_next_pwrst(cpu, power_state);
	set_cpu_wakeup_addr(cpu, virt_to_phys(omap_pm_ops.resume));
	omap_pm_ops.scu_prepare(cpu, power_state);
	l2x0_pwrst_prepare(cpu, save_state);

	voltdm_pwrdm_disable(mpu_voltdm);

	/*
	 * Call low level function  with targeted low power state.
	 */
	cpu_suspend(save_state, omap_pm_ops.finish_suspend);

	voltdm_pwrdm_enable(mpu_voltdm);

	/*
	 * Restore the CPUx power state to ON otherwise CPUx
	 * power domain can transitions to programmed low power
	 * state while doing WFI outside the low powe code. On
	 * secure devices, CPUx does WFI which can result in
	 * domain transition
	 */
	wakeup_cpu = smp_processor_id();
	/*
	 * If !master cpu return to hotplug-path.
	 *
	 * GIC distributor control register has changed between
	 * CortexA9 r1pX and r2pX. The Control Register secure
	 * banked version is now composed of 2 bits:
	 * bit 0 == Secure Enable
	 * bit 1 == Non-Secure Enable
	 * The Non-Secure banked register has not changed
	 * Because the ROM Code is based on the r1pX GIC, the CPU1
	 * GIC restoration will cause a problem to CPU0 Non-Secure SW.
	 * The workaround must be:
	 * 1) Before doing the CPU1 wakeup, CPU0 must disable
	 * the GIC distributor
	 * 2) CPU1 must re-enable the GIC distributor on
	 * it's wakeup path.
	 */
	if (wakeup_cpu)
		if (cpu_is_omap446x())
			gic_dist_enable();
	set_cpu_next_pwrst(wakeup_cpu, PWRDM_POWER_ON);

	if (omap4_mpuss_read_prev_context_state()) {
		/*
		 * Dummy dispatcher call after OSWR and OFF
		 * Restore the right return Kernel address (with MMU on) for
		 * subsequent calls to secure ROM. Otherwise the return address
		 * will be to a PA return address and the system will hang.
		 */
		if (omap_type() != OMAP2_DEVICE_TYPE_GP)
			omap_secure_dispatcher(OMAP4_PPA_SERVICE_0,
					       FLAG_START_CRITICAL,
					       0, 0, 0, 0, 0);

		restore_ivahd_tesla_regs();
		restore_l3instr_regs();
	}

	if (omap4_device_prev_state_off()) {
		omap4_dpll_resume_off();
		omap4_cm_resume_off();
	}

	pwrdm_post_transition();

	return 0;
}

/**
 * omap4_hotplug_cpu: OMAP4 CPU hotplug entry
 * @cpu : CPU ID
 * @power_state: CPU low power state.
 */
int __cpuinit omap4_hotplug_cpu(unsigned int cpu, unsigned int power_state)
{
	unsigned int cpu_state = 0;
	struct omap4_cpu_pm_info *pm_info = &per_cpu(omap4_pm_info, cpu);

	if (omap_rev() == OMAP4430_REV_ES1_0)
		return -ENXIO;

	if (power_state == PWRDM_POWER_OFF)
		cpu_state = 1;

	clear_cpu_prev_pwrst(cpu);
	set_cpu_next_pwrst(cpu, power_state);
	set_cpu_wakeup_addr(cpu, virt_to_phys(pm_info->secondary_startup));
	scu_pwrst_prepare(cpu, power_state);

	/*
	 * CPU never retuns back if targetted power state is OFF mode.
	 * CPU ONLINE follows normal CPU ONLINE ptah via
	 * omap_secondary_startup().
	 */
	/*
	 * FIXME: The kernel hotplug path seems to be buggy for an
	 * integrated l2 cache on SMP machines. So use normal resume path.
	 */
	if (cpu_is_omap54xx())
		cpu_suspend(cpu_state, omap_pm_ops.finish_suspend);
	else
		omap_pm_ops.finish_suspend(cpu_state);

	/* Clear FORCE OFF mode if supported */
	set_cpu_force_off(cpu, 0);

	/*
	 * Enable the CPU interface for the case where CPU has not entered
	 * into low power state and hence immune to wakeupgen and SGIs.
	 * Needed for the cases where CPU1 PD targeted to CSWR/OFF instead
	 * of FORCE OFF state in CPU hotplug path.
	 */
	if (cpu_is_omap54xx()) {
		__raw_writel(0xf0, OMAP54XX_GIC_CPU_BASE + 0x04);
		__raw_writel(0x1, OMAP54XX_GIC_CPU_BASE);
	}

	set_cpu_next_pwrst(cpu, PWRDM_POWER_ON);
	return 0;
}

static void enable_mercury_retention_mode(void)
{
	u32 reg;

	reg = omap4_prcm_mpu_read_inst_reg(
		OMAP54XX_PRCM_MPU_DEVICE_INST,
		OMAP54XX_PRCM_MPU_PRM_PSCON_COUNT_OFFSET);
	/* Enable the Mercury retention mode */
	reg |= BIT(24);
	omap4_prcm_mpu_write_inst_reg(reg,
		OMAP54XX_PRCM_MPU_DEVICE_INST,
		OMAP54XX_PRCM_MPU_PRM_PSCON_COUNT_OFFSET);

}

/*
 * Initialise OMAP4 MPUSS
 */
int __init omap_mpuss_init(void)
{
	struct omap4_cpu_pm_info *pm_info;
	u32 cpu_wakeup_addr, l2x0_offset, omap_type_offset;

	if (omap_rev() == OMAP4430_REV_ES1_0) {
		WARN(1, "Power Management not supported on OMAP4430 ES1.0\n");
		return -ENODEV;
	}

	sar_base = omap4_get_sar_ram_base();

	/* Initilaise per CPU PM information */
	if (cpu_is_omap44xx()) {
		cpu_wakeup_addr = CPU0_WAKEUP_NS_PA_ADDR_OFFSET;
		l2x0_offset = L2X0_SAVE_OFFSET0;
	} else if (cpu_is_omap54xx()) {
		cpu_wakeup_addr = OMAP5_CPU0_WAKEUP_NS_PA_ADDR_OFFSET;
		l2x0_offset = OMAP5_L2X0_SAVE_OFFSET0;
	}

	pm_info = &per_cpu(omap4_pm_info, 0x0);
	pm_info->scu_sar_addr = sar_base + SCU_OFFSET0;
	pm_info->wkup_sar_addr = sar_base + cpu_wakeup_addr;
	pm_info->l2x0_sar_addr = sar_base + l2x0_offset;
	pm_info->pwrdm = pwrdm_lookup("cpu0_pwrdm");
	if (!pm_info->pwrdm) {
		pr_err("Lookup failed for CPU0 pwrdm\n");
		return -ENODEV;
	}

	/* Clear CPU previous power domain state */
	pwrdm_clear_all_prev_pwrst(pm_info->pwrdm);
	cpu_clear_prev_logic_pwrst(0);

	/* Initialise CPU0 power domain state to ON */
	pwrdm_set_next_pwrst(pm_info->pwrdm, PWRDM_POWER_ON);

	if (cpu_is_omap44xx()) {
		cpu_wakeup_addr = CPU1_WAKEUP_NS_PA_ADDR_OFFSET;
		l2x0_offset = L2X0_SAVE_OFFSET1;
	} else if (cpu_is_omap54xx()) {
		cpu_wakeup_addr = OMAP5_CPU1_WAKEUP_NS_PA_ADDR_OFFSET;
		l2x0_offset = OMAP5_L2X0_SAVE_OFFSET1;
	}

	pm_info = &per_cpu(omap4_pm_info, 0x1);
	pm_info->scu_sar_addr = sar_base + SCU_OFFSET1;
	pm_info->wkup_sar_addr = sar_base + CPU1_WAKEUP_NS_PA_ADDR_OFFSET;
	pm_info->l2x0_sar_addr = sar_base + L2X0_SAVE_OFFSET1;
	if (cpu_is_omap446x())
		pm_info->secondary_startup = omap_secondary_startup_4460;
	else
		pm_info->secondary_startup = omap_secondary_startup;

	pm_info->pwrdm = pwrdm_lookup("cpu1_pwrdm");
	if (!pm_info->pwrdm) {
		pr_err("Lookup failed for CPU1 pwrdm\n");
		return -ENODEV;
	}

	/* Clear CPU previous power domain state */
	pwrdm_clear_all_prev_pwrst(pm_info->pwrdm);
	cpu_clear_prev_logic_pwrst(1);

	/* Initialise CPU1 power domain state to ON */
	pwrdm_set_next_pwrst(pm_info->pwrdm, PWRDM_POWER_ON);

	mpuss_pd = pwrdm_lookup("mpu_pwrdm");
	if (!mpuss_pd) {
		pr_err("Failed to lookup MPUSS power domain\n");
		return -ENODEV;
	}
	pwrdm_clear_all_prev_pwrst(mpuss_pd);
	mpuss_clear_prev_logic_pwrst();

	mpu_voltdm = voltdm_lookup("mpu");

	/* Save device type on scratchpad for low level code to use */
	if (cpu_is_omap44xx())
		omap_type_offset = OMAP_TYPE_OFFSET;
	else if (cpu_is_omap54xx())
		omap_type_offset = OMAP5_TYPE_OFFSET;

	if (omap_type() != OMAP2_DEVICE_TYPE_GP)
		__raw_writel(1, sar_base + omap_type_offset);
	else
		__raw_writel(0, sar_base + omap_type_offset);

	save_l2x0_context();

	if (cpu_is_omap44xx()) {
		omap_pm_ops.finish_suspend = omap4_finish_suspend;
		omap_pm_ops.resume = omap4_cpu_resume;
		omap_pm_ops.scu_prepare = scu_pwrst_prepare;
		omap_pm_ops.hotplug_restart = omap_secondary_startup;
	} else if (cpu_is_omap54xx()) {
		omap_pm_ops.finish_suspend = omap5_finish_suspend;
		omap_pm_ops.hotplug_restart = omap5_cpu_resume;
		omap_pm_ops.resume = omap5_cpu_resume;
	}

	if (cpu_is_omap54xx())
		enable_mercury_retention_mode();

	return 0;
}

/* Initialise local timer clock */
void __init omap_mpuss_timer_init(void)
{
	static struct clk *sys_clk;
	unsigned long rate;
	u32 reg, num, den;

	sys_clk = clk_get(NULL, "sys_clkin_ck");
	if (!sys_clk) {
		pr_err("Could not get SYS clock\n");
		return;
	}

	rate = clk_get_rate(sys_clk);
	switch (rate) {
	case 12000000:
		num = 64;
		den = 125;
		break;
	case 13000000:
		num = 768;
		den = 1625;
		break;
	case 19200000:
		num = 8;
		den = 25;
		break;
	case 2600000:
		num = 384;
		den = 1625;
		break;
	case 2700000:
		num = 256;
		den = 1125;
		break;
	case 38400000:
		num = 4;
		den = 25;
		break;
	default:
		/* Program it for 38.4 MHz */
		num = 4;
		den = 25;
		break;
	}

	reg = omap4_prcm_mpu_read_inst_reg(
		OMAP54XX_PRCM_MPU_DEVICE_INST,
		OMAP54XX_PRM_FRAC_INCREMENTER_NUMERATOR_OFFSET);
	reg &= NUM_DEN_MASK;
	reg |= num;
	omap4_prcm_mpu_write_inst_reg(reg,
		OMAP54XX_PRCM_MPU_DEVICE_INST,
		OMAP54XX_PRM_FRAC_INCREMENTER_NUMERATOR_OFFSET);

	reg = omap4_prcm_mpu_read_inst_reg(
		OMAP54XX_PRCM_MPU_DEVICE_INST,
		OMAP54XX_PRM_FRAC_INCREMENTER_DENUMERATOR_RELOAD_OFFSET);
	reg &= NUM_DEN_MASK;
	reg |= den;
	omap4_prcm_mpu_write_inst_reg(reg,
		OMAP54XX_PRCM_MPU_DEVICE_INST,
		OMAP54XX_PRM_FRAC_INCREMENTER_DENUMERATOR_RELOAD_OFFSET);
}
#endif
