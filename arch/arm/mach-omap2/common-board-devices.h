#ifndef __OMAP_COMMON_BOARD_DEVICES__
#define __OMAP_COMMON_BOARD_DEVICES__

#include <linux/emif.h>
#include "twl-common.h"

#define NAND_BLOCK_SIZE	SZ_128K

struct mtd_partition;
struct ads7846_platform_data;

void omap_ads7846_init(int bus_num, int gpio_pendown, int gpio_debounce,
		       struct ads7846_platform_data *board_pdata);
void omap_nand_flash_init(int opts, struct mtd_partition *parts, int n_parts);

extern struct ddr_device_info lpddr2_elpida_2G_S4_x2_info;
extern struct lpddr2_timings lpddr2_elpida_2G_S4_timings[2];

extern struct ddr_device_info lpddr2_elpida_4G_S4_x2_info;
extern struct lpddr2_timings lpddr2_elpida_4G_S4_timings[2];

extern struct ddr_min_tck lpddr2_elpida_S4_min_tck;

#endif /* __OMAP_COMMON_BOARD_DEVICES__ */
