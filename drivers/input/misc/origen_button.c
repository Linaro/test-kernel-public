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

#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/irq.h>

#include <plat/map-base.h>
#include <plat/gpio-cfg.h>

#include <mach/regs-gpio.h>
#include <mach/regs-irq.h>

static int s3c_button_gpio_init(void);
static int s3c_button_irq_init(void);
int s3c_button_init(void);
void s3c_button_exit(void);
irqreturn_t origen_irq_handler(int irq, void *_dev);

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
    struct input_dev *input_dev;
};

struct button_dev *bdev = NULL;

struct irqaction origen_button_irq = {
    .name = "origen button",
    .flags = IRQF_SHARED,
    .handler = origen_irq_handler,
};

int s3c_button_init(void)
{
    if (s3c_button_gpio_init()) {
	printk(KERN_ERR "%s failed\n", __func__);
	goto error;
    }

    if(s3c_button_irq_init()) {
	printk(KERN_ERR"Origen Keypad irq register failed\n");
	goto error;
    }

    return 0;
error:
    s3c_button_exit();
    return -1;
}

void s3c_button_exit(void)
{
    int i;

    for (i = 5; i <= 7; i++) {
	remove_irq(gpio_to_irq(EXYNOS4_GPX1(i)), &origen_button_irq);
	gpio_free(EXYNOS4_GPX1(i));
    }

    for (i = 0; i <= 1; i++) {
	remove_irq(gpio_to_irq(EXYNOS4_GPX2(i)), &origen_button_irq);
	gpio_free(EXYNOS4_GPX2(i));
    }

}

static int s3c_button_gpio_init(void)
{
    u32 err;
    u32 i;

    for (i = 5; i <= 7; i++) {
	err = gpio_request(EXYNOS4_GPX1(i), "GPX1");
	if (err) {
	    printk(KERN_INFO "gpio request error : %d\n", err);
	} else {
	    s3c_gpio_cfgpin(EXYNOS4_GPX1(i), (0xf << (i*4)));
	    s3c_gpio_setpull(EXYNOS4_GPX1(i), S3C_GPIO_PULL_NONE);
	}
    }

    for (i = 0; i <= 1; i++) {
	err = gpio_request(EXYNOS4_GPX2(i), "GPX2");
	if (err) {
	    printk(KERN_INFO "gpio request error : %d\n", err);
	} else {
	    s3c_gpio_cfgpin(EXYNOS4_GPX2(i), (0xf << (i*4)));
	    s3c_gpio_setpull(EXYNOS4_GPX2(i), S3C_GPIO_PULL_NONE);
	}
    }

    return err;

}

static int s3c_button_irq_init(void)
{
    int i, err;
    for (i = 5; i <= 7; i++) {
	err = set_irq_type(gpio_to_irq(EXYNOS4_GPX1(i)), IRQ_TYPE_EDGE_FALLING);
	if(err) {
	    printk("\n Origen Keypad: set_irq_type error\n");
	    goto error;
	}

	err = setup_irq(gpio_to_irq(EXYNOS4_GPX1(i)), &origen_button_irq);
	if(err) {
	    printk("\n Origen Keypad: setup_irq error\n");
	    goto error;
	}

#ifdef CONFIG_PM
	err = set_irq_wake(gpio_to_irq(EXYNOS4_GPX1(i)), 1);
	if(err) {
	    printk("\n Origen Keypad: set_irq_wake error\n");
	    goto error;
	}
#endif
    }

    for (i = 0; i <= 1; i++) {
	err = set_irq_type(gpio_to_irq(EXYNOS4_GPX2(i)), IRQ_TYPE_EDGE_FALLING);
	if(err) {
	    printk("\n Origen Keypad: set_irq_type error\n");
	    goto error;
	}

	err = setup_irq(gpio_to_irq(EXYNOS4_GPX2(i)), &origen_button_irq);
	if(err) {
	    printk("\n Origen Keypad: setup_irq error\n");
	    goto error;
	}

#ifdef CONFIG_PM
	err = set_irq_wake(gpio_to_irq(EXYNOS4_GPX2(i)), 1);
	if(err) {
	    printk("\n Origen Keypad: set_irq_wake error\n");
	    goto error;
	}
#endif
    }

    return 0;
error:
    return -1;
}

void timer_isr(unsigned long key_id)
{
    struct button_dev *button_dev = bdev;
    if(button_dev) {
	    __change_bit(button_dev->keymap[key_id], button_dev->input_dev->key);
    }
    del_timer(&button_dev->timer[key_id]);
}

irqreturn_t origen_irq_handler(int irq, void *_dev)
{
    uint val;
    struct button_dev *button_dev = _dev;
    struct input_dev *input = button_dev->input_dev;

    val = irq - IRQ_EINT(13);
    if((val < 0) || (val > 5))
	return -EINVAL;

    val += 1;
    if(!test_bit(button_dev->keymap[val], input->key)) {
	input_event(input, EV_MSC, MSC_SCAN, val);
	input_report_key(input, button_dev->keymap[val], 1);

	input_sync(input);
	button_dev->timer[val].expires = msecs_to_jiffies(150) + jiffies;
	add_timer(&button_dev->timer[val]);
    }

    return IRQ_HANDLED;
}

int __init origen_init(void)
{
    u32 err, i;
    struct button_dev *button_dev;
    struct input_dev *input;

    button_dev = kzalloc(sizeof(struct button_dev), GFP_KERNEL);
    if(!button_dev) {
	err = -ENOMEM;
	printk("\n Origen keypad: Failed during allocation \n");
	goto nomem1;
    }

    memcpy(button_dev->keymap, origen_map, sizeof(button_dev->keymap));
    input = input_allocate_device();
    if(!input) { 
	err = -ENOMEM;
	printk("\n Origen keypad: Failed during allocation \n");
	goto nomem2;
    }

    button_dev->input_dev = input;
    bdev = button_dev;
    input->name = "Origen button";
    input->phys = "origen/input0";
    input->id.bustype = BUS_HOST;
    input->keycode = button_dev->keymap;
    input->keycodemax = ARRAY_SIZE(bdev->keymap);
    input->keycodesize = sizeof(unsigned short);

    input_set_capability(input, EV_MSC, MSC_SCAN);
    __set_bit(EV_KEY, input->evbit);
    for(i  =0; i < ARRAY_SIZE(origen_map); i++) {
	__set_bit(button_dev->keymap[i], input->keybit);
	init_timer(&button_dev->timer[i]);
	button_dev->timer[i].data = i;
	button_dev->timer[i].function = timer_isr;
    }

    origen_button_irq.dev_id = bdev;

    err = input_register_device(input);
    if(err) {
	err = -EINVAL;
	printk("\n Origen keypad: input registration failed\n");
	goto register_err;
    }

    if(s3c_button_init()) {
	printk("\n Origen keypad: s3c_button_init failed\n");
	err = -1;
	goto register_err;
    }
    return 0;

register_err:
    input_free_device(input);
nomem2:
    kfree(button_dev);
    bdev = NULL;
nomem1:
    return err;
}

void __exit origen_exit(void)
{
    if(!bdev) {
	if(bdev->input_dev)
	    input_free_device(bdev->input_dev);
	kfree(bdev);
	bdev = NULL;
    }

    s3c_button_exit();

}

module_init(origen_init);
module_exit(origen_exit);
