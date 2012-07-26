#ifndef __ASM_EMEV2_H__
#define __ASM_EMEV2_H__

#ifdef CONFIG_ARCH_EMEV2

extern void emev2_map_io(void);
extern void emev2_init_irq(void);
extern void emev2_add_early_devices(void);
extern void emev2_add_standard_devices(void);
extern void emev2_clock_init(void);
extern void emev2_set_boot_vector(unsigned long value);
extern unsigned int emev2_get_core_count(void);
extern int emev2_platform_cpu_kill(unsigned int cpu);
extern void emev2_secondary_init(unsigned int cpu);
extern int emev2_boot_secondary(unsigned int cpu);
extern void emev2_smp_prepare_cpus(void);
#else
static inline unsigned int emev2_get_core_count(void) { return 1; };
static inline int emev2_platform_cpu_kill(unsigned int cpu) { return 0; }
static inline void emev2_secondary_init(unsigned int cpu) {}
static inline int emev2_boot_secondary(unsigned int cpu) { return 0; }
static inline void emev2_smp_prepare_cpus(void) {}
#endif

#define EMEV2_GPIO_BASE 200
#define EMEV2_GPIO_IRQ(n) (EMEV2_GPIO_BASE + (n))

#endif /* __ASM_EMEV2_H__ */
