/* linux/arch/arm/mach-exynos4/mach-origen.c
 *
 * Copyright (c) 2011 Insignal Co., Ltd.
 *		http://www.insignal.co.kr/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/serial_core.h>
#include <linux/leds.h>
#include <linux/gpio.h>
#include <linux/mmc/host.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/input.h>
#include <linux/pwm_backlight.h>
#include <linux/gpio_keys.h>
#include <linux/i2c.h>
#include <linux/regulator/machine.h>
#include <linux/mfd/max8997.h>
#include <linux/lcd.h>
#include <linux/rfkill-gpio.h>
#include <linux/ath6kl.h>
#include <linux/delay.h>

#include <asm/mach/arch.h>
#include <asm/mach-types.h>

#include <video/platform_lcd.h>

#include <plat/regs-serial.h>
#include <plat/regs-fb-v4.h>
#include <plat/exynos4.h>
#include <plat/cpu.h>
#include <plat/devs.h>
#include <plat/sdhci.h>
#include <plat/iic.h>
#include <plat/ehci.h>
#include <plat/clock.h>
#include <plat/gpio-cfg.h>
#include <plat/backlight.h>
#include <plat/pd.h>
#include <plat/fb.h>
#include <plat/mfc.h>
#include <plat/otg.h>

#include <mach/ohci.h>
#include <mach/map.h>

/* Following are default values for UCON, ULCON and UFCON UART registers */
#define ORIGEN_UCON_DEFAULT	(S3C2410_UCON_TXILEVEL |	\
				 S3C2410_UCON_RXILEVEL |	\
				 S3C2410_UCON_TXIRQMODE |	\
				 S3C2410_UCON_RXIRQMODE |	\
				 S3C2410_UCON_RXFIFO_TOI |	\
				 S3C2443_UCON_RXERR_IRQEN)

#define ORIGEN_ULCON_DEFAULT	S3C2410_LCON_CS8

#define ORIGEN_UFCON_DEFAULT	(S3C2410_UFCON_FIFOMODE |	\
				 S5PV210_UFCON_TXTRIG4 |	\
				 S5PV210_UFCON_RXTRIG4)

static struct s3c2410_uartcfg origen_uartcfgs[] __initdata = {
	[0] = {
		.hwport		= 0,
		.flags		= 0,
		.ucon		= ORIGEN_UCON_DEFAULT,
		.ulcon		= ORIGEN_ULCON_DEFAULT,
		.ufcon		= ORIGEN_UFCON_DEFAULT,
	},
	[1] = {
		.hwport		= 1,
		.flags		= 0,
		.ucon		= ORIGEN_UCON_DEFAULT,
		.ulcon		= ORIGEN_ULCON_DEFAULT,
		.ufcon		= ORIGEN_UFCON_DEFAULT,
	},
	[2] = {
		.hwport		= 2,
		.flags		= 0,
		.ucon		= ORIGEN_UCON_DEFAULT,
		.ulcon		= ORIGEN_ULCON_DEFAULT,
		.ufcon		= ORIGEN_UFCON_DEFAULT,
	},
	[3] = {
		.hwport		= 3,
		.flags		= 0,
		.ucon		= ORIGEN_UCON_DEFAULT,
		.ulcon		= ORIGEN_ULCON_DEFAULT,
		.ufcon		= ORIGEN_UFCON_DEFAULT,
	},
};

static struct regulator_consumer_supply __initdata ldo3_consumer[] = {
	REGULATOR_SUPPLY("vdd11", "s5p-mipi-csis.0"), /* MIPI */
	REGULATOR_SUPPLY("vdd", "exynos4-hdmi"), /* HDMI */
	REGULATOR_SUPPLY("vdd_pll", "exynos4-hdmi"), /* HDMI */
};
static struct regulator_consumer_supply __initdata ldo6_consumer[] = {
	REGULATOR_SUPPLY("vdd18", "s5p-mipi-csis.0"), /* MIPI */
};
static struct regulator_consumer_supply __initdata ldo7_consumer[] = {
	REGULATOR_SUPPLY("avdd", "alc5625"), /* Realtek ALC5625 */
};
static struct regulator_consumer_supply __initdata ldo8_consumer[] = {
	REGULATOR_SUPPLY("vdd", "s5p-adc"), /* ADC */
	REGULATOR_SUPPLY("vdd_osc", "exynos4-hdmi"), /* HDMI */
};
static struct regulator_consumer_supply __initdata ldo9_consumer[] = {
	REGULATOR_SUPPLY("dvdd", "swb-a31"), /* AR6003 WLAN & CSR 8810 BT */
};
static struct regulator_consumer_supply __initdata ldo11_consumer[] = {
	REGULATOR_SUPPLY("dvdd", "alc5625"), /* Realtek ALC5625 */
};
static struct regulator_consumer_supply __initdata ldo14_consumer[] = {
	REGULATOR_SUPPLY("avdd18", "swb-a31"), /* AR6003 WLAN & CSR 8810 BT */
};
static struct regulator_consumer_supply __initdata ldo17_consumer[] = {
	REGULATOR_SUPPLY("vdd33", "swb-a31"), /* AR6003 WLAN & CSR 8810 BT */
};
static struct regulator_consumer_supply __initdata buck1_consumer[] = {
	REGULATOR_SUPPLY("vdd_arm", NULL), /* CPUFREQ */
};
static struct regulator_consumer_supply __initdata buck2_consumer[] = {
	REGULATOR_SUPPLY("vdd_int", NULL), /* CPUFREQ */
};
static struct regulator_consumer_supply __initdata buck3_consumer[] = {
	REGULATOR_SUPPLY("vdd_g3d", "mali_drm"), /* G3D */
};
static struct regulator_consumer_supply __initdata buck7_consumer[] = {
	REGULATOR_SUPPLY("vcc_lcd", "platform-lcd.0"), /* LCD */
};

static struct regulator_init_data __initdata max8997_ldo1_data = {
	.constraints	= {
		.name		= "VDD_ABB_3.3V",
		.min_uV		= 3300000,
		.max_uV		= 3300000,
		.apply_uV	= 1,
		.state_mem	= {
			.disabled	= 1,
		},
	},
};

static struct regulator_init_data __initdata max8997_ldo2_data	= {
	.constraints	= {
		.name		= "VDD_ALIVE_1.1V",
		.min_uV		= 1100000,
		.max_uV		= 1100000,
		.apply_uV	= 1,
		.always_on	= 1,
		.state_mem	= {
			.enabled	= 1,
		},
	},
};

static struct regulator_init_data __initdata max8997_ldo3_data = {
	.constraints	= {
		.name		= "VMIPI_1.1V",
		.min_uV		= 1100000,
		.max_uV		= 1100000,
		.apply_uV	= 1,
		.valid_ops_mask	= REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled	= 1,
		},
	},
	.num_consumer_supplies	= ARRAY_SIZE(ldo3_consumer),
	.consumer_supplies	= ldo3_consumer,
};

static struct regulator_init_data __initdata max8997_ldo4_data = {
	.constraints	= {
		.name		= "VDD_RTC_1.8V",
		.min_uV		= 1800000,
		.max_uV		= 1800000,
		.apply_uV	= 1,
		.always_on	= 1,
		.state_mem	= {
			.disabled	= 1,
		},
	},
};

static struct regulator_init_data __initdata max8997_ldo6_data = {
	.constraints	= {
		.name		= "VMIPI_1.8V",
		.min_uV		= 1800000,
		.max_uV		= 1800000,
		.apply_uV	= 1,
		.valid_ops_mask	= REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled	= 1,
		},
	},
	.num_consumer_supplies	= ARRAY_SIZE(ldo6_consumer),
	.consumer_supplies	= ldo6_consumer,
};

static struct regulator_init_data __initdata max8997_ldo7_data = {
	.constraints	= {
		.name		= "VDD_AUD_1.8V",
		.min_uV		= 1800000,
		.max_uV		= 1800000,
		.apply_uV	= 1,
		.always_on	= 1,
		.valid_ops_mask	= REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled	= 1,
		},
	},
	.num_consumer_supplies	= ARRAY_SIZE(ldo7_consumer),
	.consumer_supplies	= ldo7_consumer,
};

static struct regulator_init_data __initdata max8997_ldo8_data = {
	.constraints	= {
		.name		= "VADC_3.3V",
		.min_uV		= 3300000,
		.max_uV		= 3300000,
		.apply_uV	= 1,
		.valid_ops_mask	= REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled	= 1,
		},
	},
	.num_consumer_supplies	= ARRAY_SIZE(ldo8_consumer),
	.consumer_supplies	= ldo8_consumer,
};

static struct regulator_init_data __initdata max8997_ldo9_data = {
	.constraints	= {
		.name		= "DVDD_SWB_2.8V",
		.min_uV		= 2800000,
		.max_uV		= 2800000,
		.apply_uV	= 1,
		.always_on	= 1,
		.valid_ops_mask	= REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled	= 1,
		},
	},
	.num_consumer_supplies	= ARRAY_SIZE(ldo9_consumer),
	.consumer_supplies	= ldo9_consumer,
};

static struct regulator_init_data __initdata max8997_ldo10_data = {
	.constraints	= {
		.name		= "VDD_PLL_1.1V",
		.min_uV		= 1100000,
		.max_uV		= 1100000,
		.apply_uV	= 1,
		.always_on	= 1,
		.state_mem	= {
			.disabled	= 1,
		},
	},
};

static struct regulator_init_data __initdata max8997_ldo11_data = {
	.constraints	= {
		.name		= "VDD_AUD_3V",
		.min_uV		= 3000000,
		.max_uV		= 3000000,
		.apply_uV	= 1,
		.always_on	= 1,
		.valid_ops_mask	= REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled	= 1,
		},
	},
	.num_consumer_supplies	= ARRAY_SIZE(ldo11_consumer),
	.consumer_supplies	= ldo11_consumer,
};

static struct regulator_init_data __initdata max8997_ldo14_data = {
	.constraints	= {
		.name		= "AVDD18_SWB_1.8V",
		.min_uV		= 1800000,
		.max_uV		= 1800000,
		.apply_uV	= 1,
		.always_on	= 1,
		.valid_ops_mask	= REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled	= 1,
		},
	},
	.num_consumer_supplies	= ARRAY_SIZE(ldo14_consumer),
	.consumer_supplies	= ldo14_consumer,
};

static struct regulator_init_data __initdata max8997_ldo17_data = {
	.constraints	= {
		.name		= "VDD_SWB_3.3V",
		.min_uV		= 3300000,
		.max_uV		= 3300000,
		.apply_uV	= 1,
		.always_on	= 1,
		.valid_ops_mask	= REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled	= 1,
		},
	},
	.num_consumer_supplies	= ARRAY_SIZE(ldo17_consumer),
	.consumer_supplies	= ldo17_consumer,
};

static struct regulator_init_data __initdata max8997_ldo21_data = {
	.constraints	= {
		.name		= "VDD_MIF_1.2V",
		.min_uV		= 1200000,
		.max_uV		= 1200000,
		.apply_uV	= 1,
		.always_on	= 1,
		.state_mem	= {
			.disabled	= 1,
		},
	},
};

static struct regulator_init_data __initdata max8997_buck1_data = {
	.constraints	= {
		.name		= "VDD_ARM_1.2V",
		.min_uV		= 950000,
		.max_uV		= 1350000,
		.always_on	= 1,
		.boot_on	= 1,
		.valid_ops_mask	= REGULATOR_CHANGE_VOLTAGE,
		.state_mem	= {
			.disabled	= 1,
		},
	},
	.num_consumer_supplies	= ARRAY_SIZE(buck1_consumer),
	.consumer_supplies	= buck1_consumer,
};

static struct regulator_init_data __initdata max8997_buck2_data = {
	.constraints	= {
		.name		= "VDD_INT_1.1V",
		.min_uV		= 900000,
		.max_uV		= 1100000,
		.always_on	= 1,
		.boot_on	= 1,
		.valid_ops_mask	= REGULATOR_CHANGE_VOLTAGE,
		.state_mem	= {
			.disabled	= 1,
		},
	},
	.num_consumer_supplies	= ARRAY_SIZE(buck2_consumer),
	.consumer_supplies	= buck2_consumer,
};

static struct regulator_init_data __initdata max8997_buck3_data = {
	.constraints	= {
		.name		= "VDD_G3D_1.1V",
		.min_uV		= 900000,
		.max_uV		= 1100000,
		.valid_ops_mask	= REGULATOR_CHANGE_VOLTAGE |
					REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled	= 1,
		},
	},
	.num_consumer_supplies	= ARRAY_SIZE(buck3_consumer),
	.consumer_supplies	= buck3_consumer,
};

static struct regulator_init_data __initdata max8997_buck5_data = {
	.constraints	= {
		.name		= "VDDQ_M1M2_1.2V",
		.min_uV		= 1200000,
		.max_uV		= 1200000,
		.apply_uV	= 1,
		.always_on	= 1,
		.state_mem	= {
			.disabled	= 1,
		},
	},
};

static struct regulator_init_data __initdata max8997_buck7_data = {
	.constraints	= {
		.name		= "VDD_LCD_3.3V",
		.min_uV		= 750000,
		.max_uV		= 3900000,
		.boot_on	= 1,
		.valid_ops_mask	= REGULATOR_CHANGE_STATUS |
					REGULATOR_CHANGE_VOLTAGE,
		.state_mem	= {
			.disabled	= 1
		},
	},
	.num_consumer_supplies	= ARRAY_SIZE(buck7_consumer),
	.consumer_supplies	= buck7_consumer,
};

static struct max8997_regulator_data __initdata origen_max8997_regulators[] = {
	{ MAX8997_LDO1,		&max8997_ldo1_data },
	{ MAX8997_LDO2,		&max8997_ldo2_data },
	{ MAX8997_LDO3,		&max8997_ldo3_data },
	{ MAX8997_LDO4,		&max8997_ldo4_data },
	{ MAX8997_LDO6,		&max8997_ldo6_data },
	{ MAX8997_LDO7,		&max8997_ldo7_data },
	{ MAX8997_LDO8,		&max8997_ldo8_data },
	{ MAX8997_LDO9,		&max8997_ldo9_data },
	{ MAX8997_LDO10,	&max8997_ldo10_data },
	{ MAX8997_LDO11,	&max8997_ldo11_data },
	{ MAX8997_LDO14,	&max8997_ldo14_data },
	{ MAX8997_LDO17,	&max8997_ldo17_data },
	{ MAX8997_LDO21,	&max8997_ldo21_data },
	{ MAX8997_BUCK1,	&max8997_buck1_data },
	{ MAX8997_BUCK2,	&max8997_buck2_data },
	{ MAX8997_BUCK3,	&max8997_buck3_data },
	{ MAX8997_BUCK5,	&max8997_buck5_data },
	{ MAX8997_BUCK7,	&max8997_buck7_data },
};

struct max8997_platform_data __initdata origen_max8997_pdata = {
	.num_regulators = ARRAY_SIZE(origen_max8997_regulators),
	.regulators	= origen_max8997_regulators,

	.wakeup	= true,
	.buck1_gpiodvs	= false,
	.buck2_gpiodvs	= false,
	.buck5_gpiodvs	= false,
	.irq_base	= IRQ_GPIO_END + 1,

	.ignore_gpiodvs_side_effect = true,
	.buck125_default_idx = 0x0,

	.buck125_gpios[0]	= EXYNOS4_GPX0(0),
	.buck125_gpios[1]	= EXYNOS4_GPX0(1),
	.buck125_gpios[2]	= EXYNOS4_GPX0(2),

	.buck1_voltage[0]	= 1350000,
	.buck1_voltage[1]	= 1300000,
	.buck1_voltage[2]	= 1250000,
	.buck1_voltage[3]	= 1200000,
	.buck1_voltage[4]	= 1150000,
	.buck1_voltage[5]	= 1100000,
	.buck1_voltage[6]	= 1000000,
	.buck1_voltage[7]	= 950000,

	.buck2_voltage[0]	= 1100000,
	.buck2_voltage[1]	= 1100000,
	.buck2_voltage[2]	= 1100000,
	.buck2_voltage[3]	= 1100000,
	.buck2_voltage[4]	= 1000000,
	.buck2_voltage[5]	= 1000000,
	.buck2_voltage[6]	= 1000000,
	.buck2_voltage[7]	= 1000000,

	.buck5_voltage[0]	= 1200000,
	.buck5_voltage[1]	= 1200000,
	.buck5_voltage[2]	= 1200000,
	.buck5_voltage[3]	= 1200000,
	.buck5_voltage[4]	= 1200000,
	.buck5_voltage[5]	= 1200000,
	.buck5_voltage[6]	= 1200000,
	.buck5_voltage[7]	= 1200000,
};

/* I2C0 */
static struct i2c_board_info i2c0_devs[] __initdata = {
	{
		I2C_BOARD_INFO("max8997", (0xCC >> 1)),
		.platform_data	= &origen_max8997_pdata,
		.irq		= IRQ_EINT(4),
	},
#ifdef CONFIG_TOUCHSCREEN_UNIDISPLAY_TS
	{
		I2C_BOARD_INFO("unidisplay_ts", 0x41),
		.irq = IRQ_TS,
	},
#endif
};

/* I2C1 */
static struct i2c_board_info i2c1_devs[] __initdata = {
	{
		I2C_BOARD_INFO("alc5625", 0x1E),
	},
};

static struct s3c_sdhci_platdata origen_hsmmc0_pdata __initdata = {
	.cd_type		= S3C_SDHCI_CD_INTERNAL,
	.clk_type		= S3C_SDHCI_CLK_DIV_EXTERNAL,
};

static struct s3c_sdhci_platdata origen_hsmmc2_pdata __initdata = {
	.cd_type		= S3C_SDHCI_CD_INTERNAL,
	.clk_type		= S3C_SDHCI_CLK_DIV_EXTERNAL,
};


/*
 * WLAN: SDIO Host will call this func at booting time
 */
static int origen_wifi_status_register(void (*notify_func)
		(struct platform_device *, int state));

/* WLAN: MMC3-SDIO */
static struct s3c_sdhci_platdata origen_hsmmc3_pdata __initdata = {
	.max_width		= 4,
	.host_caps		= MMC_CAP_4_BIT_DATA |
			MMC_CAP_MMC_HIGHSPEED | MMC_CAP_SD_HIGHSPEED,
	.cd_type		= S3C_SDHCI_CD_EXTERNAL,
	.ext_cd_init		= origen_wifi_status_register,
};

/*
 * WLAN: Save SDIO Card detect func into this pointer
 */
static void (*wifi_status_cb)(struct platform_device *, int state);

static int origen_wifi_status_register(void (*notify_func)
		(struct platform_device *, int state))
{
	if (!notify_func)
		return -EAGAIN;
	else
		wifi_status_cb = notify_func;

	return 0;
}

#define ORIGEN_WLAN_WOW EXYNOS4_GPX2(3)
#define ORIGEN_WLAN_RESET EXYNOS4_GPX2(4)


static void origen_wlan_setup_power(bool val)
{
	int err;

	if (val) {
		err = gpio_request_one(ORIGEN_WLAN_RESET,
				GPIOF_OUT_INIT_LOW, "GPX2_4");
		if (err) {
			pr_warning("ORIGEN: Not obtain WIFI gpios\n");
			return;
		}
		s3c_gpio_cfgpin(ORIGEN_WLAN_RESET, S3C_GPIO_OUTPUT);
		s3c_gpio_setpull(ORIGEN_WLAN_RESET,
						S3C_GPIO_PULL_NONE);
		/* VDD33,I/O Supply must be done */
		gpio_set_value(ORIGEN_WLAN_RESET, 0);
		udelay(30);	/*Tb */
		gpio_direction_output(ORIGEN_WLAN_RESET, 1);
	} else {
		gpio_direction_output(ORIGEN_WLAN_RESET, 0);
		gpio_free(ORIGEN_WLAN_RESET);
	}

	mdelay(100);

	return;
}

/*
 * This will be called at init time of WLAN driver
 */
static int origen_wifi_set_detect(bool val)
{
	if (!wifi_status_cb) {
		printk(KERN_WARNING "WLAN: Nobody to notify\n");
		return -EAGAIN;
	}
	if (true == val) {
		origen_wlan_setup_power(true);
		wifi_status_cb(&s3c_device_hsmmc3, 1);
	} else {
		origen_wlan_setup_power(false);
		wifi_status_cb(&s3c_device_hsmmc3, 0);
	}

	return 0;
}

struct ath6kl_platform_data origen_wlan_data  __initdata = {
	.setup_power = origen_wifi_set_detect,
};


/* USB EHCI */
static struct s5p_ehci_platdata origen_ehci_pdata;

static void __init origen_ehci_init(void)
{
	struct s5p_ehci_platdata *pdata = &origen_ehci_pdata;

	s5p_ehci_set_platdata(pdata);
}

/* USB OHCI */
static struct exynos4_ohci_platdata origen_ohci_pdata;

static void __init origen_ohci_init(void)
{
	struct exynos4_ohci_platdata *pdata = &origen_ohci_pdata;

	exynos4_ohci_set_platdata(pdata);
};

/* USB OTG */
static struct s5p_otg_platdata origen_otg_pdata;

static void __init origen_otg_init(void)
{
	struct s5p_otg_platdata *pdata = &origen_otg_pdata;

	s5p_otg_set_platdata(pdata);
}

static struct gpio_led origen_gpio_leds[] = {
	{
		.name			= "origen::status1",
		.default_trigger	= "heartbeat",
		.gpio			= EXYNOS4_GPX1(3),
		.active_low		= 1,
	},
	{
		.name			= "origen::status2",
		.default_trigger	= "mmc0",
		.gpio			= EXYNOS4_GPX1(4),
		.active_low		= 1,
	},
};

static struct gpio_led_platform_data origen_gpio_led_info = {
	.leds		= origen_gpio_leds,
	.num_leds	= ARRAY_SIZE(origen_gpio_leds),
};

static struct platform_device origen_leds_gpio = {
	.name	= "leds-gpio",
	.id	= -1,
	.dev	= {
		.platform_data	= &origen_gpio_led_info,
	},
};

static struct gpio_keys_button origen_gpio_keys_table[] = {
	{
		.code			= KEY_MENU,
		.gpio			= EXYNOS4_GPX1(5),
		.desc			= "gpio-keys: KEY_MENU",
		.type			= EV_KEY,
		.active_low		= 1,
		.wakeup			= 1,
		.debounce_interval	= 1,
	}, {
		.code			= KEY_HOME,
		.gpio			= EXYNOS4_GPX1(6),
		.desc			= "gpio-keys: KEY_HOME",
		.type			= EV_KEY,
		.active_low		= 1,
		.wakeup			= 1,
		.debounce_interval	= 1,
	}, {
		.code			= KEY_BACK,
		.gpio			= EXYNOS4_GPX1(7),
		.desc			= "gpio-keys: KEY_BACK",
		.type			= EV_KEY,
		.active_low		= 1,
		.wakeup			= 1,
		.debounce_interval	= 1,
	}, {
		.code			= KEY_UP,
		.gpio			= EXYNOS4_GPX2(0),
		.desc			= "gpio-keys: KEY_UP",
		.type			= EV_KEY,
		.active_low		= 1,
		.wakeup			= 1,
		.debounce_interval	= 1,
	}, {
		.code			= KEY_DOWN,
		.gpio			= EXYNOS4_GPX2(1),
		.desc			= "gpio-keys: KEY_DOWN",
		.type			= EV_KEY,
		.active_low		= 1,
		.wakeup			= 1,
		.debounce_interval	= 1,
	},
};

static struct gpio_keys_platform_data origen_gpio_keys_data = {
	.buttons	= origen_gpio_keys_table,
	.nbuttons	= ARRAY_SIZE(origen_gpio_keys_table),
};

static struct platform_device origen_device_gpiokeys = {
	.name		= "gpio-keys",
	.dev		= {
		.platform_data	= &origen_gpio_keys_data,
	},
};

static void lcd_hv070wsa_set_power(struct plat_lcd_data *pd, unsigned int power)
{
	int ret;

	if (power)
		ret = gpio_request_one(EXYNOS4_GPE3(4),
					GPIOF_OUT_INIT_HIGH, "GPE3_4");
	else
		ret = gpio_request_one(EXYNOS4_GPE3(4),
					GPIOF_OUT_INIT_LOW, "GPE3_4");

	gpio_free(EXYNOS4_GPE3(4));

	if (ret)
		pr_err("failed to request gpio for LCD power: %d\n", ret);
}

static struct plat_lcd_data origen_lcd_hv070wsa_data = {
	.set_power = lcd_hv070wsa_set_power,
	.min_uV		= 3300000,
	.max_uV		= 3300000,
};

static struct platform_device origen_lcd_hv070wsa = {
	.name			= "platform-lcd",
	.dev.parent		= &s5p_device_fimd0.dev,
	.dev.platform_data	= &origen_lcd_hv070wsa_data,
};

static struct s3c_fb_pd_win origen_fb_win0 = {
	.win_mode = {
		.left_margin	= 64,
		.right_margin	= 16,
		.upper_margin	= 64,
		.lower_margin	= 16,
		.hsync_len	= 48,
		.vsync_len	= 3,
		.xres		= 1024,
		.yres		= 600,
	},
	.max_bpp		= 32,
	.default_bpp		= 24,
};

static struct s3c_fb_platdata origen_lcd_pdata __initdata = {
	.win[0]		= &origen_fb_win0,
	.vidcon0	= VIDCON0_VIDOUT_RGB | VIDCON0_PNRMODE_RGB,
	.vidcon1	= VIDCON1_INV_HSYNC | VIDCON1_INV_VSYNC |
				VIDCON1_INV_VCLK,
	.setup_gpio	= exynos4_fimd0_gpio_setup_24bpp,
};

/* Bluetooth rfkill gpio platform data */
struct rfkill_gpio_platform_data origen_bt_pdata = {
	.reset_gpio	= EXYNOS4_GPX2(2),
	.shutdown_gpio	= -1,
	.type		= RFKILL_TYPE_BLUETOOTH,
	.name		= "origen-bt",
};

/* Bluetooth Platform device */
static struct platform_device origen_device_bluetooth = {
	.name		= "rfkill_gpio",
	.id		= -1,
	.dev		= {
		.platform_data	= &origen_bt_pdata,
	},
};

static struct platform_device *origen_devices[] __initdata = {
	&s3c_device_hsmmc2,
	&s3c_device_hsmmc0,
	&s3c_device_hsmmc3,
	&s3c_device_i2c0,
	&s3c_device_i2c1,
	&s3c_device_rtc,
	&s3c_device_usbgadget,
	&s3c_device_wdt,
	&s5p_device_ehci,
	&s5p_device_fimc0,
	&s5p_device_fimc1,
	&s5p_device_fimc2,
	&s5p_device_fimc3,
	&s5p_device_fimc_md,
	&s5p_device_fimd0,
	&s5p_device_g2d,
	&s5p_device_hdmi,
	&s5p_device_i2c_hdmiphy,
#ifdef CONFIG_VIDEO_JPEG
	&s5p_device_jpeg,
#endif
	&s5p_device_mfc,
	&s5p_device_mfc_l,
	&s5p_device_mfc_r,
	&s5p_device_mixer,
	&samsung_asoc_dma,
	&exynos4_device_i2s0,
	&exynos4_device_ohci,
	&exynos4_device_pd[PD_LCD0],
	&exynos4_device_pd[PD_TV],
	&exynos4_device_pd[PD_G3D],
	&exynos4_device_pd[PD_LCD1],
	&exynos4_device_pd[PD_CAM],
	&exynos4_device_pd[PD_GPS],
	&exynos4_device_pd[PD_MFC],
	&origen_device_gpiokeys,
	&origen_lcd_hv070wsa,
	&origen_leds_gpio,
	&origen_device_bluetooth,
	&exynos4_device_tmu,
};

/* LCD Backlight data */
static struct samsung_bl_gpio_info origen_bl_gpio_info = {
	.no		= EXYNOS4_GPD0(0),
	.func		= S3C_GPIO_SFN(2),
};

static struct platform_pwm_backlight_data origen_bl_data = {
	.pwm_id		= 0,
	.pwm_period_ns	= 1000,
};

static void __init origen_bt_setup(void)
{
	gpio_request(EXYNOS4_GPA0(0), "GPIO BT_UART");
	/* 4 UART Pins configuration */
	s3c_gpio_cfgrange_nopull(EXYNOS4_GPA0(0), 4, S3C_GPIO_SFN(2));
	/* Setup BT Reset, this gpio will be requesed by rfkill-gpio */
	s3c_gpio_cfgpin(EXYNOS4_GPX2(2), S3C_GPIO_OUTPUT);
	s3c_gpio_setpull(EXYNOS4_GPX2(2), S3C_GPIO_PULL_NONE);
}

static void s5p_tv_setup(void)
{
	/* Direct HPD to HDMI chip */
	gpio_request_one(EXYNOS4_GPX3(7), GPIOF_IN, "hpd-plug");
	s3c_gpio_cfgpin(EXYNOS4_GPX3(7), S3C_GPIO_SFN(0x3));
	s3c_gpio_setpull(EXYNOS4_GPX3(7), S3C_GPIO_PULL_NONE);
}

static void __init origen_map_io(void)
{
	s5p_init_io(NULL, 0, S5P_VA_CHIPID);
	s3c24xx_init_clocks(24000000);
	s3c24xx_init_uarts(origen_uartcfgs, ARRAY_SIZE(origen_uartcfgs));
}

static void __init origen_power_init(void)
{
	gpio_request(EXYNOS4_GPX0(4), "PMIC_IRQ");
	s3c_gpio_cfgpin(EXYNOS4_GPX0(4), S3C_GPIO_SFN(0xf));
	s3c_gpio_setpull(EXYNOS4_GPX0(4), S3C_GPIO_PULL_NONE);
}

#if defined(CONFIG_S5P_MEM_CMA)
static void __init exynos4_reserve_cma(void)
{
	static struct cma_region regions[] = {
#if defined(CONFIG_VIDEO_JPEG)
		{
			.name = "jpeg",
			.size = CONFIG_VIDEO_SAMSUNG_MEMSIZE_JPEG * SZ_1K,
			.start = 0
		},
#endif
		{
			.name = "common",
			.size = CONFIG_CMA_COMMON_MEMORY_SIZE * SZ_1K,
			.start = 0
		},
		{}
	};
	static const char map[] __initconst =
#if defined(CONFIG_VIDEO_JPEG)
		"s5p-jpeg=jpeg;"
#endif
		"*=common";

	int i = 0;
	unsigned int bank0_end = meminfo.bank[0].start +
					meminfo.bank[0].size;
	unsigned int bank1_end = meminfo.bank[1].start +
					meminfo.bank[1].size;

	for (; i < ARRAY_SIZE(regions) ; i++) {
		if (regions[i].start == 0) {
			regions[i].start = bank0_end - regions[i].size;
			bank0_end = regions[i].start;
		} else if (regions[i].start == 1) {
			regions[i].start = bank1_end - regions[i].size;
			bank1_end = regions[i].start;
		}
		pr_err("CMA reserve : %s, addr is 0x%x, size is 0x%x\n",
			regions[i].name, regions[i].start, regions[i].size);
	}

	cma_set_defaults(regions, map);
	cma_early_regions_reserve(NULL);
}
#else
static void __init exynos4_reserve_cma(void) {}
#endif

static void __init origen_reserve(void)
{
	s5p_mfc_reserve_mem(0x43000000, 8 << 20, 0x51000000, 8 << 20);
	exynos4_reserve_cma();
}

static void __init origen_machine_init(void)
{
	origen_power_init();

	s3c_i2c0_set_platdata(NULL);
	i2c_register_board_info(0, i2c0_devs, ARRAY_SIZE(i2c0_devs));

	s3c_i2c1_set_platdata(NULL);
	i2c_register_board_info(1, i2c1_devs, ARRAY_SIZE(i2c1_devs));

	/*
	 * Since sdhci instance 2 can contain a bootable media,
	 * sdhci instance 0 is registered after instance 2.
	 */
	s3c_sdhci2_set_platdata(&origen_hsmmc2_pdata);
	s3c_sdhci0_set_platdata(&origen_hsmmc0_pdata);
	s3c_sdhci3_set_platdata(&origen_hsmmc3_pdata);

	origen_ehci_init();
	origen_ohci_init();
	origen_otg_init();
	clk_xusbxti.rate = 24000000;

	s5p_tv_setup();
	s5p_i2c_hdmiphy_set_platdata(NULL);

	s5p_fimd0_set_platdata(&origen_lcd_pdata);

	platform_add_devices(origen_devices, ARRAY_SIZE(origen_devices));

	s5p_device_fimd0.dev.parent = &exynos4_device_pd[PD_LCD0].dev;

	s5p_device_hdmi.dev.parent = &exynos4_device_pd[PD_TV].dev;
	s5p_device_mixer.dev.parent = &exynos4_device_pd[PD_TV].dev;

	s5p_device_mfc.dev.parent = &exynos4_device_pd[PD_MFC].dev;

	samsung_bl_set(&origen_bl_gpio_info, &origen_bl_data);

	origen_bt_setup();

	ath6kl_set_platform_data(&origen_wlan_data);
}

MACHINE_START(ORIGEN, "ORIGEN")
	/* Maintainer: JeongHyeon Kim <jhkim@insignal.co.kr> */
	.atag_offset	= 0x100,
	.init_irq	= exynos4_init_irq,
	.map_io		= origen_map_io,
	.init_machine	= origen_machine_init,
	.timer		= &exynos4_timer,
	.reserve	= &origen_reserve,
MACHINE_END
