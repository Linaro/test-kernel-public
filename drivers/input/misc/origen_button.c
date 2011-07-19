/*
 * Origen keypad driver
 *
 * Copyright (C) 2011 Samsung Electronics Co.Ltd
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
*/

#include <linux/input.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/slab.h>
#include <linux/platform_device.h>

#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/irq.h>

#include <plat/map-base.h>
#include <plat/gpio-cfg.h>
#include <plat/button.h>

#include <mach/regs-gpio.h>
#include <mach/regs-irq.h>

unsigned short origen_map[] = {
	KEY_RESERVED,
	KEY_MENU,
	KEY_HOME,
	KEY_BACK,
	KEY_UP,
	KEY_DOWN
};

struct button_dev {
	unsigned short keymap[ARRAY_SIZE(origen_map)];
	struct timer_list timer[ARRAY_SIZE(origen_map)];
	atomic_t timeout_flag[ARRAY_SIZE(origen_map)];
	struct input_dev *input_dev;
};

struct button_dev *bdev;

void timer_isr(unsigned long key_id)
{
	struct button_dev *button_dev = bdev;
	if (button_dev)
		atomic_dec(&button_dev->timeout_flag[key_id]);

	del_timer(&button_dev->timer[key_id]);
}

irqreturn_t origen_irq_handler(int irq, void *_dev)
{
	uint val;
	struct button_dev *button_dev = _dev;
	struct input_dev *input = button_dev->input_dev;

	val = irq - IRQ_EINT(13);
	if ((val < 0) || (val > 5))
		return -EINVAL;

	val += 1;
	if (atomic_read(&button_dev->timeout_flag[val]) == 0) {
		input_event(input, EV_MSC, MSC_SCAN, val);
		input_report_key(input, button_dev->keymap[val], 1);
		input_report_key(input, button_dev->keymap[val], 0);
		input_sync(input);
		button_dev->timer[val].expires\
			= msecs_to_jiffies(300) + jiffies;
		atomic_inc(&button_dev->timeout_flag[val]);
		add_timer(&button_dev->timer[val]);
	}

return IRQ_HANDLED;
}

struct irqaction origen_button_irq = {
	.name = "origen button",
	.flags = IRQF_SHARED,
	.handler = origen_irq_handler,
};

static int __devinit origen_buttons_probe(struct platform_device *pdev)
{
	u32 err, i;
	struct button_dev *button_dev;
	struct input_dev *input;
	struct resource *res;
	struct origen_button_data *pdata;

	button_dev = kzalloc(sizeof(struct button_dev), GFP_KERNEL);
	if (!button_dev) {
		err = -ENOMEM;
		printk(KERN_ERR"\n Origen keypad: Failed during allocation\n");
		goto error_nomem1;
	}

	memcpy(button_dev->keymap, origen_map, sizeof(button_dev->keymap));
	input = input_allocate_device();
	if (!input) {
		err = -ENOMEM;
		printk(KERN_ERR"\n Origen keypad: Failed during allocation\n");
		goto error_nomem2;
	}

	button_dev->input_dev = input;
	bdev = button_dev;
	input->name = "Origen button";
	input->phys = "origen/input0";
	input->id.bustype = BUS_HOST;
	input->dev.parent = &pdev->dev;
	input->keycode = button_dev->keymap;
	input->keycodemax = ARRAY_SIZE(bdev->keymap);
	input->keycodesize = sizeof(unsigned short);

	input_set_capability(input, EV_MSC, MSC_SCAN);
	__set_bit(EV_KEY, input->evbit);
	for (i  = 0; i < ARRAY_SIZE(origen_map); i++) {
		__set_bit(button_dev->keymap[i], input->keybit);
		init_timer(&button_dev->timer[i]);
		button_dev->timer[i].data = i;
		button_dev->timer[i].function = timer_isr;
		atomic_set(&button_dev->timeout_flag[i], 0);
	}

	origen_button_irq.dev_id = bdev;

	err = input_register_device(input);
	if (err) {
		err = -EINVAL;
		printk(KERN_ERR"\n Origen keypad: input registration failed\n");
		goto error_register;
	}

	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!res) {
		err = -EBUSY;
		goto error_register;
	}
	pdata = pdev->dev.platform_data;
	if (!pdata) {
		printk(KERN_ERR"\n could not get origen button platform data\n");
		goto error_irq;
	}
	err = pdata->cfg_gpio(1);
	if (err) {
		printk(KERN_ERR"\n Error configuring gpio\n");
		goto error_gpio;
	}
	for (i = res->start; i <= res->end; i++) {
		err = irq_set_irq_type(i, IRQ_TYPE_EDGE_FALLING);
		if (err) {
			printk(KERN_ERR"\n Origen Keypad: irq_set_irq_type error\n");
			goto error_irq;
		}

		err = setup_irq(i, &origen_button_irq);
		if (err) {
			printk(KERN_ERR"\n Origen Keypad: setup_irq error\n");
			goto error_irq;
		}

#ifdef CONFIG_PM
		err = irq_set_irq_wake(i, 1);
		if (err) {
			printk(KERN_ERR"\n Origen Keypad: irq_set_irq_wake error\n");
			goto error_irq;
		}
#endif

	}

	printk(KERN_DEBUG"\n Origen button probe successfully completed\n");
	return 0;

error_irq:
	for (i = res->start; i <= res->end; i++)
		remove_irq(i, &origen_button_irq);
error_gpio:
	pdata->cfg_gpio(0);
error_register:
	input_free_device(input);
error_nomem2:
	kfree(button_dev);
	bdev = NULL;
error_nomem1:
	return err;
}

static int __devexit origen_buttons_remove(struct platform_device *pdev)
{
	struct origen_button_data *pdata;
	if (!bdev) {
		if (bdev->input_dev)
			input_free_device(bdev->input_dev);
		kfree(bdev);
		bdev = NULL;
	}
	pdata = pdev->dev.platform_data;
	if (!pdata) {
		printk(KERN_ERR"\n could not get origen button platform data during exit\n");
		return -1;
	}
	pdata->cfg_gpio(0);
	return 0;
}

MODULE_DESCRIPTION("Origen button interface driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:Origen buttons");

static struct platform_driver origen_buttons_driver = {
	.probe  = origen_buttons_probe,
	.remove = origen_buttons_remove,
	.driver = {
	.name   = "origen-button",
	.owner  = THIS_MODULE,
	},
};

static int __init origen_buttons_init(void)
{
	return platform_driver_register(&origen_buttons_driver);
}

static void __exit origen_buttons_exit(void)
{
	platform_driver_unregister(&origen_buttons_driver);
}

module_init(origen_buttons_init);
module_exit(origen_buttons_exit);

