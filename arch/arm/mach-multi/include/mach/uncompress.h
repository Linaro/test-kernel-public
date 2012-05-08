#ifndef __MACH_UNCOMPRESS_H
#define __MACH_UNCOMPRESS_H

/* no-op dummies for the uncompress debugging for multiplatform kernels */

static inline void putc(int c) {}
static inline void flush(void) {}
static inline void arch_decomp_setup(void) {}

#endif
