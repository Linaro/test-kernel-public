/*
 * OmniVision OV5650 sensor driver
 *
 * Copyright (C) 2011 Texas Instruments Incorporated - http://www.ti.com/
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation version 2.
 *
 * This program is distributed "as is" WITHOUT ANY WARRANTY of any
 * kind, whether express or implied; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/videodev2.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/log2.h>
#include <linux/delay.h>

#include <media/v4l2-subdev.h>
#include <media/v4l2-chip-ident.h>
#include <media/soc_camera.h>

/* OV5650 has only one fixed colorspace per pixelcode */
struct ov5650_datafmt {
	enum v4l2_mbus_pixelcode	code;
	enum v4l2_colorspace		colorspace;
};

static const struct ov5650_datafmt ov5650_fmts[] = {
	{V4L2_MBUS_FMT_SBGGR10_2X8_PADLO_LE, V4L2_COLORSPACE_SRGB},
};

enum ov5650_size {
	OV5650_SIZE_5MP,
	OV5650_SIZE_LAST,
};

static const struct v4l2_frmsize_discrete ov5650_frmsizes[OV5650_SIZE_LAST] = {
	{ 2592, 1944 },
};

/* Find a data format by a pixel code in an array */
static int ov5650_find_datafmt(enum v4l2_mbus_pixelcode code)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(ov5650_fmts); i++)
		if (ov5650_fmts[i].code == code)
			break;

	return i;
}

/* Find a frame size in an array */
static int ov5650_find_framesize(u32 width, u32 height)
{
	int i;

	for (i = 0; i < OV5650_SIZE_LAST; i++) {
		if ((ov5650_frmsizes[i].width >= width) &&
		    (ov5650_frmsizes[i].height >= height))
			break;
	}

	return i;
}

struct ov5650 {
	struct v4l2_subdev subdev;
	struct v4l2_subdev_sensor_interface_parms *plat_parms;
	int i_size;
	int i_fmt;
};

static struct ov5650 *to_ov5650(const struct i2c_client *client)
{
	return container_of(i2c_get_clientdata(client), struct ov5650, subdev);
}

/**
 * struct ov5650_reg - ov5650 register format
 * @reg: 16-bit offset to register
 * @val: 8/16/32-bit register value
 * @length: length of the register
 *
 * Define a structure for OV5650 register initialization values
 */
struct ov5650_reg {
	u16	reg;
	u8	val;
};

/* TODO: Divide this properly */
static const struct ov5650_reg configscript_5MP[] = {
	{ 0x3008, 0x82 },
	{ 0x3008, 0x42 },
	{ 0x3103, 0x93 },
	{ 0x3b07, 0x0c },
	{ 0x3017, 0xff },
	{ 0x3018, 0xfc },
	{ 0x3706, 0x41 },
	{ 0x3703, 0xe6 },
	{ 0x3613, 0x44 },
	{ 0x3630, 0x22 },
	{ 0x3605, 0x04 },
	{ 0x3606, 0x3f },
	{ 0x3712, 0x13 },
	{ 0x370e, 0x00 },
	{ 0x370b, 0x40 },
	{ 0x3600, 0x54 },
	{ 0x3601, 0x05 },
	{ 0x3713, 0x22 },
	{ 0x3714, 0x27 },
	{ 0x3631, 0x22 },
	{ 0x3612, 0x1a },
	{ 0x3604, 0x40 },
	{ 0x3705, 0xda },
	{ 0x370a, 0x80 },
	{ 0x370c, 0x00 },
	{ 0x3710, 0x28 },
	{ 0x3702, 0x3a },
	{ 0x3704, 0x18 },
	{ 0x3a18, 0x00 },
	{ 0x3a19, 0xf8 },
	{ 0x3a00, 0x38 },
	{ 0x3800, 0x02 },
	{ 0x3801, 0x54 },
	{ 0x3803, 0x0c },
	{ 0x3808, 0x0a },
	{ 0x3809, 0x20 },
	{ 0x380a, 0x07 },
	{ 0x380b, 0x98 },
	{ 0x380c, 0x0c },
	{ 0x380d, 0xb4 },
	{ 0x380e, 0x07 },
	{ 0x380f, 0xb0 },
	{ 0x3830, 0x50 },
	{ 0x3a08, 0x12 },
	{ 0x3a09, 0x70 },
	{ 0x3a0a, 0x0f },
	{ 0x3a0b, 0x60 },
	{ 0x3a0d, 0x06 },
	{ 0x3a0e, 0x06 },
	{ 0x3a13, 0x54 },
	{ 0x3815, 0x82 },
	{ 0x5059, 0x80 },
	{ 0x505a, 0x0a },
	{ 0x505b, 0x2e },
	{ 0x3a1a, 0x06 },
	{ 0x3503, 0x00 },
	{ 0x3623, 0x01 },
	{ 0x3633, 0x24 },
	{ 0x3c01, 0x34 },
	{ 0x3c04, 0x28 },
	{ 0x3c05, 0x98 },
	{ 0x3c07, 0x07 },
	{ 0x3c09, 0xc2 },
	{ 0x4000, 0x05 },
	{ 0x401d, 0x28 },
	{ 0x4001, 0x02 },
	{ 0x401c, 0x46 },
	{ 0x5046, 0x01 },
	{ 0x3810, 0x40 },
	{ 0x3836, 0x41 },
	{ 0x505f, 0x04 },
	{ 0x5000, 0x00 },
	{ 0x5001, 0x00 },
	{ 0x5002, 0x00 },
	{ 0x503d, 0x00 },
	{ 0x5901, 0x00 },
	{ 0x585a, 0x01 },
	{ 0x585b, 0x2c },
	{ 0x585c, 0x01 },
	{ 0x585d, 0x93 },
	{ 0x585e, 0x01 },
	{ 0x585f, 0x90 },
	{ 0x5860, 0x01 },
	{ 0x5861, 0x0d },
	{ 0x5180, 0xc0 },
	{ 0x5184, 0x00 },
	{ 0x470a, 0x00 },
	{ 0x470b, 0x00 },
	{ 0x470c, 0x00 },
	{ 0x300f, 0x8e },
	{ 0x3603, 0xa7 },
	{ 0x3615, 0x50 },
	{ 0x3632, 0x55 },
	{ 0x3620, 0x56 },
	{ 0x3621, 0x2f },
	{ 0x381a, 0x3c },
	{ 0x3818, 0xc0 },
	{ 0x3631, 0x36 },
	{ 0x3632, 0x5f },
	{ 0x3711, 0x24 },
	{ 0x401f, 0x03 },
	{ 0x3011, 0x14 },
	{ 0x3007, 0x3b },
	{ 0x4801, 0x0f },
	{ 0x3003, 0x03 },
	{ 0x300e, 0x0c },
	{ 0x4803, 0x50 },
	{ 0x4800, 0x04 },
	{ 0x300f, 0x8f },
	{ 0x3815, 0x82 },
	{ 0x3003, 0x01 },
	{ 0x3008, 0x02 },
};

static struct v4l2_subdev_sensor_serial_parms mipi_cfgs[OV5650_SIZE_LAST] = {
	[OV5650_SIZE_5MP] = {
		.lanes = 2,
		.channel = 0,
		.phy_rate = (480 * 2 * 1000000),
		.pix_clk = 21, /* Revisit */
	},
};

/**
 * ov5650_reg_read - Read a value from a register in an ov5650 sensor device
 * @client: i2c driver client structure
 * @reg: register address / offset
 * @val: stores the value that gets read
 *
 * Read a value from a register in an ov5650 sensor device.
 * The value is returned in 'val'.
 * Returns zero if successful, or non-zero otherwise.
 */
static int ov5650_reg_read(struct i2c_client *client, u16 reg, u8 *val)
{
	int ret;
	u8 data[2] = {0};
	struct i2c_msg msg = {
		.addr	= client->addr,
		.flags	= 0,
		.len	= 2,
		.buf	= data,
	};

	data[0] = (u8)(reg >> 8);
	data[1] = (u8)(reg & 0xff);

	ret = i2c_transfer(client->adapter, &msg, 1);
	if (ret < 0)
		goto err;

	msg.flags = I2C_M_RD;
	msg.len = 1;
	ret = i2c_transfer(client->adapter, &msg, 1);
	if (ret < 0)
		goto err;

	*val = data[0];
	return 0;

err:
	dev_err(&client->dev, "Failed reading register 0x%02x!\n", reg);
	return ret;
}

/**
 * Write a value to a register in ov5650 sensor device.
 * @client: i2c driver client structure.
 * @reg: Address of the register to read value from.
 * @val: Value to be written to a specific register.
 * Returns zero if successful, or non-zero otherwise.
 */
static int ov5650_reg_write(struct i2c_client *client, u16 reg, u8 val)
{
	int ret;
	unsigned char data[3] = { (u8)(reg >> 8), (u8)(reg & 0xff), val };
	struct i2c_msg msg = {
		.addr	= client->addr,
		.flags	= 0,
		.len	= 3,
		.buf	= data,
	};

	ret = i2c_transfer(client->adapter, &msg, 1);
	if (ret < 0) {
		dev_err(&client->dev, "Failed writing register 0x%02x!\n", reg);
		return ret;
	}

	return 0;
}

/**
 * Initialize a list of ov5650 registers.
 * The list of registers is terminated by the pair of values
 * @client: i2c driver client structure.
 * @reglist[]: List of address of the registers to write data.
 * Returns zero if successful, or non-zero otherwise.
 */
static int ov5650_reg_writes(struct i2c_client *client,
			     const struct ov5650_reg reglist[],
			     int size)
{
	int err = 0, i;

	for (i = 0; i < size; i++) {
		err = ov5650_reg_write(client, reglist[i].reg,
				reglist[i].val);
		if (err)
			return err;
	}
	return 0;
}

static int ov5650_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret = 0;

	if (enable) {
		/* HACK: Hardcoding to 5MP! */
		ret = ov5650_reg_writes(client, configscript_5MP,
				ARRAY_SIZE(configscript_5MP));
		if (ret)
			goto out;
	}

out:
	return ret;
}

static int ov5650_set_bus_param(struct soc_camera_device *icd,
				 unsigned long flags)
{
	/* TODO: Do the right thing here, and validate bus params */
	return 0;
}

static unsigned long ov5650_query_bus_param(struct soc_camera_device *icd)
{
	unsigned long flags = SOCAM_PCLK_SAMPLE_FALLING |
		SOCAM_HSYNC_ACTIVE_HIGH | SOCAM_VSYNC_ACTIVE_HIGH |
		SOCAM_DATA_ACTIVE_HIGH | SOCAM_MASTER;

	/* TODO: Do the right thing here, and validate bus params */

	flags |= SOCAM_DATAWIDTH_10;

	return flags;
}

static int ov5650_g_fmt(struct v4l2_subdev *sd,
			 struct v4l2_mbus_framefmt *mf)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct ov5650 *ov5650 = to_ov5650(client);

	mf->width	= ov5650_frmsizes[ov5650->i_size].width;
	mf->height	= ov5650_frmsizes[ov5650->i_size].height;
	mf->code	= ov5650_fmts[ov5650->i_fmt].code;
	mf->colorspace	= ov5650_fmts[ov5650->i_fmt].colorspace;
	mf->field	= V4L2_FIELD_NONE;

	return 0;
}

static int ov5650_try_fmt(struct v4l2_subdev *sd,
			   struct v4l2_mbus_framefmt *mf)
{
	int i_fmt;
	int i_size;

	i_fmt = ov5650_find_datafmt(mf->code);

	mf->code = ov5650_fmts[i_fmt].code;
	mf->colorspace	= ov5650_fmts[i_fmt].colorspace;
	mf->field	= V4L2_FIELD_NONE;

	i_size = ov5650_find_framesize(mf->width, mf->height);

	mf->width = ov5650_frmsizes[i_size].width;
	mf->height = ov5650_frmsizes[i_size].height;

	return 0;
}

static int ov5650_s_fmt(struct v4l2_subdev *sd,
			 struct v4l2_mbus_framefmt *mf)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct ov5650 *ov5650 = to_ov5650(client);
	int ret;

	ret = ov5650_try_fmt(sd, mf);
	if (ret < 0)
		return ret;

	ov5650->i_size = ov5650_find_framesize(mf->width, mf->height);
	ov5650->i_fmt = ov5650_find_datafmt(mf->code);

	/* TODO: Introduce sensor config here! */

	return 0;
}

static int ov5650_g_chip_ident(struct v4l2_subdev *sd,
				struct v4l2_dbg_chip_ident *id)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	if (id->match.type != V4L2_CHIP_MATCH_I2C_ADDR)
		return -EINVAL;

	if (id->match.addr != client->addr)
		return -ENODEV;

	id->ident	= V4L2_IDENT_OV5650;
	id->revision	= 0;

	return 0;
}

#ifdef CONFIG_VIDEO_ADV_DEBUG
static int ov5650_g_register(struct v4l2_subdev *sd,
			     struct v4l2_dbg_register *reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	if (reg->match.type != V4L2_CHIP_MATCH_I2C_ADDR || reg->size > 2)
		return -EINVAL;

	if (reg->match.addr != client->addr)
		return -ENODEV;

	reg->size = 2;
	if (ov5650_reg_read(client, reg->reg, &reg->val))
		return -EIO

	return 0;
}

static int ov5650_s_register(struct v4l2_subdev *sd,
			     struct v4l2_dbg_register *reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	if (reg->match.type != V4L2_CHIP_MATCH_I2C_ADDR || reg->size > 2)
		return -EINVAL;

	if (reg->match.addr != client->addr)
		return -ENODEV;

	if (ov5650_reg_write(client, reg->reg, reg->val))
		return -EIO;

	return 0;
}
#endif

static struct soc_camera_ops ov5650_ops = {
	.set_bus_param		= ov5650_set_bus_param,
	.query_bus_param	= ov5650_query_bus_param,
};

static int ov5650_init(struct i2c_client *client)
{
	int ret = 0;

	dev_dbg(&client->dev, "Sensor initialized\n");

	return ret;
}

/*
 * Interface active, can use i2c. If it fails, it can indeed mean, that
 * this wasn't our capture interface, so, we wait for the right one
 */
static int ov5650_video_probe(struct soc_camera_device *icd,
			      struct i2c_client *client)
{
	unsigned long flags;
	int ret = 0;
	u8 revision = 0;

	/*
	 * We must have a parent by now. And it cannot be a wrong one.
	 * So this entire test is completely redundant.
	 */
	if (!icd->parent ||
	    to_soc_camera_host(icd->parent)->nr != icd->iface)
		return -ENODEV;

	ret = ov5650_reg_read(client, 0x302A, &revision);
	if (ret) {
		dev_err(&client->dev, "Failure to detect OV5650 chip\n");
		goto out;
	}

	revision &= 0xF;

	flags = SOCAM_DATAWIDTH_8;

	dev_info(&client->dev, "Detected a OV5650 chip, revision %x\n",
		 revision);

	/* TODO: Do something like ov5650_init */

out:
	return ret;
}

static void ov5650_video_remove(struct soc_camera_device *icd)
{
	dev_dbg(icd->pdev, "Video removed: %p, %p\n",
		icd->parent, icd->vdev);
}

static struct v4l2_subdev_core_ops ov5650_subdev_core_ops = {
	.g_chip_ident	= ov5650_g_chip_ident,
#ifdef CONFIG_VIDEO_ADV_DEBUG
	.g_register	= ov5650_g_register,
	.s_register	= ov5650_s_register,
#endif
};

static int ov5650_enum_fmt(struct v4l2_subdev *sd, unsigned int index,
			    enum v4l2_mbus_pixelcode *code)
{
	if (index >= ARRAY_SIZE(ov5650_fmts))
		return -EINVAL;

	*code = ov5650_fmts[index].code;
	return 0;
}

static int ov5650_enum_framesizes(struct v4l2_subdev *sd,
				  struct v4l2_frmsizeenum *fsize)
{
	if (fsize->index > OV5650_SIZE_LAST)
		return -EINVAL;

	fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;
	fsize->pixel_format = V4L2_PIX_FMT_SBGGR10;

	fsize->discrete = ov5650_frmsizes[fsize->index];

	return 0;
}

static int ov5650_g_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *param)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct ov5650 *ov5650 = to_ov5650(client);
	struct v4l2_captureparm *cparm;

	if (param->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		return -EINVAL;

	cparm = &param->parm.capture;

	memset(param, 0, sizeof(*param));
	param->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	cparm->capability = V4L2_CAP_TIMEPERFRAME;

	switch (ov5650->i_size) {
	case OV5650_SIZE_5MP:
	default:
		cparm->timeperframe.numerator = 1;
		cparm->timeperframe.denominator = 5;
		break;
	}

	return 0;
}
static int ov5650_s_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *param)
{
	/*
	 * FIXME: This just enforces the hardcoded framerates until this is
	 * flexible enough.
	 */
	return ov5650_g_parm(sd, param);
}

static struct v4l2_subdev_video_ops ov5650_subdev_video_ops = {
	.s_stream	= ov5650_s_stream,
	.s_mbus_fmt	= ov5650_s_fmt,
	.g_mbus_fmt	= ov5650_g_fmt,
	.try_mbus_fmt	= ov5650_try_fmt,
	.enum_mbus_fmt	= ov5650_enum_fmt,
	.enum_framesizes = ov5650_enum_framesizes,
	.g_parm = ov5650_g_parm,
	.s_parm = ov5650_s_parm,
};

static int ov5650_g_skip_frames(struct v4l2_subdev *sd, u32 *frames)
{
	/* Quantity of initial bad frames to skip. Revisit. */
	*frames = 5;

	return 0;
}

static int ov5650_g_interface_parms(struct v4l2_subdev *sd,
			struct v4l2_subdev_sensor_interface_parms *parms)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct ov5650 *ov5650 = to_ov5650(client);

	if (!parms)
		return -EINVAL;

	parms->if_type = ov5650->plat_parms->if_type;
	parms->if_mode = ov5650->plat_parms->if_mode;
	parms->parms.serial = mipi_cfgs[ov5650->i_size];

	return 0;
}

static struct v4l2_subdev_sensor_ops ov5650_subdev_sensor_ops = {
	.g_skip_frames	= ov5650_g_skip_frames,
	.g_interface_parms = ov5650_g_interface_parms,
};

static struct v4l2_subdev_ops ov5650_subdev_ops = {
	.core	= &ov5650_subdev_core_ops,
	.video	= &ov5650_subdev_video_ops,
	.sensor	= &ov5650_subdev_sensor_ops,
};

static int ov5650_probe(struct i2c_client *client,
			 const struct i2c_device_id *did)
{
	struct ov5650 *ov5650;
	struct soc_camera_device *icd = client->dev.platform_data;
	struct soc_camera_link *icl;
	int ret;

	if (!icd) {
		dev_err(&client->dev, "OV5650: missing soc-camera data!\n");
		return -EINVAL;
	}

	icl = to_soc_camera_link(icd);
	if (!icl) {
		dev_err(&client->dev, "OV5650 driver needs platform data\n");
		return -EINVAL;
	}

	if (!icl->priv) {
		dev_err(&client->dev,
			"OV5650 driver needs i/f platform data\n");
		return -EINVAL;
	}

	ov5650 = kzalloc(sizeof(struct ov5650), GFP_KERNEL);
	if (!ov5650)
		return -ENOMEM;

	v4l2_i2c_subdev_init(&ov5650->subdev, client, &ov5650_subdev_ops);

	/* Second stage probe - when a capture adapter is there */
	icd->ops		= &ov5650_ops;

	ov5650->i_size = OV5650_SIZE_5MP;
	ov5650->i_fmt = 0; /* First format in the list */
	ov5650->plat_parms = icl->priv;

	ret = ov5650_video_probe(icd, client);
	if (ret) {
		icd->ops = NULL;
		kfree(ov5650);
	}

	/* init the sensor here */
	ret = ov5650_init(client);
	if (ret) {
		dev_err(&client->dev, "Failed to initialize sensor\n");
		ret = -EINVAL;
	}

	return ret;
}

static int ov5650_remove(struct i2c_client *client)
{
	struct ov5650 *ov5650 = to_ov5650(client);
	struct soc_camera_device *icd = client->dev.platform_data;

	icd->ops = NULL;
	ov5650_video_remove(icd);
	client->driver = NULL;
	kfree(ov5650);

	return 0;
}

static const struct i2c_device_id ov5650_id[] = {
	{ "ov5650", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, ov5650_id);

static struct i2c_driver ov5650_i2c_driver = {
	.driver = {
		.name = "ov5650",
	},
	.probe		= ov5650_probe,
	.remove		= ov5650_remove,
	.id_table	= ov5650_id,
};

static int __init ov5650_mod_init(void)
{
	return i2c_add_driver(&ov5650_i2c_driver);
}

static void __exit ov5650_mod_exit(void)
{
	i2c_del_driver(&ov5650_i2c_driver);
}

module_init(ov5650_mod_init);
module_exit(ov5650_mod_exit);

MODULE_DESCRIPTION("OmniVision OV5650 Camera driver");
MODULE_AUTHOR("Sergio Aguirre <saaguirre@ti.com>");
MODULE_LICENSE("GPL v2");
