/*
 * OMAP4 clock function prototypes and macros
 *
 * Copyright (C) 2009 Texas Instruments, Inc.
 * Copyright (C) 2010 Nokia Corporation
 */

#ifndef __ARCH_ARM_MACH_OMAP2_CLOCK44XX_H
#define __ARCH_ARM_MACH_OMAP2_CLOCK44XX_H

/*
 * OMAP4430_REGM4XEN_MULT: If the CM_CLKMODE_DPLL_ABE.DPLL_REGM4XEN bit is
 *    set, then the DPLL's lock frequency is multiplied by 4 (OMAP4430 TRM
 *    vV Section 3.6.3.3.1 "DPLLs Output Clocks Parameters")
 */
#define OMAP4430_REGM4XEN_MULT	4

int omap4xxx_clk_init(void);
int omap4_core_dpll_m2_set_rate(struct clk *clk, unsigned long rate);
long omap4_dpll_regm4xen_round_rate(struct clk *clk, unsigned long target_rate);
unsigned long omap4_dpll_regm4xen_recalc(struct clk *clk);
int omap4_core_dpll_m5x2_set_rate(struct clk *clk, unsigned long rate);
#endif
