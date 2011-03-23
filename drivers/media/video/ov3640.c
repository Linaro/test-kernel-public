/*
 * OmniVision OV3640 sensor driver
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

#include <linux/i2c.h>
#include <linux/delay.h>

#include <media/v4l2-subdev.h>
#include <media/v4l2-chip-ident.h>
#include <media/soc_camera.h>

#define OV3640_DRIVER_NAME		 "ov3640"

#define OV3640_CSI2_VIRTUAL_ID		0x1

/* FPS Capabilities */
#define OV3640_MIN_FPS			5
#define OV3640_DEF_FPS			15
#define OV3640_MAX_FPS			30

#define OV3640_MIN_BRIGHT		0
#define OV3640_MAX_BRIGHT		6
#define OV3640_DEF_BRIGHT		0
#define OV3640_BRIGHT_STEP		1

#define OV3640_DEF_CONTRAST		0
#define OV3640_MIN_CONTRAST		0
#define OV3640_MAX_CONTRAST		6
#define OV3640_CONTRAST_STEP		1

/* define a structure for ov3640 register initialization values */
struct ov3640_reg {
	unsigned int reg;
	unsigned char val;
};

enum ov3640_image_size {
	XGA,
	QXGA
};
enum ov3640_pixel_format {
	YUV,
	RGB565,
	RGB555,
	RAW10
};

#define OV3640_NUM_IMAGE_SIZES		2
#define OV3640_NUM_PIXEL_FORMATS	4
#define OV3640_NUM_FPS			3

struct ov3640_capture_size {
	unsigned long width;
	unsigned long height;
};

const struct v4l2_fract ov3640_frameintervals[] = {
	{ .numerator = 2, .denominator = 15 },
	{ .numerator = 1, .denominator = 15 },
	{ .numerator = 1, .denominator = 30 },
};

static const struct ov3640_reg ov3640_common[2][100] = {
	/* XGA_Default settings */
	{
		{0x3002, 0x03},
		{0x3003, 0x0F},
		{0x3001, 0x07},
		{0x304d, 0x45},
		{0x30aa, 0x45},
		{0x30B1, 0xff},
		{0x30B2, 0x10},
		{0x3018, 0x38},
		{0x3019, 0x30},
		{0x301A, 0x61},
		{0x3082, 0x20},
		{0x3015, 0x14},
#if defined(CONFIG_SOC_CAMERA_OV3640_ISP)
		{0x3013, 0xFD},
#else
		{0x3013, 0xF8},
#endif
		{0x303C, 0x08},
		{0x303D, 0x18},
		{0x303E, 0x06},
		{0x303F, 0x0c},
		{0x3030, 0x62},
		{0x3031, 0x26},
		{0x3032, 0xe6},
		{0x3033, 0x6e},
		{0x3034, 0xea},
		{0x3035, 0xae},
		{0x3036, 0xa6},
		{0x3037, 0x6a},
		{0x3104, 0x02},
		{0x3105, 0xfd},
		{0x3106, 0x00},
		{0x3107, 0xff},
		{0x3300, 0x13},
		{0x3301, 0xde},
		{0x3302, 0xef},
		{0x3316, 0xff},
		{0x3317, 0x00},
		{0x3312, 0x26},
		{0x3314, 0x42},
		{0x3313, 0x2b},
		{0x3315, 0x42},
		{0x3310, 0xd0},
		{0x3311, 0xbd},
		{0x330c, 0x18},
		{0x330d, 0x18},
		{0x330e, 0x56},
		{0x330f, 0x5c},
		{0x330b, 0x1c},
		{0x3306, 0x5c},
		{0x3307, 0x11},
		{0x336A, 0x52},
		{0x3370, 0x46},
		{0x3376, 0x38},
		{0x30B8, 0x20},
		{0x30B9, 0x17},
		{0x30BA, 0x04},
		{0x30BB, 0x08},
		{0x3507, 0x06},
		{0x350a, 0x4f},
		{0x3100, 0x02},
		{0x3301, 0xde},
		{0x3304, 0xfc},
		{0x3012, 0x10},
		{0x3023, 0x07},
		{0x3026, 0x03},
		{0x3027, 0x04},
		{0x3075, 0x24},
		{0x300D, 0x01},
		{0x30d7, 0x90},
		{0x335F, 0x34},
		{0x3360, 0x0c},
		{0x3361, 0x04},
		{0x3362, 0x34},
		{0x3363, 0x08},
		{0x3364, 0x04},
		{0x3403, 0x42},
		{0x3088, 0x04},
		{0x3089, 0x00},
		{0x308A, 0x03},
		{0x308B, 0x00},
		{0x308D, 0x04},
		{0x3600, 0xc4},
		{0x332B, 0x00},
		{0x332D, 0x60},
		{0x332F, 0x03},
	},
	/* QXGA Default settings */
	{
		{0x3002, 0x06},
		{0x3003, 0x1F},
		{0x3001, 0x12},
		{0x304d, 0x45},
		{0x30aa, 0x45},
		{0x30B0, 0xff},
		{0x30B1, 0xff},
		{0x30B2, 0x10},
		{0x30d7, 0x10},
		{0x3047, 0x00},
		{0x3018, 0x60},
		{0x3019, 0x58},
		{0x301A, 0xa1},
		{0x3087, 0x02},
		{0x3082, 0x20},
		{0x303C, 0x08},
		{0x303D, 0x18},
		{0x303E, 0x06},
		{0x303F, 0x0c},
		{0x3030, 0x62},
		{0x3031, 0x26},
		{0x3032, 0xe6},
		{0x3033, 0x6e},
		{0x3034, 0xea},
		{0x3035, 0xae},
		{0x3036, 0xa6},
		{0x3037, 0x6a},
		{0x3015, 0x12},
#if defined(CONFIG_SOC_CAMERA_OV3640_ISP)
		{0x3013, 0xFD},
#else
		{0x3013, 0xF8},
#endif
		{0x3104, 0x02},
		{0x3105, 0xfd},
		{0x3106, 0x00},
		{0x3107, 0xff},
		{0x3308, 0xa5},
		{0x3316, 0xff},
		{0x3317, 0x00},
		{0x3087, 0x02},
		{0x3082, 0x20},
		{0x3300, 0x13},
		{0x3301, 0xd6},
		{0x3302, 0xef},
		{0x30B8, 0x20},
		{0x30B9, 0x17},
		{0x30BA, 0x04},
		{0x30BB, 0x08},
		{0x3020, 0x01},
		{0x3021, 0x1d},
		{0x3022, 0x00},
		{0x3023, 0x0b},
		{0x3024, 0x08},
		{0x3025, 0x18},
		{0x3026, 0x06},
		{0x3027, 0x0c},
		{0x335F, 0x68},
		{0x3360, 0x18},
		{0x3361, 0x0c},
		{0x3362, 0x68},
		{0x3363, 0x08},
		{0x3364, 0x04},
		{0x3403, 0x42},
		{0x3088, 0x08},
		{0x3089, 0x00},
		{0x308A, 0x06},
		{0x308B, 0x00},
		{0x3507, 0x06},
		{0x350a, 0x4f},
		{0x3600, 0xc4},
		{0x332B, 0x00},
		{0x332D, 0x45},
		{0x332D, 0x60},
		{0x332F, 0x03},
	},
};

static const struct ov3640_reg ov3640_common_csi2[] = {
	{0x3602, 0x22},
	{0x361E, 0x00},
	{0x3622, 0x18},
	{0x3623, 0x69},
	{0x3626, 0x00},
	{0x3627, 0xF0},
	{0x3628, 0x00},
	{0x3629, 0x26},
	{0x362A, 0x00},
	{0x362B, 0x5F},
	{0x362C, 0xD0},
	{0x362D, 0x3C},
	{0x3632, 0x10},
	{0x3633, 0x28},
	{0x3603, 0x4D},
	{0x364C, 0x04},
	{0x309e, 0x00},
};

/* Array of image sizes supported by OV3640.  These must be ordered from
 * smallest image size to largest.
 */
static const struct ov3640_capture_size ov3640_sizes[] = {
	/* XGA */
	{ 1024, 768 },
	/* QXGA */
	{ 2048, 1536 },
};

/**
 * struct ov3640 - main structure for storage of sensor information
 * @i2c_client: iic client device structure
 * @pix: V4L2 pixel format information structure
 * @timeperframe: time per frame expressed as V4L fraction
 * @isize: base image size
 * @ver: ov3640 chip version
 * @width: configured width
 * @height: configuredheight
 * @vsize: vertical size for the image
 * @hsize: horizontal size for the image
 */
struct ov3640 {
	struct v4l2_subdev subdev;
	struct v4l2_subdev_sensor_interface_parms *plat_parms;
	struct v4l2_fract timeperframe;
	int i_size;
	int i_fmt;
	int ver;
	int fps;
};

static struct ov3640 *to_ov3640(const struct i2c_client *client)
{
	return container_of(i2c_get_clientdata(client), struct ov3640, subdev);
}

/* OV3640 has only one fixed colorspace per pixelcode */
struct ov3640_datafmt {
	enum v4l2_mbus_pixelcode	code;
	enum v4l2_colorspace		colorspace;
};

/* List of image formats supported by OV3640 sensor */
static const struct ov3640_datafmt ov3640_fmts[] = {
	{V4L2_MBUS_FMT_YUYV8_2X8, V4L2_COLORSPACE_JPEG},
};


static const struct v4l2_fmtdesc ov3640_formats[] = {
#ifndef CONFIG_SOC_CAMERA_OV3640_ISP
	{
		.description	= "RAW10",
		.pixelformat	= V4L2_PIX_FMT_SGRBG10,
	},
#else
	{
		.description	= "RGB565, le",
		.pixelformat	= V4L2_PIX_FMT_RGB565,
	},
	{
		.description	= "RGB565, be",
		.pixelformat	= V4L2_PIX_FMT_RGB565X,
	},
	{
		.description	= "YUYV (YUV 4:2:2), packed",
		.pixelformat	= V4L2_PIX_FMT_YUYV,
	},
	{
		.description	= "UYVY, packed",
		.pixelformat	= V4L2_PIX_FMT_UYVY,
	},
	{
		.description	= "RGB555, le",
		.pixelformat	= V4L2_PIX_FMT_RGB555,
	},
	{
		.description	= "RGB555, be",
		.pixelformat	= V4L2_PIX_FMT_RGB555X,
	},
#endif
};

#define OV3640_NUM_CAPTURE_FORMATS \
			(sizeof(ov3640_formats) / sizeof(ov3640_formats[0]))

/* register initialization tables for ov3640 */

static const struct ov3640_reg ov3640_out_xga[] = {
	{0x3088, 0x04},
	{0x3089, 0x00},
	{0x308A, 0x03},
	{0x308B, 0x00},
};

static const struct ov3640_reg ov3640_out_qxga[] = {
	{0x3088, 0x08},
	{0x3089, 0x00},
	{0x308A, 0x06},
	{0x308B, 0x00},
};

/* Brightness Settings - 7 levels */
static const struct ov3640_reg brightness[7][5] = {
	{
		{0x3355, 0x04},
		{0x3354, 0x09},
		{0x335E, 0x30},
	},
	{
		{0x3355, 0x04},
		{0x3354, 0x09},
		{0x335E, 0x20},
	},
	{
		{0x3355, 0x04},
		{0x3354, 0x09},
		{0x335E, 0x10},
	},
	{
		{0x3355, 0x04},
		{0x3354, 0x01},
		{0x335E, 0x00},
	},
	{
		{0x3355, 0x04},
		{0x3354, 0x01},
		{0x335E, 0x10},
	},
	{
		{0x3355, 0x04},
		{0x3354, 0x01},
		{0x335E, 0x20},
	},
	{
		{0x3355, 0x04},
		{0x3354, 0x01},
		{0x335E, 0x30},
	},
};

/* Contrast Settings - 7 levels */
static const struct ov3640_reg contrast[7][5] = {
	{
		{0x3355, 0x04},
		{0x335C, 0x14},
		{0x335D, 0x14},
	},
	{
		{0x3355, 0x04},
		{0x335C, 0x18},
		{0x335D, 0x18},
	},
	{
		{0x3355, 0x04},
		{0x335C, 0x1c},
		{0x335D, 0x1c},
	},
	{
		{0x3355, 0x04},
		{0x335C, 0x20},
		{0x335D, 0x20},
	},
	{
		{0x3355, 0x04},
		{0x335C, 0x24},
		{0x335D, 0x24},
	},
	{
		{0x3355, 0x04},
		{0x335C, 0x28},
		{0x335D, 0x28},
	},
	{
		{0x3355, 0x04},
		{0x335C, 0x2c},
		{0x335D, 0x2c},
	},
};

/* Color Settings - 3 colors */
static const struct ov3640_reg colors[3][5] = {
	{
		{0x3355, 0x00},
		{0x335A, 0x80},
		{0x335B, 0x80},
	},
	{
		{0x3355, 0x18},
		{0x335A, 0x80},
		{0x335B, 0x80},
	},
	{
		{0x3355, 0x18},
		{0x335A, 0x40},
		{0x335B, 0xa6},
	},
};

/* Average Based Algorithm - Based on target Luminance */
static const struct ov3640_reg exposure_avg[11][5] = {
	/* -1.7EV */
	{
		{0x3047, 0x00},
		{0x3018, 0x10},
		{0x3019, 0x08},
		{0x301A, 0x21},
	},
	/* -1.3EV */
	{
		{0x3047, 0x00},
		{0x3018, 0x18},
		{0x3019, 0x10},
		{0x301A, 0x31},
	},
	/* -1.0EV */
	{
		{0x3047, 0x00},
		{0x3018, 0x20},
		{0x3019, 0x18},
		{0x301A, 0x41},
	},
	/* -0.7EV */
	{
		{0x3047, 0x00},
		{0x3018, 0x28},
		{0x3019, 0x20},
		{0x301A, 0x51},
	},
	/* -0.3EV */
	{
		{0x3047, 0x00},
		{0x3018, 0x30},
		{0x3019, 0x28},
		{0x301A, 0x61},
	},
	/* default */
	{
		{0x3047, 0x00},
		{0x3018, 0x38},
		{0x3019, 0x30},
		{0x301A, 0x61},
	},
	/* 0.3EV */
	{
		{0x3047, 0x00},
		{0x3018, 0x40},
		{0x3019, 0x38},
		{0x301A, 0x71},
	},
	/* 0.7EV */
	{
		{0x3047, 0x00},
		{0x3018, 0x48},
		{0x3019, 0x40},
		{0x301A, 0x81},
	},
	/* 1.0EV */
	{
		{0x3047, 0x00},
		{0x3018, 0x50},
		{0x3019, 0x48},
		{0x301A, 0x91},
	},
	/* 1.3EV */
	{
		{0x3047, 0x00},
		{0x3018, 0x58},
		{0x3019, 0x50},
		{0x301A, 0x91},
	},
	/* 1.7EV */
	{
		{0x3047, 0x00},
		{0x3018, 0x60},
		{0x3019, 0x58},
		{0x301A, 0xa1},
	},
};

/* Histogram Based Algorithm - Based on histogram and probability */
static const struct ov3640_reg exposure_hist[11][5] = {
	/* -1.7EV */
	{
		{0x3047, 0x80},
		{0x3018, 0x58},
		{0x3019, 0x38},
	},
	/* -1.3EV */
	{
		{0x3047, 0x80},
		{0x3018, 0x60},
		{0x3019, 0x40},
	},
	/* -1.0EV */
	{
		{0x3047, 0x80},
		{0x3018, 0x68},
		{0x3019, 0x48},
	},
	/* -0.7EV */
	{
		{0x3047, 0x80},
		{0x3018, 0x70},
		{0x3019, 0x50},
	},
	/* -0.3EV */
	{
		{0x3047, 0x80},
		{0x3018, 0x78},
		{0x3019, 0x58},
	},
	/* default */
	{
		{0x3047, 0x80},
		{0x3018, 0x80},
		{0x3019, 0x60},
	},
	/* 0.3EV */
	{
		{0x3047, 0x80},
		{0x3018, 0x88},
		{0x3019, 0x68},
	},
	/* 0.7EV */
	{
		{0x3047, 0x80},
		{0x3018, 0x90},
		{0x3019, 0x70},
	},
	/* 1.0EV */
	{
		{0x3047, 0x80},
		{0x3018, 0x98},
		{0x3019, 0x78},
	},
	/* 1.3EV */
	{
		{0x3047, 0x80},
		{0x3018, 0xa0},
		{0x3019, 0x80},
	},
	/* 1.7EV */
	{
		{0x3047, 0x80},
		{0x3018, 0xa8},
		{0x3019, 0x88},
	},
};

/* ov3640 register configuration for combinations of pixel format and
 * image size
 */

static const struct ov3640_reg qxga_yuv[] = {
	{0x3100, 0x02},
	{0x3304, 0xFC},
	{0x3400, 0x00},
	{0x3404, 0x00},
	{0x3601, 0x01},
	{0x302A, 0x06},
	{0x302B, 0x20},
};

static const struct ov3640_reg qxga_565[] = {
	{0x3100, 0x02},
	{0x3304, 0xFC},
	{0x3400, 0x01},
	{0x3404, 0x11},
	{0x3601, 0x01},
	{0x302A, 0x06},
	{0x302B, 0x20},
};

static const struct ov3640_reg qxga_555[] = {
	{0x3100, 0x02},
	{0x3304, 0xFC},
	{0x3400, 0x01},
	{0x3404, 0x13},
	{0x3601, 0x01},
	{0x302A, 0x06},
	{0x302B, 0x20},
};

static const struct ov3640_reg qxga_raw10[] = {
	{0x3100, 0x22},
	{0x3304, 0x01},
	{0x3400, 0x04},
	{0x3404, 0x18},
	{0x3601, 0x00},
	{0x302A, 0x06},
	{0x302B, 0x20},
};

static const struct ov3640_reg xga_yuv[] = {
	{0x3100, 0x02},
	{0x3304, 0xFC},
	{0x3400, 0x00},
	{0x3404, 0x00},
	{0x3601, 0x01},
	{0x302A, 0x03},
	{0x302B, 0x10},
};

static const struct ov3640_reg xga_565[] = {
	{0x3100, 0x02},
	{0x3304, 0xFC},
	{0x3400, 0x01},
	{0x3404, 0x11},
	{0x3601, 0x01},
	{0x302A, 0x03},
	{0x302B, 0x10},
};

static const struct ov3640_reg xga_555[] = {
	{0x3100, 0x02},
	{0x3304, 0xFC},
	{0x3400, 0x01},
	{0x3404, 0x13},
	{0x3601, 0x01},
	{0x302A, 0x03},
	{0x302B, 0x10},
};

static const struct ov3640_reg xga_raw10[] = {
	{0x3100, 0x22},
	{0x3304, 0x01},
	{0x3400, 0x04},
	{0x3404, 0x18},
	{0x3601, 0x00},
	{0x302A, 0x03},
	{0x302B, 0x10},
};

static const struct ov3640_reg
	*ov3640_reg_init[OV3640_NUM_PIXEL_FORMATS][OV3640_NUM_IMAGE_SIZES] = {
	{xga_yuv, qxga_yuv},
	{xga_565, qxga_565},
	{xga_555, qxga_555},
	{xga_raw10, qxga_raw10}
};

/*
 * struct vcontrol - Video controls
 * @v4l2_queryctrl: V4L2 VIDIOC_QUERYCTRL ioctl structure
 * @current_value: current value of this control
 */
static const struct v4l2_queryctrl ov3640_controls[] = {
#if defined(CONFIG_SOC_CAMERA_OV3640_ISP)
	{
		.id = V4L2_CID_BRIGHTNESS,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "Brightness",
		.minimum = OV3640_MIN_BRIGHT,
		.maximum = OV3640_MAX_BRIGHT,
		.step = OV3640_BRIGHT_STEP,
		.default_value = OV3640_DEF_BRIGHT,
	},
	{
		.id = V4L2_CID_CONTRAST,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "Contrast",
		.minimum = OV3640_MIN_CONTRAST,
		.maximum = OV3640_MAX_CONTRAST,
		.step = OV3640_CONTRAST_STEP,
		.default_value = OV3640_DEF_CONTRAST,
	},
	{
		.id = V4L2_CID_COLORFX,
		.type = V4L2_CTRL_TYPE_MENU,
		.name = "Color Effects",
		.minimum = V4L2_COLORFX_NONE,
		.maximum = V4L2_COLORFX_SEPIA,
		.step = 1,
		.default_value = V4L2_COLORFX_NONE,
	}
#endif
};

/*
 * find_vctrl - Finds the requested ID in the video control structure array
 * @id: ID of control to search the video control array.
 *
 * Returns the index of the requested ID from the control structure array
 */
static int find_vctrl(int id)
{
	int i = 0;

	if (id < V4L2_CID_BASE)
		return -EDOM;

	for (i = (ARRAY_SIZE(ov3640_controls) - 1); i >= 0; i--)
		if (ov3640_controls[i].id == id)
			break;
	if (i < 0)
		i = -EINVAL;
	return i;
}

/**
 * ov3640_reg_read - Read a value from a register in an ov3640 sensor device
 * @client: i2c driver client structure
 * @reg: register address / offset
 * @val: stores the value that gets read
 *
 * Read a value from a register in an ov3640 sensor device.
 * The value is returned in 'val'.
 * Returns zero if successful, or non-zero otherwise.
 */
static int ov3640_reg_read(struct i2c_client *client, u16 reg, u8 *val)
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
 * Write a value to a register in ov3640 sensor device.
 * @client: i2c driver client structure.
 * @reg: Address of the register to read value from.
 * @val: Value to be written to a specific register.
 * Returns zero if successful, or non-zero otherwise.
 */
static int ov3640_reg_write(struct i2c_client *client, u16 reg, u8 val)
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
 * Initialize a list of ov3640 registers.
 * The list of registers is terminated by the pair of values
 * @client: i2c driver client structure.
 * @reglist[]: List of address of the registers to write data.
 * Returns zero if successful, or non-zero otherwise.
 */
static int ov3640_reg_writes(struct i2c_client *client,
			     const struct ov3640_reg reglist[],
			     int size)
{
	int err = 0, i;

	for (i = 0; i < size; i++) {
		err = ov3640_reg_write(client, reglist[i].reg,
				reglist[i].val);
		if (err)
			return err;
	}
	return 0;
}

/* Find a data format by a pixel code in an array */
static int ov3640_find_datafmt(enum v4l2_mbus_pixelcode code)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(ov3640_fmts); i++)
		if (ov3640_fmts[i].code == code)
			break;

	return i;
}

/* Find the best match for a requested image capture size.  The best match
 * is chosen as the nearest match that has the same number or fewer pixels
 * as the requested size, or the smallest image size if the requested size
 * has fewer pixels than the smallest image.
 */
static int ov3640_find_size(unsigned int width, unsigned int height)
{
	if ((width > ov3640_sizes[XGA].width) ||
	    (height > ov3640_sizes[XGA].height))
		return QXGA;
	return XGA;
}

/*
 * Set CSI2 Virtual ID.
 */
static int ov3640_set_virtual_id(struct i2c_client *client, u32 id)
{
	return ov3640_reg_write(client, 0x360C, (0x3 & id) << 6 |
									0x02);
}

/*
 * Calculates the MIPIClk.
 * 1) Calculate fclk
 *     fclk = (64 - 0x300E[5:0]) * N * Bit8Div * MCLK / M
 *    where N = 1/1.5/2/3 for 0x300F[7:6]=0~3
 *          M = 1/1.5/2/3 for 0x300F[1:0]=0~3
 *    Bit8Div = 1/1/4/5 for 0x300F[5:4]
 * 2) Calculate MIPIClk
 *     MIPIClk = fclk / ScaleDiv / MIPIDiv
 *             = fclk * (1/ScaleDiv) / MIPIDiv
 *    where 1/ScaleDiv = 0x3010[3:0]*2
 *          MIPIDiv = 0x3010[5] + 1
 * NOTE:
 *  - The lookup table 'lut1' has been multiplied by 2 so all its values
 *    are integers. Since both N & M use the same table, and they are
 *    used as a ratio then the factor of 2 is already take into account.
 *    i.e.  2N/2M = N/M
 */
static u32 ov3640_calc_mipiclk(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	u32 rxpll, n, m, bit8div;
	u32 sdiv_inv, mipidiv;
	u32 fclk, mipiclk, mclk = 24000000;
	u8 lut1[4] = {2, 3, 4, 6};
	u8 lut2[4] = {1, 1, 4, 5};
	u8 val = 0;

	/* Calculate fclk */
	ov3640_reg_read(client, 0x300E, &val);
	rxpll = val & 0x3F;

	ov3640_reg_read(client, 0x300F, &val);
	n = lut1[(val >> 6) & 0x3];
	m = lut1[val & 0x3];
	bit8div = lut2[(val >> 4) & 0x3];
	fclk = (64 - rxpll) * n * bit8div * mclk / m;

	ov3640_reg_read(client, 0x3010, &val);
	mipidiv = ((val >> 5) & 1) + 1;
	sdiv_inv = (val & 0xF) * 2;

	if ((val & 0xF) >= 1)
		mipiclk = fclk / sdiv_inv / mipidiv;
	else
		mipiclk = fclk / mipidiv;
	dev_dbg(&client->dev, "mipiclk=%u  fclk=%u  val&0xF=%u  sdiv_inv=%u  "
							"mipidiv=%u\n",
							mipiclk, fclk, val&0xF,
							sdiv_inv, mipidiv);
	return mipiclk;
}

/**
 * ov3640_set_framerate
 **/
static int ov3640_set_framerate(struct i2c_client *client,
				struct v4l2_fract *fper,
				enum ov3640_image_size isize)
{
	u32 tempfps1, tempfps2;
	u8 clkval;
	int err = 0;

	/* FIXME: QXGA framerate setting forced to 15 FPS */
	if (isize == QXGA) {
		err = ov3640_reg_write(client, 0x300E, 0x32);
		err = ov3640_reg_write(client, 0x300F, 0x21);
		err = ov3640_reg_write(client, 0x3010, 0x21);
		err = ov3640_reg_write(client, 0x3011, 0x01);
		err = ov3640_reg_write(client, 0x304c, 0x81);
		return err;
	}

	tempfps1 = fper->denominator * 10000;
	tempfps1 /= fper->numerator;
	tempfps2 = fper->denominator / fper->numerator;
	if ((tempfps1 % 10000) != 0)
		tempfps2++;
	clkval = (u8)((30 / tempfps2) - 1);

	err = ov3640_reg_write(client, 0x3011, clkval);
	/* RxPLL = 50d = 32h */
	err = ov3640_reg_write(client, 0x300E, 0x32);
	/* RxPLL = 50d = 32h */
	err = ov3640_reg_write(client, 0x300F,
					(0x2 << 4) |
					0x1);
	/*
	 * NOTE: Sergio's Fix for MIPI CLK timings, not suggested by OV
	 */
	err = ov3640_reg_write(client, 0x3010, 0x21 +
							(clkval & 0xF));
	/* Setting DVP divisor value */
	err = ov3640_reg_write(client, 0x304c, 0x82);
	return err;
}

static int ov3640_configure(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct ov3640 *ov3640 = to_ov3640(client);
	int err = 0;
	u32 mipiclk;
	int i_fmt = ov3640->i_fmt;
	int i_size = ov3640->i_size;

	/* Reset */
	ov3640_reg_write(client, 0x3012, 0x80);
	mdelay(5);

	/* Common registers */
	err = ov3640_reg_writes(client, ov3640_common[i_size],
				ARRAY_SIZE(ov3640_common[i_size]));

	/* Configure image size and pixel format */
	err = ov3640_reg_writes(client, ov3640_reg_init[i_fmt][i_size],
				7);

	/* Setting of frame rate (OV suggested way) */
	err = ov3640_set_framerate(client, &ov3640->timeperframe, i_size);
#ifdef CONFIG_SOC_CAMERA_OV3640_CSI2
	/* Set CSI2 common register settings */
	err = ov3640_reg_writes(client, ov3640_common_csi2,
				ARRAY_SIZE(ov3640_common_csi2));
#endif
	if (i_size == XGA)
		ov3640_reg_writes(client, ov3640_out_xga,
				  ARRAY_SIZE(ov3640_out_xga));
	else
		ov3640_reg_writes(client, ov3640_out_qxga,
				  ARRAY_SIZE(ov3640_out_qxga));

#ifdef CONFIG_SOC_CAMERA_OV3640_CSI2
	mipiclk = ov3640_calc_mipiclk(sd);

	/* Set sensors virtual channel*/
	ov3640_set_virtual_id(client, OV3640_CSI2_VIRTUAL_ID);
#endif
	return err;
}


/* Detect if an ov3640 is present, returns a negative error number if no
 * device is detected, or pidl as version number if a device is detected.
 */
static int ov3640_detect(struct i2c_client *client)
{
	u8 pidh, pidl;

	if (!client)
		return -ENODEV;

	if (ov3640_reg_read(client, 0x300A, &pidh))
		return -ENODEV;

	if (ov3640_reg_read(client, 0x300B, &pidl))
		return -ENODEV;

	if ((pidh == 0x36) && ((pidl == 0x41) ||
						(pidl == 0x4C))) {
		dev_info(&client->dev, "Detect success (%02X,%02X)\n", pidh,
									pidl);
		return pidl;
	}

	return -ENODEV;
}

static int ov3640_video_probe(struct soc_camera_device *icd,
			      struct i2c_client *client)
{
	struct ov3640 *ov3640 = to_ov3640(client);
	int ver;

	ver = ov3640_detect(client);
	if (ver < 0) {
		dev_err(&client->dev, "Unable to detect sensor, err %d\n",
			ver);
		return ver;
	}
	ov3640->ver = ver;
	dev_dbg(&client->dev, "Chip version 0x%02x detected\n", ov3640->ver);

	return 0;
}

static int ov3640_g_chip_ident(struct v4l2_subdev *sd,
				struct v4l2_dbg_chip_ident *id)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	if (id->match.type != V4L2_CHIP_MATCH_I2C_ADDR)
		return -EINVAL;

	if (id->match.addr != client->addr)
		return -ENODEV;

	id->ident	= V4L2_IDENT_OV3640;
	id->revision	= 0;

	return 0;
}

static int ov3640_g_ctrl(struct v4l2_subdev *sd, struct v4l2_control *vc)
{
	int i;

	i = find_vctrl(vc->id);
	if (i < 0)
		return -EINVAL;

	/* FIXME: This is just an ugly hack, need to read actual values */
	vc->value = ov3640_controls[i].default_value;
	return 0;
}

static int ov3640_s_ctrl(struct v4l2_subdev *sd, struct v4l2_control *vc)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret = -EINVAL;
	int i;

	i = find_vctrl(vc->id);
	if (i < 0)
		return -EINVAL;

	if ((vc->value < ov3640_controls[i].minimum) ||
	    (vc->value > ov3640_controls[i].maximum)) {
		return -ERANGE;
	}

	switch (vc->id) {
	case V4L2_CID_BRIGHTNESS:
		ret = ov3640_reg_writes(client,
				   brightness[vc->value],
				   ARRAY_SIZE(brightness[vc->value]));
		break;
	case V4L2_CID_CONTRAST:
		ret = ov3640_reg_writes(client, contrast[vc->value],
				ARRAY_SIZE(contrast[vc->value]));
		break;
	case V4L2_CID_COLORFX:
		ret = ov3640_reg_writes(client, colors[vc->value],
					ARRAY_SIZE(colors[vc->value]));
		break;
	}
	return ret;
}

#ifdef CONFIG_VIDEO_ADV_DEBUG
static int ov3640_g_register(struct v4l2_subdev *sd,
			     struct v4l2_dbg_register *reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	if (reg->match.type != V4L2_CHIP_MATCH_I2C_ADDR || reg->size > 2)
		return -EINVAL;

	if (reg->match.addr != client->addr)
		return -ENODEV;

	reg->size = 2;
	if (ov3640_reg_read(client, reg->reg, &reg->val))
		return -EIO

	return 0;
}

static int ov3640_s_register(struct v4l2_subdev *sd,
			     struct v4l2_dbg_register *reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	if (reg->match.type != V4L2_CHIP_MATCH_I2C_ADDR || reg->size > 2)
		return -EINVAL;

	if (reg->match.addr != client->addr)
		return -ENODEV;

	if (ov3640_reg_write(client, reg->reg, reg->val))
		return -EIO;

	return 0;
}
#endif

static struct v4l2_subdev_core_ops ov3640_subdev_core_ops = {
	.g_chip_ident	= ov3640_g_chip_ident,
	.g_ctrl		= ov3640_g_ctrl,
	.s_ctrl		= ov3640_s_ctrl,
#ifdef CONFIG_VIDEO_ADV_DEBUG
	.g_register	= ov3640_g_register,
	.s_register	= ov3640_s_register,
#endif
};

static int ov3640_s_stream(struct v4l2_subdev *sd, int enable)
{
	if (enable)
		ov3640_configure(sd);

	return 0;
}

static int ov3640_try_fmt(struct v4l2_subdev *sd,
			   struct v4l2_mbus_framefmt *mf)
{
	int i_fmt;
	int i_size;

	i_fmt = ov3640_find_datafmt(mf->code);

	mf->code = ov3640_fmts[i_fmt].code;
	mf->colorspace = ov3640_fmts[i_fmt].colorspace;
	mf->field = V4L2_FIELD_NONE;

	i_size = ov3640_find_size(mf->width, mf->height);

	mf->width = ov3640_sizes[i_size].width;
	mf->height = ov3640_sizes[i_size].height;

	return 0;
}

static int ov3640_s_fmt(struct v4l2_subdev *sd,
			 struct v4l2_mbus_framefmt *mf)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct ov3640 *ov3640 = to_ov3640(client);
	int ret;

	ret = ov3640_try_fmt(sd, mf);
	if (ret < 0)
		return ret;

	ov3640->i_size = ov3640_find_size(mf->width, mf->height);
	ov3640->i_fmt = ov3640_find_datafmt(mf->code);

	return 0;
}

static int ov3640_g_fmt(struct v4l2_subdev *sd,
			 struct v4l2_mbus_framefmt *mf)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct ov3640 *ov3640 = to_ov3640(client);

	mf->width	= ov3640_sizes[ov3640->i_size].width;
	mf->height	= ov3640_sizes[ov3640->i_size].height;
	mf->code	= ov3640_fmts[ov3640->i_fmt].code;
	mf->colorspace	= ov3640_fmts[ov3640->i_fmt].colorspace;
	mf->field	= V4L2_FIELD_NONE;

	return 0;
}

static int ov3640_enum_fmt(struct v4l2_subdev *sd, unsigned int index,
			    enum v4l2_mbus_pixelcode *code)
{
	if (index >= ARRAY_SIZE(ov3640_fmts))
		return -EINVAL;

	*code = ov3640_fmts[index].code;
	return 0;
}

static int ov3640_enum_framesizes(struct v4l2_subdev *sd,
				 struct v4l2_frmsizeenum *frms)
{
	int ifmt;

	for (ifmt = 0; ifmt < OV3640_NUM_CAPTURE_FORMATS; ifmt++) {
		if (frms->pixel_format == ov3640_formats[ifmt].pixelformat)
			break;
	}
	/* Is requested pixelformat not found on sensor? */
	if (ifmt == OV3640_NUM_CAPTURE_FORMATS)
		return -EINVAL;

	/* Do we already reached all discrete framesizes? */
	if (frms->index >= 2)
		return -EINVAL;

	frms->type = V4L2_FRMSIZE_TYPE_DISCRETE;
	frms->discrete.width = ov3640_sizes[frms->index].width;
	frms->discrete.height = ov3640_sizes[frms->index].height;

	return 0;
}

static int ov3640_enum_frameintervals(struct v4l2_subdev *sd,
				     struct v4l2_frmivalenum *frmi)
{
	int ifmt;

	for (ifmt = 0; ifmt < OV3640_NUM_CAPTURE_FORMATS; ifmt++) {
		if (frmi->pixel_format == ov3640_formats[ifmt].pixelformat)
			break;
	}
	/* Is requested pixelformat not found on sensor? */
	if (ifmt == OV3640_NUM_CAPTURE_FORMATS)
		return -EINVAL;

	/* Do we already reached all discrete framesizes? */

	if ((frmi->width == ov3640_sizes[1].width) &&
				(frmi->height == ov3640_sizes[1].height)) {
		/* FIXME: The only frameinterval supported by QXGA capture is
		 * 2/15 fps
		 */
		if (frmi->index != 0)
			return -EINVAL;
	} else {
		if (frmi->index >= 3)
			return -EINVAL;
	}

	frmi->type = V4L2_FRMIVAL_TYPE_DISCRETE;
	frmi->discrete.numerator =
				ov3640_frameintervals[frmi->index].numerator;
	frmi->discrete.denominator =
				ov3640_frameintervals[frmi->index].denominator;

	return 0;
}

static int ov3640_g_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *a)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct ov3640 *ov3640 = to_ov3640(client);
	struct v4l2_captureparm *cparm = &a->parm.capture;

	if (a->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		return -EINVAL;

	memset(a, 0, sizeof(*a));
	a->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	cparm->capability = V4L2_CAP_TIMEPERFRAME;
	cparm->timeperframe = ov3640->timeperframe;

	return 0;
}

static int ov3640_s_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *a)
{
	int rval = 0;
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct ov3640 *ov3640 = to_ov3640(client);
	struct v4l2_fract *timeperframe = &a->parm.capture.timeperframe;
	struct v4l2_fract timeperframe_old;
	int desired_fps;

	timeperframe_old = ov3640->timeperframe;
	ov3640->timeperframe = *timeperframe;

	desired_fps = timeperframe->denominator / timeperframe->numerator;
	if ((desired_fps < OV3640_MIN_FPS) || (desired_fps > OV3640_MAX_FPS))
		rval = -EINVAL;

	if (rval)
		ov3640->timeperframe = timeperframe_old;
	else
		*timeperframe = ov3640->timeperframe;

	return rval;
}

static struct v4l2_subdev_video_ops ov3640_subdev_video_ops = {
	.s_stream	= ov3640_s_stream,
	.try_mbus_fmt	= ov3640_try_fmt,
	.s_mbus_fmt	= ov3640_s_fmt,
	.g_mbus_fmt	= ov3640_g_fmt,
	.enum_mbus_fmt	= ov3640_enum_fmt,
	.enum_framesizes = ov3640_enum_framesizes,
	.enum_frameintervals = ov3640_enum_frameintervals,
	.g_parm = ov3640_g_parm,
	.s_parm = ov3640_s_parm,
};

static int ov3640_g_skip_frames(struct v4l2_subdev *sd, u32 *frames)
{
	/* Quantity of initial bad frames to skip. Revisit. */
	*frames = 3;

	return 0;
}

static int ov3640_g_interface_parms(struct v4l2_subdev *sd,
			struct v4l2_subdev_sensor_interface_parms *parms)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct ov3640 *ov3640 = to_ov3640(client);

	if (!parms)
		return -EINVAL;

	parms->if_type = ov3640->plat_parms->if_type;
	parms->if_mode = ov3640->plat_parms->if_mode;
	/* FIXME */
	parms->parms.serial.lanes = 2;
	parms->parms.serial.channel = OV3640_CSI2_VIRTUAL_ID;
	parms->parms.serial.phy_rate = 224000000; /* FIX: ov3640_calc_mipiclk */
	parms->parms.serial.pix_clk = 21; /* Revisit */

	return 0;
}

static struct v4l2_subdev_sensor_ops ov3640_subdev_sensor_ops = {
	.g_skip_frames	= ov3640_g_skip_frames,
	.g_interface_parms = ov3640_g_interface_parms,
};

static struct v4l2_subdev_ops ov3640_subdev_ops = {
	.core	= &ov3640_subdev_core_ops,
	.video	= &ov3640_subdev_video_ops,
	.sensor	= &ov3640_subdev_sensor_ops,
};

static int ov3640_set_bus_param(struct soc_camera_device *icd,
				 unsigned long flags)
{
	/* TODO: Do the right thing here, and validate bus params */
	return 0;
}

static unsigned long ov3640_query_bus_param(struct soc_camera_device *icd)
{
	unsigned long flags = SOCAM_PCLK_SAMPLE_FALLING |
		SOCAM_HSYNC_ACTIVE_HIGH | SOCAM_VSYNC_ACTIVE_HIGH |
		SOCAM_DATA_ACTIVE_HIGH | SOCAM_MASTER;

	/* TODO: Do the right thing here, and validate bus params */

	flags |= SOCAM_DATAWIDTH_10;

	return flags;
}

static struct soc_camera_ops ov3640_ops = {
	.set_bus_param		= ov3640_set_bus_param,
	.query_bus_param	= ov3640_query_bus_param,
	.controls		= ov3640_controls,
	.num_controls		= ARRAY_SIZE(ov3640_controls),
};

/*
 * ov3640_probe - sensor driver i2c probe handler
 * @client: i2c driver client device structure
 *
 * Register sensor as an i2c client device and V4L2
 * device.
 */
static int ov3640_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct ov3640 *ov3640;
	struct soc_camera_device *icd = client->dev.platform_data;
	struct soc_camera_link *icl;
	int ret;

	if (!icd) {
		dev_err(&client->dev, "OV3640: missing soc-camera data!\n");
		return -EINVAL;
	}

	icl = to_soc_camera_link(icd);
	if (!icl) {
		dev_err(&client->dev, "OV3640 driver needs platform data\n");
		return -EINVAL;
	}

	if (!icl->priv) {
		dev_err(&client->dev,
			"OV3640 driver needs i/f platform data\n");
		return -EINVAL;
	}

	ov3640 = kzalloc(sizeof(struct ov3640), GFP_KERNEL);
	if (!ov3640)
		return -ENOMEM;

	v4l2_i2c_subdev_init(&ov3640->subdev, client, &ov3640_subdev_ops);

	/* Second stage probe - when a capture adapter is there */
	icd->ops		= &ov3640_ops;

	/* Set sensor default values */
	ov3640->i_size = XGA;
	ov3640->i_fmt = 0; /* First format in the list */
	ov3640->timeperframe.numerator = 1;
	ov3640->timeperframe.denominator = 15;
	ov3640->plat_parms = icl->priv;

	ret = ov3640_video_probe(icd, client);
	if (ret) {
		icd->ops = NULL;
		kfree(ov3640);
	}

	return 0;
}

static int ov3640_remove(struct i2c_client *client)
{
	struct ov3640 *ov3640 = to_ov3640(client);
	struct soc_camera_device *icd = client->dev.platform_data;

	icd->ops = NULL;
	client->driver = NULL;
	kfree(ov3640);

	return 0;
}

static const struct i2c_device_id ov3640_id[] = {
	{ OV3640_DRIVER_NAME, 0 },
	{ },
};
MODULE_DEVICE_TABLE(i2c, ov3640_id);

static struct i2c_driver ov3640_i2c_driver = {
	.driver = {
		.name	= OV3640_DRIVER_NAME,
	},
	.probe		= ov3640_probe,
	.remove		= ov3640_remove,
	.id_table	= ov3640_id,
};

static int __init ov3640_mod_init(void)
{
	return i2c_add_driver(&ov3640_i2c_driver);
}

static void __exit ov3640_mod_exit(void)
{
	i2c_del_driver(&ov3640_i2c_driver);
}

module_init(ov3640_mod_init);
module_exit(ov3640_mod_exit);

MODULE_DESCRIPTION("OmniVision OV3640 Camera driver");
MODULE_AUTHOR("Sergio Aguirre <saaguirre@ti.com>");
MODULE_LICENSE("GPL v2");
