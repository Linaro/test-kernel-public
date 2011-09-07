/*
 * linux/drivers/input/keyboard/insignal-keypad.c
 *
 * Keypad Driver for Torbreck Board
 *
 * Author : Jhoonkim <jhoon_kim@nate.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/input.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/device.h>
#include <linux/irq.h>

#include <asm/io.h>
#include <asm/delay.h>
#include <asm/irq.h>

#include <mach/map.h>
#include <mach/gpio.h>
#include <plat/gpio-cfg.h>
#include <mach/regs-gpio.h>

#define DEVICE_NAME		"insignal-keypad"
#define MAX_KEYPAD_CNT		5

//#define USE_DEBUG

#define EXYNOS4_GPX1DAT (S5P_VA_GPIO2 + 0x0c24)
#define EXYNOS4_GPX2DAT (S5P_VA_GPIO2 + 0x0c44)

#define	IRQ_KEY_MENU		EINT_NUMBER(13)
#define	IRQ_KEY_HOME		EINT_NUMBER(14)
#define	IRQ_KEY_BACK		EINT_NUMBER(15)
#define	IRQ_KEY_VOLUMEUP	EINT_NUMBER(16)
#define	IRQ_KEY_VOLUMEDOWN	EINT_NUMBER(17)

#ifdef USE_DEBUG
#define DPRINTK(fmt, args...) \
	printk(fmt, ## args)
#else
#define DPRINTK(fmt, args...) \
	({ do {} while (0); 0; })
#endif

static struct input_dev	*insignal_keypad;

static int insignal_keypad_open(struct input_dev *dev);
static void insignal_keypad_close(struct input_dev *dev);

static void insignal_keypad_release_device(struct device *dev);
static int insignal_keypad_resume(struct device *dev);
static int insignal_keypad_suspend(struct device *dev, pm_message_t state);

static int __devinit insignal_keypad_probe(struct device *pdev);
static int __devexit insignal_keypad_remove(struct device *pdev);

static int __init insignal_keypad_init(void);
static void __exit insignal_keypad_exit(void);

static void insignal_keypad_gpio_cfg(void);

unsigned int insignal_keypad_keycode_map[MAX_KEYPAD_CNT] = {
	KEY_MENU, 
	KEY_HOME,
	KEY_BACK,
	KEY_VOLUMEUP,
	KEY_VOLUMEDOWN,
};

static void insignal_keypad_gpio_cfg(void)
{
	/* Push Key Button 1 */
	s3c_gpio_cfgpin(EXYNOS4_GPX1(5), S3C_GPIO_SFN(0));
	s3c_gpio_setpull(EXYNOS4_GPX1(5), S3C_GPIO_PULL_NONE);

	/* Push Key Button 2 */
	s3c_gpio_cfgpin(EXYNOS4_GPX1(6), S3C_GPIO_SFN(0));
	s3c_gpio_setpull(EXYNOS4_GPX1(6), S3C_GPIO_PULL_NONE);

	/* Push Key Button 3 */
	s3c_gpio_cfgpin(EXYNOS4_GPX1(7), S3C_GPIO_SFN(0));
	s3c_gpio_setpull(EXYNOS4_GPX1(7), S3C_GPIO_PULL_NONE);

	/* Push Key Button 4 */
	s3c_gpio_cfgpin(EXYNOS4_GPX2(0), S3C_GPIO_SFN(0));
	s3c_gpio_setpull(EXYNOS4_GPX2(0), S3C_GPIO_PULL_NONE);

	/* Push Key Button 4 */
	s3c_gpio_cfgpin(EXYNOS4_GPX2(1), S3C_GPIO_SFN(0));
	s3c_gpio_setpull(EXYNOS4_GPX2(1), S3C_GPIO_PULL_NONE);
}

//[*]TODO : ---------------------------------------------------------[*]

static int	insignal_keypad_open(struct input_dev *dev)
{
	return	0;
}

static void	insignal_keypad_close(struct input_dev *dev)
{
	return;
}

static void	insignal_keypad_release_device(struct device *dev)
{
	return;
}

static int sleepmode = 0;
static int powerkey = 0;
static int	insignal_keypad_resume(struct device *dev)
{
	sleepmode = 0;
	if(powerkey)
	{
		input_report_key(insignal_keypad, KEY_POWER, 0);
		powerkey = 0;
	}
	return 0;
}

static int	insignal_keypad_suspend(struct device *dev, pm_message_t state)
{
	sleepmode = 1;
	return 0;
}

static irqreturn_t insignal_keypad_irq(int irq, void *dev_id)
{
	static unsigned int prev_keycode = 0;
	static unsigned int prev_pressed = 0;
	unsigned int keycode = 0;
	unsigned int pressed = 0;

	switch(irq)
	{
	case IRQ_KEY_MENU:
		keycode = KEY_MENU;
		pressed = ((readl(EXYNOS4_GPX1DAT) & (0x1<<5)) == 0) ? 1 : 0;
		break;

	case IRQ_KEY_HOME:
		keycode = KEY_HOME;
		pressed = ((readl(EXYNOS4_GPX1DAT) & (0x1<<6)) == 0) ? 1 : 0;
		break;
	case IRQ_KEY_BACK:
		keycode = KEY_BACK;
		pressed = ((readl(EXYNOS4_GPX1DAT) & (0x1<<7)) == 0) ? 1 : 0;
		break;
	case IRQ_KEY_VOLUMEUP:
		keycode = KEY_VOLUMEUP;
		pressed = ((readl(EXYNOS4_GPX2DAT) & (0x1<<0)) == 0) ? 1 : 0;
		break;
	case IRQ_KEY_VOLUMEDOWN:
		keycode = KEY_VOLUMEDOWN;
		pressed = ((readl(EXYNOS4_GPX2DAT) & (0x1<<1)) == 0) ? 1 : 0;
		break;
	default:
		return IRQ_HANDLED;
	}
	if( sleepmode )
	{
		keycode = KEY_POWER;
		powerkey = pressed;
	}
	if( prev_keycode != keycode || prev_pressed != pressed )
	{
		prev_keycode = keycode;
		prev_pressed = pressed;
		input_event(insignal_keypad, EV_MSC, MSC_SCAN, irq);
		input_report_key(insignal_keypad, keycode, pressed);
		input_sync(insignal_keypad);
#ifdef USE_DEBUG
		switch(keycode)
		{
		case KEY_MENU:
			printk("[MENU] keycode=%d, pressed=%d\n", keycode, pressed);
			break;
		case KEY_HOME:
			printk("[HOME] keycode=%d, pressed=%d\n", keycode, pressed);
			break;
		case KEY_BACK:
			printk("[BACK] keycode=%d, pressed=%d\n", keycode, pressed);
			break;
		case KEY_VOLUMEUP:
			printk("[VOLUMEUP] keycode=%d, pressed=%d\n", keycode, pressed);
			break;
		case KEY_VOLUMEDOWN:
			printk("[VOLUMEDOWN] keycode=%d, pressed=%d\n", keycode, pressed);
			break;
		case KEY_POWER:
			printk("[POWER] keycode=%d, pressed=%d\n", keycode, pressed);
			break;
		default:
			printk("[UNKNOWN] keycode=%d, pressed=%d\n", keycode, pressed);
			break;
		}
#endif
	}
	return IRQ_HANDLED;
}

//[*]TODO : ----------------------------------------------------------------[*]

struct platform_device insignal_platform_device_driver = {
	.name		= DEVICE_NAME,
	.id		= 0,
	.num_resources	= 0,
	.dev	= {
		.release	= insignal_keypad_release_device,
	},
};

struct device_driver insignal_device_driver = {
	.owner		= THIS_MODULE,
	.name		= DEVICE_NAME,
	.bus		= &platform_bus_type,
	.probe		= insignal_keypad_probe,
	.remove		= __devexit_p(insignal_keypad_remove),
	.suspend	= insignal_keypad_suspend,
	.resume		= insignal_keypad_resume,
};

static int __devinit	insignal_keypad_probe(struct device *pdev)
{
	int	key, code;

	insignal_keypad = input_allocate_device();

	if(!(insignal_keypad)) {
		dev_err(pdev, "Can't allocation Memory.\n");
		return -ENOMEM;
	}

	/* GPIO Initialize */
	insignal_keypad_gpio_cfg();

	/* Set event key bits */
	set_bit(EV_KEY, insignal_keypad->evbit);

	/* Allocation key event bits */

	for(key = 0; key < MAX_KEYPAD_CNT; key++){
		code = insignal_keypad_keycode_map[key];
		if(code<=0)
			continue;
		set_bit(code & KEY_MAX, insignal_keypad->keybit);
	}

	/* Allocation Platform Device Structure */
	
	insignal_keypad->name	= DEVICE_NAME;
	insignal_keypad->phys	= "insignal-keypad/input0";
	insignal_keypad->open	= insignal_keypad_open;
	insignal_keypad->close	= insignal_keypad_close;
	
	insignal_keypad->id.bustype = BUS_HOST;
	insignal_keypad->id.vendor = 0x0001;
	insignal_keypad->id.product = 0x0001;
	insignal_keypad->id.version = 0x0001;

	if(input_register_device(insignal_keypad))	{
		dev_err(pdev, "insignal keypad driver register failed\n");
		input_free_device(insignal_keypad);		
		return	-ENODEV;
	}
	
	if(request_irq(IRQ_KEY_MENU, insignal_keypad_irq, IRQF_TRIGGER_FALLING\
			| IRQF_TRIGGER_RISING|IRQF_DISABLED, "KEY_MENU", NULL))
		dev_err(pdev, "Unable to request IRQ_KEY_MENU.\n");
	else
		irq_set_irq_wake(IRQ_KEY_MENU, 1);

	if(request_irq(IRQ_KEY_HOME, insignal_keypad_irq, IRQF_TRIGGER_FALLING\
		| IRQF_TRIGGER_RISING | IRQF_DISABLED, "KEY_HOME", NULL))
		dev_err(pdev, "Unable to request IRQ_KEY_HOME.\n");
	else
		irq_set_irq_wake(IRQ_KEY_HOME, 1);
	if(request_irq(IRQ_KEY_BACK, insignal_keypad_irq, IRQF_TRIGGER_FALLING\
		| IRQF_TRIGGER_RISING | IRQF_DISABLED, "KEY_BACK", NULL))
		dev_err(pdev, "Unable to request IRQ_KEY_BACK.\n");
	else
		irq_set_irq_wake(IRQ_KEY_BACK, 1);
	if(request_irq(IRQ_KEY_VOLUMEUP, insignal_keypad_irq,\
		IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING\
		| IRQF_DISABLED, "KEY_VOLUMEUP", NULL))
		dev_err(pdev, "Unable to request IRQ_KEY_VOLUMEUP.\n");
	else
		irq_set_irq_wake(IRQ_KEY_VOLUMEUP, 1);
	if(request_irq(IRQ_KEY_VOLUMEDOWN, insignal_keypad_irq,\
		IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING\
		| IRQF_DISABLED, "KEY_VOLUMEDOWN", NULL))
		dev_err(pdev, "Unable to request IRQ_KEY_VOLUMEDOWN.\n");
	else
		irq_set_irq_wake(IRQ_KEY_VOLUMEDOWN, 1);

	dev_info(pdev, "insignal keypad input driver Initialized!!\n");
	return 0;
}

static int __devexit	insignal_keypad_remove(struct device *pdev)
{
	disable_irq(IRQ_KEY_MENU);
	disable_irq(IRQ_KEY_HOME);
	disable_irq(IRQ_KEY_BACK);
	disable_irq(IRQ_KEY_VOLUMEUP);
	disable_irq(IRQ_KEY_VOLUMEDOWN);
	input_unregister_device(insignal_keypad);
	return 0;
}

static int __init insignal_keypad_init(void)
{
	int ret = driver_register(&insignal_device_driver);
	
	if (!ret) {
		ret = platform_device_register(&insignal_platform_device_driver);
		
		if (ret)
			driver_unregister(&insignal_device_driver);
	}
	return ret;
}

static void __exit insignal_keypad_exit(void)
{
	platform_device_unregister(&insignal_platform_device_driver);
	driver_unregister(&insignal_device_driver);
}

module_init(insignal_keypad_init);
module_exit(insignal_keypad_exit);

MODULE_AUTHOR("Jhoonkim, <jhoon_kim@nate.com>");
MODULE_DESCRIPTION("Keypad interface driver for Origen Board");
MODULE_LICENSE("GPL");
