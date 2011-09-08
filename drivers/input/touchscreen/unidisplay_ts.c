#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/pm.h>
#include <linux/i2c.h>
#include <linux/string.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/input.h>
#include <linux/clk.h>
#include <linux/irq.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/delay.h>

#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/wakelock.h>
#include <linux/earlysuspend.h>
#include <linux/suspend.h>
#endif

#include <asm/irq.h>

#include <mach/gpio.h>
#include <mach/map.h>
#include <mach/hardware.h>
#include <mach/irqs.h>
#include <mach/regs-gpio.h>

#include <plat/gpio-cfg.h>
#include <linux/kthread.h>

/* 20 ms */
#define TOUCH_READ_TIME		(HZ/200)

#define TOUCH_INT_PIN		EXYNOS4_GPX3(1)
#define TOUCH_INT_PIN_SHIFT	1
#define TOUCH_RST_PIN		EXYNOS4_GPE3(5)

#define TOUCHSCREEN_MINX	0
#define TOUCHSCREEN_MAXX	3968
#define TOUCHSCREEN_MINY	0
#define TOUCHSCREEN_MAXY	2304


#define	INPUT_REPORT(x, y, p)	\
		{ \
		input_report_abs(tsdata->input, ABS_MT_POSITION_X, x); \
		input_report_abs(tsdata->input, ABS_MT_POSITION_Y, y); \
		input_report_abs(tsdata->input, ABS_MT_TOUCH_MAJOR, p); \
		input_mt_sync(tsdata->input); \
		}

struct unidisplay_ts_data {
	struct i2c_client *client;
	struct input_dev *input;
	struct delayed_work work;
	int irq;
};

wait_queue_head_t idle_wait;
static struct task_struct *kidle_task;

static irqreturn_t unidisplay_ts_isr(int irq, void *dev_id);

static void unidisplay_ts_config(void)
{
	s3c_gpio_cfgpin(TOUCH_INT_PIN, S3C_GPIO_SFN(0x0F));
	s3c_gpio_setpull(TOUCH_INT_PIN, S3C_GPIO_PULL_UP);

	if (gpio_request(TOUCH_INT_PIN, "TOUCH_INT_PIN")) {
		printk(KERN_ERR "%s : gpio request failed.\n", __func__);
		return;
	}
	gpio_direction_input(TOUCH_INT_PIN);
	gpio_free(TOUCH_INT_PIN);

	s3c_gpio_setpull(TOUCH_RST_PIN, S3C_GPIO_PULL_NONE);
	s3c_gpio_cfgpin(TOUCH_RST_PIN, S3C_GPIO_OUTPUT);

	if (gpio_request(TOUCH_RST_PIN, "TOUCH_RST_PIN")) {
		printk(KERN_ERR "%s : gpio request failed.\n", __func__);
		return;
	}
	gpio_direction_output(TOUCH_RST_PIN, 1);
	gpio_free(TOUCH_RST_PIN);
}

static void unidisplay_ts_start(void)
{
	if (gpio_request(TOUCH_RST_PIN, "TOUCH_RST_PIN")) {
		printk(KERN_ERR "%s : gpio request failed.\n", __func__);
		return;
	}
	gpio_set_value(TOUCH_RST_PIN, 0);
	gpio_free(TOUCH_RST_PIN);
}

static void unidisplay_ts_stop(void)
{
	if (gpio_request(TOUCH_RST_PIN, "TOUCH_RST_PIN")) {
		printk(KERN_ERR "%s : gpio request failed.\n", __func__);
		return;
	}
	gpio_set_value(TOUCH_RST_PIN, 1);
	gpio_free(TOUCH_RST_PIN);
}

static void unidisplay_ts_reset(void)
{
	unidisplay_ts_stop();
	udelay(100);
	unidisplay_ts_start();
}

static int unidisplay_ts_read_irq(void)
{
	return (readl(S5P_VA_GPIO2 + 0xC64) >> (TOUCH_INT_PIN_SHIFT)) & 0x1;
}

static irqreturn_t unidisplay_ts_isr(int irq, void *dev_id)
{
	if (!unidisplay_ts_read_irq())
		wake_up_interruptible(&(idle_wait));

	return IRQ_HANDLED;
}

int unidisplay_ts_thread(void *kthread)
{
	struct unidisplay_ts_data *tsdata = 
		(struct unidisplay_ts_data *)kthread;
	u8 buf[9];
	int x1 = 0, x2 = 0, y1 = 0, y2 = 0;
	int ret;
	u8 type = 0;
	int pendown;

	do {

		interruptible_sleep_on(&idle_wait);
		disable_irq(tsdata->irq);
		while (!kthread_should_stop()) {

			pendown = !unidisplay_ts_read_irq();
			if (pendown) {

				u8 addr = 0x10;
				memset(buf, 0, sizeof(buf));
				ret = i2c_master_send(tsdata->client, &addr, 1);
				if (ret != 1) {
					dev_err(&tsdata->client->dev,\
					"Unable to write to i2c touchscreen\n");
					enable_irq(tsdata->irq);
					continue;
				}
				ret = i2c_master_recv(tsdata->client, buf, 9);
				if (ret != 9) {
					dev_err(&tsdata->client->dev,\
					"Unable to read to i2c touchscreen!\n");
					enable_irq(tsdata->irq);
					continue;
				}

				type = buf[0];

				if (type & 0x1) {
					x1 = buf[2];
					x1 <<= 8;
					x1 |= buf[1];
					y1 = buf[4];
					y1 <<= 8;
					y1 |= buf[3];
					INPUT_REPORT(x1, y1, 1);
				}
				if (type & 0x2) {
					x2 = buf[6];
					x2 <<= 8;
					x2 |= buf[5];
					y2 = buf[8];
					y2 <<= 8;
					y2 |= buf[7];
					INPUT_REPORT(x2, y2, 2);
				}

				input_sync(tsdata->input);
				interruptible_sleep_on_timeout(&idle_wait,\
							TOUCH_READ_TIME);
			} else {
				if (type & 1) {
					INPUT_REPORT(x1, y1, 0);
					x1 = 0;
					y1 = 0;
				}
				if (type & 2) {
					INPUT_REPORT(x2, y2, 0);
					x2 = 0;
					y2 = 0;
				}
				input_sync(tsdata->input);

				INPUT_REPORT(0, 0, 0);
				INPUT_REPORT(0, 0, 0);
				input_sync(tsdata->input);
				break;
			}
		}
		enable_irq(tsdata->irq);
	} while (!kthread_should_stop());

	return 0;
}

static int unidisplay_ts_open(struct input_dev *dev)
{
	return 0;
}

static void unidisplay_ts_close(struct input_dev *dev)
{
}

static int unidisplay_ts_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	struct unidisplay_ts_data *tsdata;
	int error;

	unidisplay_ts_config();
	unidisplay_ts_reset();

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		printk(KERN_ERR "unidisplay_ts_probe: i2c function check failed.\n");
		return -ENODEV;
	}

	tsdata = kzalloc(sizeof(*tsdata), GFP_KERNEL);
	if (!tsdata) {
		dev_err(&client->dev, "failed to allocate driver data!\n");
		error = -ENOMEM;
		dev_set_drvdata(&client->dev, NULL);
		return error;
	}

	dev_set_drvdata(&client->dev, tsdata);

	tsdata->input = input_allocate_device();
	if (!tsdata->input) {
		dev_err(&client->dev, "failed to allocate input device!\n");
		input_free_device(tsdata->input);
		kfree(tsdata);
		return -ENOMEM;
	}

	tsdata->input->evbit[0] = BIT_MASK(EV_SYN) | BIT_MASK(EV_KEY) |\
		BIT_MASK(EV_ABS);
	set_bit(EV_SYN, tsdata->input->evbit);
	set_bit(EV_KEY, tsdata->input->evbit);
	set_bit(EV_ABS, tsdata->input->evbit);

	tsdata->input->keybit[BIT_WORD(BTN_TOUCH)] = BIT_MASK(BTN_TOUCH);

	set_bit(0, tsdata->input->absbit);
	set_bit(1, tsdata->input->absbit);
	set_bit(2, tsdata->input->absbit);

	input_set_abs_params(tsdata->input, ABS_X, TOUCHSCREEN_MINX,\
						TOUCHSCREEN_MAXX, 0, 0);
	input_set_abs_params(tsdata->input, ABS_Y, TOUCHSCREEN_MINY,\
						TOUCHSCREEN_MAXY, 0, 0);
	input_set_abs_params(tsdata->input, ABS_HAT0X, TOUCHSCREEN_MINX,\
						TOUCHSCREEN_MAXX, 0, 0);
	input_set_abs_params(tsdata->input, ABS_HAT0Y, TOUCHSCREEN_MINY,\
						TOUCHSCREEN_MAXY, 0, 0);
	input_set_abs_params(tsdata->input, ABS_MT_POSITION_X,\
				TOUCHSCREEN_MINX, TOUCHSCREEN_MAXX, 0, 0);
	input_set_abs_params(tsdata->input, ABS_MT_POSITION_Y, \
				TOUCHSCREEN_MINY, TOUCHSCREEN_MAXY, 0, 0);
	input_set_abs_params(tsdata->input, ABS_MT_TOUCH_MAJOR, 0, 255, 0, 0);
	input_set_abs_params(tsdata->input, ABS_MT_WIDTH_MAJOR, 0, 25, 0, 0);

	tsdata->input->name = client->name;
	tsdata->input->id.bustype = BUS_I2C;
	tsdata->input->dev.parent = &client->dev;

	tsdata->input->open = unidisplay_ts_open;
	tsdata->input->close = unidisplay_ts_close;

	input_set_drvdata(tsdata->input, tsdata);

	tsdata->client = client;
	tsdata->irq = client->irq;

	if (input_register_device(tsdata->input)) {
		input_free_device(tsdata->input);
		kfree(tsdata);
		return -EIO;
	}

	device_init_wakeup(&client->dev, 1);
	init_waitqueue_head(&idle_wait);
		

	kidle_task = kthread_run(unidisplay_ts_thread, tsdata, "kidle_timeout");
	if (IS_ERR(kidle_task)) {
		printk(KERN_ERR "error k thread run\n");
		return -1;
	}

	if (request_irq(tsdata->irq, unidisplay_ts_isr,\
		IRQF_DISABLED | IRQF_TRIGGER_FALLING, client->name, tsdata)) {
		dev_err(&client->dev, "Unable to request touchscreen IRQ.\n");
		input_unregister_device(tsdata->input);
	}

	return 0;
}


static int unidisplay_ts_remove(struct i2c_client *client)
{
	struct unidisplay_ts_data *tsdata = dev_get_drvdata(&client->dev);
	disable_irq(tsdata->irq);
	free_irq(tsdata->irq, tsdata);
	input_unregister_device(tsdata->input);
	kfree(tsdata);
	dev_set_drvdata(&client->dev, NULL);
	return 0;
}

static int unidisplay_ts_suspend(struct i2c_client *client, pm_message_t mesg)
{
	struct unidisplay_ts_data *tsdata = dev_get_drvdata(&client->dev);
	disable_irq(tsdata->irq);
	return 0;
}

static int unidisplay_ts_resume(struct i2c_client *client)
{
	struct unidisplay_ts_data *tsdata = dev_get_drvdata(&client->dev);
	enable_irq(tsdata->irq);
	return 0;
}

static const struct i2c_device_id unidisplay_ts_i2c_id[] = {
	{ "unidisplay_ts", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, unidisplay_ts_i2c_id);

static struct i2c_driver unidisplay_ts_i2c_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "unidisplay touchscreen driver",
	},
	.probe = unidisplay_ts_probe,
	.remove = unidisplay_ts_remove,
	.suspend = unidisplay_ts_suspend,
	.resume = unidisplay_ts_resume,
	.id_table = unidisplay_ts_i2c_id,
};

static int __init unidisplay_ts_init(void)
{
	return i2c_add_driver(&unidisplay_ts_i2c_driver);
}

static void __exit unidisplay_ts_exit(void)
{
	i2c_del_driver(&unidisplay_ts_i2c_driver);
}

module_init(unidisplay_ts_init);
module_exit(unidisplay_ts_exit);

MODULE_AUTHOR("JHKIM");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0");
MODULE_DESCRIPTION("unidisplay Touch-screen Driver");

