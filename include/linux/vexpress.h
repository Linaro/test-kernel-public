/*
 * include/linux/vexpress.h
 *
 * Copyright (C) 2012 ARM Limited
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive
 * for more details.
 *
 * Versatile Express register definitions and exported functions
 */

#define	VEXPRESS_SPC_WAKE_INTR_IRQ(cluster, cpu) \
			(1 << (4 * (cluster) + (cpu)))
#define	VEXPRESS_SPC_WAKE_INTR_FIQ(cluster, cpu) \
			(1 << (7 * (cluster) + (cpu)))
#define	VEXPRESS_SPC_WAKE_INTR_SWDOG			(1 << 10)
#define	VEXPRESS_SPC_WAKE_INTR_GTIMER		(1 << 11)
#define	VEXPRESS_SPC_WAKE_INTR_MASK			0xFFF

#ifdef CONFIG_ARM_SPC
extern int vexpress_spc_get_performance(int cluster, int *perf);
extern int vexpress_spc_set_performance(int cluster, int perf);
extern void vexpress_spc_set_wake_intr(u32 mask);
extern u32 vexpress_spc_get_wake_intr(int raw);
extern void vexpress_spc_powerdown_enable(int cluster, int enable);
extern void vexpress_spc_adb400_pd_enable(int cluster, int enable);
extern void vexpress_spc_wfi_cpureset(int cluster, int cpu, int enable);
extern int vexpress_spc_wfi_cpustat(int cluster);
extern void vexpress_spc_wfi_cluster_reset(int cluster, int enable);
extern bool vexpress_spc_check_loaded(void);
extern void vexpress_scc_ctl_snoops(int cluster, int enable);
#else
static inline int vexpress_spc_get_performance(int cluster, int *perf)
{
	return -EINVAL;
}
static inline int vexpress_spc_set_performance(int cluster, int perf)
{
	return -EINVAL;
}
static inline void vexpress_spc_set_wake_intr(u32 mask) { }
static inline u32 vexpress_spc_get_wake_intr(int raw) { return 0; }
static inline void vexpress_spc_powerdown_enable(int cluster, int enable) { }
static inline void vexpress_spc_adb400_pd_enable(int cluster, int enable) { }
static inline void vexpress_spc_wfi_cpureset(int cluster, int cpu, int enable)
{ }
static inline int vexpress_spc_wfi_cpustat(int cluster) { return 0; }
static inline void vexpress_spc_wfi_cluster_reset(int cluster, int enable) { }
static inline bool vexpress_spc_check_loaded(void)
{
	return false;
}
static inline void vexpress_scc_ctl_snoops(int cluster, int enable) { }
#endif
