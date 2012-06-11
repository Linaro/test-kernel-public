/*
 * OMAP5 CPU idle Routines
 *
 * Copyright (C) 2012 Texas Instruments, Inc.
 * Santosh Shilimkar <santosh.shilimkar@ti.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/sched.h>
#include <linux/cpuidle.h>
#include <linux/cpu_pm.h>
#include <linux/clockchips.h>
#include <linux/spinlock.h>

#include <asm/proc-fns.h>

#include "common.h"
#include "pm.h"
#include "prm.h"
#include "clockdomain.h"

#ifdef CONFIG_CPU_IDLE

/* Machine specific information to be recorded in the C-state driver_data */
struct omap5_idle_statedata {
	u32 cpu_state;
	u32 mpu_logic_state;
	u32 mpu_state;
	u8 mpu_state_vote;
	u8 valid;
};

static struct cpuidle_params cpuidle_params_table[] = {
	/* C1 - CPU0 ON + CPU1 ON + MPU ON */
	{.exit_latency = 2 + 2 , .target_residency = 5, .valid = 1},
	/* C2 - CPU0 INA + CPU1 INA + MPU INA */
	{.exit_latency = 10 + 10 , .target_residency = 10, .valid = 1},
	/* C3- CPU0 CSWR + CPU1 CSWR + MPU CSWR */
	{.exit_latency = 328 + 440 , .target_residency = 960, .valid = 1},
	/* C4 - CPU0 OFF + CPU1 OFF + MPU OSWR */
	{.exit_latency = 460 + 518 , .target_residency = 1100, .valid = 1},
};

#define OMAP5_NUM_STATES ARRAY_SIZE(cpuidle_params_table)

struct omap5_idle_statedata omap5_idle_data[OMAP5_NUM_STATES];
static struct powerdomain *mpu_pd, *cpu_pd[NR_CPUS];
static struct clockdomain *cpu_clkdm[NR_CPUS];
static DEFINE_RAW_SPINLOCK(mpuss_idle_lock);
static atomic_t abort_barrier;
static bool cpu_done[NR_CPUS];

/**
 * omap5_enter_idle - Programs OMAP5 to enter the specified state
 * @dev: cpuidle device
 * @drv: cpuidle driver
 * @index: the index of state to be entered
 *
 * Called from the CPUidle framework to program the device to the
 * specified low power state selected by the governor.
 * Returns the amount of time spent in the low power state.
 */
static int omap5_enter_idle(struct cpuidle_device *dev,
			struct cpuidle_driver *drv,
			int index)
{
	struct omap5_idle_statedata *cx =
			cpuidle_get_statedata(&dev->states_usage[index]);
	struct timespec ts_preidle, ts_postidle, ts_idle;
	int idle_time;
	int cpu_id = smp_processor_id();

	/* Used to keep track of the total time in idle */
	getnstimeofday(&ts_preidle);

	local_irq_disable();
	local_fiq_disable();

	if (index > 0)
		clockevents_notify(CLOCK_EVT_NOTIFY_BROADCAST_ENTER, &cpu_id);

	/*
	 * Call idle CPU PM enter notifier chain so that
	 * VFP and per CPU interrupt context is saved.
	 */
	if (cx->cpu_state == PWRDM_POWER_OFF)
		cpu_pm_enter();

	raw_spin_lock(&mpuss_idle_lock);
	cx->mpu_state_vote++;
	raw_spin_unlock(&mpuss_idle_lock);

	if (cx->mpu_state_vote == num_online_cpus()) {
		pwrdm_set_logic_retst(mpu_pd, cx->mpu_logic_state);
		omap_set_pwrdm_state(mpu_pd, cx->mpu_state);
	}

	/*
	 * Call idle CPU cluster PM enter notifier chain
	 * to save GIC and wakeupgen context.
	 */
	if ((cx->mpu_state == PWRDM_POWER_RET) &&
		(cx->mpu_logic_state == PWRDM_POWER_OFF))
			cpu_cluster_pm_enter();

	omap_enter_lowpower(dev->cpu, cx->cpu_state);

	/*
	 * Call idle CPU PM exit notifier chain to restore
	 * VFP and per CPU IRQ context. Only CPU0 state is
	 * considered since CPU1 is managed by CPU hotplug.
	 */
	if (pwrdm_read_prev_pwrst(cpu_pd[dev->cpu]) == PWRDM_POWER_OFF)
		cpu_pm_exit();

	raw_spin_lock(&mpuss_idle_lock);
	cx->mpu_state_vote--;
	raw_spin_unlock(&mpuss_idle_lock);

	/*
	 * Call idle CPU cluster PM exit notifier chain
	 * to restore GIC and wakeupgen context.
	 */
	if (omap_mpuss_read_prev_context_state())
		cpu_cluster_pm_exit();

	if (index > 0)
		clockevents_notify(CLOCK_EVT_NOTIFY_BROADCAST_EXIT, &cpu_id);

	getnstimeofday(&ts_postidle);
	ts_idle = timespec_sub(ts_postidle, ts_preidle);

	local_irq_enable();
	local_fiq_enable();

	idle_time = ts_idle.tv_nsec / NSEC_PER_USEC + ts_idle.tv_sec * \
								USEC_PER_SEC;
	/* Update cpuidle counters */
	dev->last_residency = idle_time;

	return index;
}

static int omap5_enter_couple_idle(struct cpuidle_device *dev,
			struct cpuidle_driver *drv,
			int index)
{
	struct omap5_idle_statedata *cx =
			cpuidle_get_statedata(&dev->states_usage[index]);
	struct timespec ts_preidle, ts_postidle, ts_idle;
	int idle_time;
	int cpu_id = smp_processor_id();

	/* Used to keep track of the total time in idle */
	getnstimeofday(&ts_preidle);

	local_irq_disable();
	local_fiq_disable();
	clockevents_notify(CLOCK_EVT_NOTIFY_BROADCAST_ENTER, &cpu_id);

	if ((dev->cpu == 0) && cpumask_test_cpu(1, cpu_online_mask)) {
		while (pwrdm_read_pwrst(cpu_pd[1]) != PWRDM_POWER_OFF) {
			cpu_relax();

			/*
			 * CPU1 could have already entered & exited idle
			 * without hitting off because of a wakeup
			 * or a failed attempt to hit off mode.  Check for
			 * that here, otherwise we could spin forever
			 * waiting for CPU1 off.
			 */
			if (cpu_done[1])
				goto fail;
		}
	}

	cpu_pm_enter();

	if (dev->cpu == 0) {
		pwrdm_set_logic_retst(mpu_pd, cx->mpu_logic_state);
		omap_set_pwrdm_state(mpu_pd, cx->mpu_state);

		if ((cx->mpu_state == PWRDM_POWER_RET) &&
			(cx->mpu_logic_state == PWRDM_POWER_OFF))
				cpu_cluster_pm_enter();
	}

	if (dev->cpu)
		pwrdm_enable_force_off(cpu_pd[1]);

	omap_enter_lowpower(dev->cpu, cx->cpu_state);

	if (dev->cpu)
		pwrdm_disable_force_off(cpu_pd[1]);

	cpu_done[dev->cpu] = true;

	/* Wakeup CPU1 only if it is not offlined */
	if ((dev->cpu == 0) && cpumask_test_cpu(1, cpu_online_mask)) {
		clkdm_wakeup(cpu_clkdm[1]);
		clkdm_allow_idle(cpu_clkdm[1]);
	}
	cpu_pm_exit();

	/*
	 * Call idle CPU cluster PM exit notifier chain
	 * to restore GIC and wakeupgen context.
	 */
	if (omap_mpuss_read_prev_context_state())
		cpu_cluster_pm_exit();
fail:
	cpuidle_coupled_parallel_barrier(dev, &abort_barrier);
	cpu_done[dev->cpu] = false;

	clockevents_notify(CLOCK_EVT_NOTIFY_BROADCAST_EXIT, &cpu_id);
	getnstimeofday(&ts_postidle);
	ts_idle = timespec_sub(ts_postidle, ts_preidle);
	local_irq_enable();
	local_fiq_enable();
	idle_time = ts_idle.tv_nsec / NSEC_PER_USEC + ts_idle.tv_sec * \
								USEC_PER_SEC;
	dev->last_residency = idle_time;
	return index;
}

DEFINE_PER_CPU(struct cpuidle_device, omap5_idle_dev);

struct cpuidle_driver omap5_idle_driver = {
	.name =		"omap5_idle",
	.owner =	THIS_MODULE,
};

static inline void _fill_cstate(struct cpuidle_driver *drv,
					int idx, const char *descr, int flags)
{
	struct cpuidle_state *state = &drv->states[idx];

	state->exit_latency	= cpuidle_params_table[idx].exit_latency;
	state->target_residency	= cpuidle_params_table[idx].target_residency;
	state->flags            = (CPUIDLE_FLAG_TIME_VALID | flags);
	if (state->flags & CPUIDLE_FLAG_COUPLED)
		state->enter		= omap5_enter_couple_idle;
	else
		state->enter		= omap5_enter_idle;

	sprintf(state->name, "C%d", idx + 1);
	strncpy(state->desc, descr, CPUIDLE_DESC_LEN);
}

static inline struct omap5_idle_statedata *_fill_cstate_usage(
					struct cpuidle_device *dev,
					int idx)
{
	struct omap5_idle_statedata *cx = &omap5_idle_data[idx];
	struct cpuidle_state_usage *state_usage = &dev->states_usage[idx];

	cx->valid = cpuidle_params_table[idx].valid;
	cpuidle_set_statedata(state_usage, cx);

	return cx;
}

/**
 * omap5_idle_init - Init routine for OMAP5 idle
 *
 * Registers the OMAP5 specific cpuidle driver to the cpuidle
 * framework with the valid set of states.
 */
int __init omap5_idle_init(void)
{
	struct omap5_idle_statedata *cx;
	struct cpuidle_device *dev;
	struct cpuidle_driver *drv = &omap5_idle_driver;
	unsigned int cpu_id = 0;

	mpu_pd = pwrdm_lookup("mpu_pwrdm");
	cpu_pd[0] = pwrdm_lookup("cpu0_pwrdm");
	cpu_pd[1] = pwrdm_lookup("cpu1_pwrdm");
	if ((!mpu_pd) || (!cpu_pd[0]) || (!cpu_pd[1]))
		return -ENODEV;

	cpu_clkdm[0] = clkdm_lookup("mpu0_clkdm");
	cpu_clkdm[1] = clkdm_lookup("mpu1_clkdm");
	if (!cpu_clkdm[0] || !cpu_clkdm[1])
		return -ENODEV;


	for_each_cpu(cpu_id, cpu_online_mask) {
		drv->safe_state_index = -1;
		dev = &per_cpu(omap5_idle_dev, cpu_id);
		dev->cpu = cpu_id;
		dev->state_count = 0;
		drv->state_count = 0;
		dev->coupled_cpus = *cpu_online_mask;

		/* C1 - CPU0 ON + CPU1 ON + MPU ON */
		_fill_cstate(drv, 0, "MPUSS ON", 0);
		drv->safe_state_index = 0;
		cx = _fill_cstate_usage(dev, 0);
		cx->valid = 1;	/* C1 is always valid */
		cx->cpu_state = PWRDM_POWER_ON;
		cx->mpu_state = PWRDM_POWER_ON;
		cx->mpu_state_vote = 0;
		cx->mpu_logic_state = PWRDM_POWER_RET;
		dev->state_count++;
		drv->state_count++;

		/* C2 - CPU0 INA + CPU1 INA + MPU INA */
		_fill_cstate(drv, 1, "MPUSS CSWR", 0);
		cx = _fill_cstate_usage(dev, 1);
		if (cx != NULL) {
			cx->cpu_state = PWRDM_POWER_INACTIVE;
			cx->mpu_state = PWRDM_POWER_INACTIVE;
			cx->mpu_state_vote = 0;
			cx->mpu_logic_state = PWRDM_POWER_RET;
			dev->state_count++;
			drv->state_count++;
		}

		/* C3 - CPU0 CSWR + CPU1 CSWR + MPU CSWR */
		_fill_cstate(drv, 2, "MPUSS OSWR", 0);
		cx = _fill_cstate_usage(dev, 2);
		if (cx != NULL) {
			cx->cpu_state = PWRDM_POWER_RET;
			cx->mpu_state = PWRDM_POWER_RET;
			cx->mpu_state_vote = 0;
			cx->mpu_logic_state = PWRDM_POWER_RET;
			dev->state_count++;
			drv->state_count++;
		}

		/* C4 - CPU0 OFF + CPU1 OFF + MPU OSWR */
		_fill_cstate(drv, 3, "MPUSS OSWR", CPUIDLE_FLAG_COUPLED);
		cx = _fill_cstate_usage(dev, 3);
		if (cx != NULL) {
			cx->cpu_state = PWRDM_POWER_OFF;
			cx->mpu_state = PWRDM_POWER_RET;
			cx->mpu_state_vote = 0;
			cx->mpu_logic_state = PWRDM_POWER_OFF;
			dev->state_count++;
			drv->state_count++;
		}

		cpuidle_register_driver(&omap5_idle_driver);

		pr_debug("Register %d C-states on CPU%d\n",
						dev->state_count, cpu_id);
		if (cpuidle_register_device(dev)) {
			pr_err("%s: CPUidle registration failed\n", __func__);
			return -EIO;
		}
	}

	return 0;
}
#else
int __init omap5_idle_init(void)
{
	return 0;
}
#endif /* CONFIG_CPU_IDLE */
