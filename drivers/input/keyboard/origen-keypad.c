/*
 * linux/drivers/input/keyboard/origen-keypad.c
 *
 * Keypad Driver for Origen Board
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
#include <linux/io.h>
#include <linux/delay.h>

#include <asm/irq.h>

#include <mach/map.h>
#include <mach/gpio.h>
#include <plat/gpio-cfg.h>
#include <plat/devs.h>
#include <plat/origen-keypad.h>
#include <mach/regs-gpio.h>

#define MAX_KEYPAD_CNT	5

static struct input_dev	*input_dev;
static unsigned int keycode;

static unsigned int origen_keypad_keycode_map[MAX_KEYPAD_CNT] = {
	KEY_MENU,
	KEY_HOME,
	KEY_BACK,
	KEY_VOLUMEUP,
	KEY_VOLUMEDOWN,
};

static unsigned int origen_keypad_irq_map[MAX_KEYPAD_CNT] = {
	IRQ_KEY_MENU,
	IRQ_KEY_HOME,
	IRQ_KEY_BACK,
	IRQ_KEY_VOLUMEUP,
	IRQ_KEY_VOLUMEDOWN,
};

static const char origen_keypad_keyname[MAX_KEYPAD_CNT][15] = {
	"KEY_MENU",
	"KEY_HOME",
	"KEY_BACK",
	"KEY_VOLUMEUP",
	"KEY_VOLUMEDOWN",
};

static irqreturn_t origen_keypad_irq(int irq, void *dev_id)
{
	bool key_down;

	switch (irq) {

	case IRQ_KEY_MENU:
		keycode = KEY_MENU;
		key_down = ((readl(EXYNOS4_GPX1DAT) & (0x1<<5)) == 0) ? 1 : 0;
		break;
	case IRQ_KEY_HOME:
		keycode = KEY_HOME;
		key_down = ((readl(EXYNOS4_GPX1DAT) & (0x1<<6)) == 0) ? 1 : 0;
		break;
	case IRQ_KEY_BACK:
		keycode = KEY_BACK;
		key_down = ((readl(EXYNOS4_GPX1DAT) & (0x1<<7)) == 0) ? 1 : 0;
		break;
	case IRQ_KEY_VOLUMEUP:
		keycode = KEY_VOLUMEUP;
		key_down = ((readl(EXYNOS4_GPX2DAT) & (0x1<<0)) == 0) ? 1 : 0;
		break;
	case IRQ_KEY_VOLUMEDOWN:
		keycode = KEY_VOLUMEDOWN;
		key_down = ((readl(EXYNOS4_GPX2DAT) & (0x1<<1)) == 0) ? 1 : 0;
		break;
	default:
		printk(KERN_ERR "Unknown interrupt received\n");
		return IRQ_HANDLED;
	}

		input_event(input_dev, EV_MSC, MSC_SCAN, irq);
		input_report_key(input_dev, keycode, key_down);
		input_sync(input_dev);
		printk(KERN_INFO "keycode=%d, pressed=%d\n", keycode, key_down);

	return IRQ_HANDLED;
}

static int __devinit origen_keypad_probe(struct platform_device *pdev)
{
	const struct origen_keypad_platdata *pdata;
	int key, code, error;

	pdata = pdev->dev.platform_data;
	if (!pdata) {
		dev_err(&pdev->dev, "no platform data defined\n");
		return -EINVAL;
	}

	input_dev = input_allocate_device();
	if (!(input_dev)) {
		dev_err(&pdev->dev, "Can't allocate memory.\n");
		return -ENOMEM;
	}

	/* Initialize GPIO */
	if (pdata->cfg_gpio)
		pdata->cfg_gpio();

	/* Set event key bits */
	set_bit(EV_KEY, input_dev->evbit);

	/* Allocate key event bits */
	for (key = 0; key < MAX_KEYPAD_CNT; key++) {
		code = origen_keypad_keycode_map[key];
		set_bit(code & KEY_MAX, input_dev->keybit);
	}

	/* Allocation Platform Device Structure */
	input_dev->name		= pdev->name;
	input_dev->id.bustype	= BUS_HOST;
	input_dev->dev.parent	= &pdev->dev;

	input_set_capability(input_dev, EV_MSC, MSC_SCAN);

	for (key = 0; key < MAX_KEYPAD_CNT; key++) {
		error = request_irq(origen_keypad_irq_map[key],\
				origen_keypad_irq, IRQF_TRIGGER_FALLING |\
				IRQF_TRIGGER_RISING | IRQF_DISABLED,\
				&origen_keypad_keyname[key][0], NULL);
		if (error) {
			dev_err(&pdev->dev,\
				"failed to register keypad interrupt.\n");
			goto err_free_mem;
		} else {
			irq_set_irq_wake(origen_keypad_irq_map[key], 1);
		}
	}

	error = input_register_device(input_dev);
	if (error) {
		dev_err(&pdev->dev, "failed to register keypad driver\n");
		goto err_free_mem;
	}

	return 0;

err_free_mem:
	input_free_device(input_dev);

	return error;
}

static int __devexit origen_keypad_remove(struct platform_device *pdev)
{
	int key;

	for (key = 0; key < MAX_KEYPAD_CNT; key++)
		disable_irq(origen_keypad_irq_map[key]);

	input_unregister_device(input_dev);

	return 0;
}

#ifdef CONFIG_PM
static int origen_keypad_suspend(struct device *dev)
{
	keycode = KEY_MENU;
	return 0;
}

static int origen_keypad_resume(struct device *dev)
{
	input_report_key(input_dev, keycode, 1);
	input_report_key(input_dev, keycode, 0);
	input_sync(input_dev);

	return 0;
}

static const struct dev_pm_ops origen_keypad_pm_ops = {
	.suspend	= origen_keypad_suspend,
	.resume		= origen_keypad_resume,
};
#endif

static struct platform_driver origen_keypad_driver = {
	.probe		= origen_keypad_probe,
	.remove		= __devexit_p(origen_keypad_remove),
	.driver		= {
		.name	= "origen-keypad",
		.owner	= THIS_MODULE,
#ifdef CONFIG_PM
		.pm	= &origen_keypad_pm_ops,
#endif
	},
};

static int __init origen_keypad_init(void)
{
	return platform_driver_register(&origen_keypad_driver);
}
module_init(origen_keypad_init);

static void __exit origen_keypad_exit(void)
{
	platform_driver_unregister(&origen_keypad_driver);
}
module_exit(origen_keypad_exit);

MODULE_DESCRIPTION("Origen keypad driver");
MODULE_AUTHOR("Sachin Kamat <sachin.kamat@samsung.com>");
MODULE_LICENSE("GPL");
