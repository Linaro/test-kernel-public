/* drivers/media/video/s5k4ec.c
 *
 * Driver for s5k4ec (5MP Camera) from SEC
 *
 * Copyright (C) 2010, SAMSUNG ELECTRONICS
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/version.h>
#include <linux/vmalloc.h>
#include <linux/completion.h>
#include <linux/slab.h>
#include <linux/export.h>
#include <linux/module.h>
#include <linux/clk.h>
#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>
#include <media/v4l2-ctrls.h>
#include <media/s5k4ecgx.h>
#include <linux/videodev2.h>
#include <linux/videodev2_samsung.h>

#include "s5k4ecgx_regset.h"

/* Should include twice to generate regset and data */
#ifdef CONFIG_VIDEO_S5K4ECGX_V_1_0
#include "s5k4ecgx_regs_1_0.h"
#include "s5k4ecgx_regs_1_0.h"
#endif /* CONFIG_VIDEO_S5K4ECGX_V_1_0 */

#ifdef CONFIG_VIDEO_S5K4ECGX_V_1_1
#ifndef CONFIG_VIDEO_S5K4ECGX_SLSI_4EC
#include "s5k4ecgx_regs_1_1.h"
#include "s5k4ecgx_regs_1_1.h"
#else
#include "s5k4ecgx_regs_1_1_slsi-4ec.h"
#include "s5k4ecgx_regs_1_1_slsi-4ec.h"
#endif
#endif /* CONFIG_VIDEO_S5K4ECGX_V_1_1 */

#define FORMAT_FLAGS_COMPRESSED		0x3
#define SENSOR_JPEG_SNAPSHOT_MEMSIZE	0x410580

#define DEFAULT_PIX_FMT		V4L2_PIX_FMT_UYVY	/* YUV422 */
#define DEFAULT_MCLK		24000000
#define POLL_TIME_MS		10
#define CAPTURE_POLL_TIME_MS	1000

/* maximum time for one frame at minimum fps (15fps) in normal mode */
#define NORMAL_MODE_MAX_ONE_FRAME_DELAY_MS	67
/* maximum time for one frame at minimum fps (4fps) in night mode */
#define NIGHT_MODE_MAX_ONE_FRAME_DELAY_MS	250

/* time to move lens to target position before last af mode register write */
#define LENS_MOVE_TIME_MS	100

/* level at or below which we need to enable flash when in auto mode */
#define LOW_LIGHT_LEVEL		0x1D

/* level at or below which we need to use low light capture mode */
#define HIGH_LIGHT_LEVEL	0x80

#define FIRST_AF_SEARCH_COUNT	80
#define SECOND_AF_SEARCH_COUNT	80
#define AE_STABLE_SEARCH_COUNT	4

#ifdef CONFIG_VIDEO_S5K4ECGX_DEBUG
enum {
	S5K4ECGX_DEBUG_I2C		= 1U << 0,
	S5K4ECGX_DEBUG_I2C_BURSTS	= 1U << 1,
};
static uint32_t s5k4ecgx_debug_mask = S5K4ECGX_DEBUG_I2C_BURSTS;
module_param_named(debug_mask, s5k4ecgx_debug_mask, uint, S_IWUSR | S_IRUGO);

#define s5k4ecgx_debug(mask, x...) \
	do { \
		if (s5k4ecgx_debug_mask & mask) \
			pr_info(x);	\
	} while (0)
#else

#define s5k4ecgx_debug(mask, x...)

#endif

#define S5K4ECGX_VERSION_1_0	0x00
#define S5K4ECGX_VERSION_1_1	0x11

enum s5k4ecgx_oprmode {
	S5K4ECGX_OPRMODE_VIDEO = 0,
	S5K4ECGX_OPRMODE_IMAGE = 1,
};

enum s5k4ecgx_preview_frame_size {
	S5K4ECGX_PREVIEW_QCIF = 0,	/* 176x144 */
	S5K4ECGX_PREVIEW_CIF,		/* 352x288 */
	S5K4ECGX_PREVIEW_VGA,		/* 640x480 */
	S5K4ECGX_PREVIEW_D1,		/* 720x480 */
	S5K4ECGX_PREVIEW_WVGA,		/* 800x480 */
	S5K4ECGX_PREVIEW_SVGA,		/* 800x600 */
	S5K4ECGX_PREVIEW_WSVGA,		/* 1024x600*/
	S5K4ECGX_PREVIEW_MAX,
};

enum s5k4ecgx_capture_frame_size {
	S5K4ECGX_CAPTURE_VGA = 0,	/* 640x480 */
	S5K4ECGX_CAPTURE_WVGA,		/* 800x480 */
	S5K4ECGX_CAPTURE_SVGA,		/* 800x600 */
	S5K4ECGX_CAPTURE_WSVGA,		/* 1024x600 */
	S5K4ECGX_CAPTURE_1MP,		/* 1280x960 */
	S5K4ECGX_CAPTURE_W1MP,		/* 1600x960 */
	S5K4ECGX_CAPTURE_2MP,		/* UXGA - 1600x1200 */
	S5K4ECGX_CAPTURE_W2MP,		/* 35mm Academy Offset Standard 1.66 */
					/* 2048x1232, 2.4MP */
	S5K4ECGX_CAPTURE_3MP,		/* QXGA - 2048x1536 */
	S5K4ECGX_CAPTURE_W4MP,		/* WQXGA - 2560x1536 */
	S5K4ECGX_CAPTURE_5MP,		/* 2560x1920 */
	S5K4ECGX_CAPTURE_MAX,
};

struct s5k4ecgx_framesize {
	u32 index;
	u32 width;
	u32 height;
};

static const struct s5k4ecgx_framesize s5k4ecgx_preview_framesize_list[] = {
	{ S5K4ECGX_PREVIEW_QCIF,	176, 144 },
	{ S5K4ECGX_PREVIEW_CIF,		352, 288 },
	{ S5K4ECGX_PREVIEW_VGA,		640, 480 },
	{ S5K4ECGX_PREVIEW_D1,		720, 480 },
};

static const struct s5k4ecgx_framesize s5k4ecgx_capture_framesize_list[] = {
	{ S5K4ECGX_CAPTURE_VGA,		 640,  480 },
	{ S5K4ECGX_CAPTURE_1MP,		1280,  960 },
	{ S5K4ECGX_CAPTURE_2MP,		1600, 1200 },
	{ S5K4ECGX_CAPTURE_3MP,		2048, 1536 },
	{ S5K4ECGX_CAPTURE_5MP,		2560, 1920 },
};

struct s5k4ecgx_version {
	u32 major;
	u32 minor;
};

struct s5k4ecgx_regset_table {
#ifdef CONFIG_VIDEO_S5K4ECGX_DEBUG
	const char *name;
#endif
	const struct s5k4ecgx_reg *regset;
	const u8 *data;
};

#ifdef CONFIG_VIDEO_S5K4ECGX_DEBUG
#define S5K4ECGX_REGSET_TABLE(REGSET)	\
	{						\
		.name		= #REGSET,		\
		.regset		= REGSET,		\
		.data		= REGSET##_data,	\
	}
#else
#define S5K4ECGX_REGSET_TABLE(REGSET)	\
	{						\
		.regset		= REGSET,		\
		.data		= REGSET##_data,	\
	}
#endif

#define S5K4ECGX_REGSET(IDX, REGSET)	\
	[(IDX)] = S5K4ECGX_REGSET_TABLE(REGSET)

struct s5k4ecgx_regs {
	struct s5k4ecgx_regset_table ev[EV_MAX];
	struct s5k4ecgx_regset_table metering[METERING_MAX];
	struct s5k4ecgx_regset_table iso[ISO_MAX];
	struct s5k4ecgx_regset_table effect[IMAGE_EFFECT_MAX];
	struct s5k4ecgx_regset_table white_balance[WHITE_BALANCE_MAX];
	struct s5k4ecgx_regset_table preview_size[S5K4ECGX_PREVIEW_MAX];
	struct s5k4ecgx_regset_table capture_size[S5K4ECGX_CAPTURE_MAX];
	struct s5k4ecgx_regset_table scene_mode[SCENE_MODE_MAX];
	struct s5k4ecgx_regset_table saturation[SATURATION_MAX];
	struct s5k4ecgx_regset_table contrast[CONTRAST_MAX];
	struct s5k4ecgx_regset_table sharpness[SHARPNESS_MAX];
	struct s5k4ecgx_regset_table fps[FRAME_RATE_MAX];
	struct s5k4ecgx_regset_table preview_return;
	struct s5k4ecgx_regset_table capture_start;
	struct s5k4ecgx_regset_table af_macro_mode;
	struct s5k4ecgx_regset_table af_normal_mode;
	struct s5k4ecgx_regset_table dtp_start;
	struct s5k4ecgx_regset_table dtp_stop;
	struct s5k4ecgx_regset_table init_reg;
	struct s5k4ecgx_regset_table flash_init;
	struct s5k4ecgx_regset_table reset_crop;
};

#ifdef CONFIG_VIDEO_S5K4ECGX_V_1_0
static const struct s5k4ecgx_regs regs_for_fw_version_1_0 = {
	.ev = {
		S5K4ECGX_REGSET(EV_MINUS_4, s5k4ecgx_EV_Minus_4_v1),
		S5K4ECGX_REGSET(EV_MINUS_3, s5k4ecgx_EV_Minus_3_v1),
		S5K4ECGX_REGSET(EV_MINUS_2, s5k4ecgx_EV_Minus_2_v1),
		S5K4ECGX_REGSET(EV_MINUS_1, s5k4ecgx_EV_Minus_1_v1),
		S5K4ECGX_REGSET(EV_DEFAULT, s5k4ecgx_EV_Default_v1),
		S5K4ECGX_REGSET(EV_PLUS_1, s5k4ecgx_EV_Plus_1_v1),
		S5K4ECGX_REGSET(EV_PLUS_2, s5k4ecgx_EV_Plus_2_v1),
		S5K4ECGX_REGSET(EV_PLUS_3, s5k4ecgx_EV_Plus_3_v1),
		S5K4ECGX_REGSET(EV_PLUS_4, s5k4ecgx_EV_Plus_4_v1),
	},
	.metering = {
		S5K4ECGX_REGSET(METERING_MATRIX, s5k4ecgx_Metering_Matrix_v1),
		S5K4ECGX_REGSET(METERING_CENTER, s5k4ecgx_Metering_Center_v1),
		S5K4ECGX_REGSET(METERING_SPOT, s5k4ecgx_Metering_Spot_v1),
	},
	.iso = {
		S5K4ECGX_REGSET(ISO_AUTO, s5k4ecgx_ISO_Auto_v1),
		S5K4ECGX_REGSET(ISO_50, s5k4ecgx_ISO_100_v1),	/* use 100 */
		S5K4ECGX_REGSET(ISO_100, s5k4ecgx_ISO_100_v1),
		S5K4ECGX_REGSET(ISO_200, s5k4ecgx_ISO_200_v1),
		S5K4ECGX_REGSET(ISO_400, s5k4ecgx_ISO_400_v1),
		S5K4ECGX_REGSET(ISO_800, s5k4ecgx_ISO_400_v1),	/* use 400 */
		S5K4ECGX_REGSET(ISO_1600, s5k4ecgx_ISO_400_v1),	/* use 400 */
		S5K4ECGX_REGSET(ISO_SPORTS, s5k4ecgx_ISO_Auto_v1),/* use auto */
		S5K4ECGX_REGSET(ISO_NIGHT, s5k4ecgx_ISO_Auto_v1), /* use auto */
		S5K4ECGX_REGSET(ISO_MOVIE, s5k4ecgx_ISO_Auto_v1), /* use auto */
	},
	.effect = {
		S5K4ECGX_REGSET(IMAGE_EFFECT_NONE, s5k4ecgx_Effect_Normal_v1),
		S5K4ECGX_REGSET(IMAGE_EFFECT_BNW,
				s5k4ecgx_Effect_Black_White_v1),
		S5K4ECGX_REGSET(IMAGE_EFFECT_SEPIA, s5k4ecgx_Effect_Sepia_v1),
		S5K4ECGX_REGSET(IMAGE_EFFECT_NEGATIVE,
				s5k4ecgx_Effect_Negative_v1),
	},
	.white_balance = {
		S5K4ECGX_REGSET(WHITE_BALANCE_AUTO, s5k4ecgx_WB_Auto_v1),
		S5K4ECGX_REGSET(WHITE_BALANCE_SUNNY, s5k4ecgx_WB_Sunny_v1),
		S5K4ECGX_REGSET(WHITE_BALANCE_CLOUDY, s5k4ecgx_WB_Cloudy_v1),
		S5K4ECGX_REGSET(WHITE_BALANCE_TUNGSTEN,
				s5k4ecgx_WB_Tungsten_v1),
		S5K4ECGX_REGSET(WHITE_BALANCE_FLUORESCENT,
				s5k4ecgx_WB_Fluorescent_v1),
	},
	.preview_size = {
		S5K4ECGX_REGSET(S5K4ECGX_PREVIEW_QCIF, s5k4ecgx_176_Preview_v1),
		S5K4ECGX_REGSET(S5K4ECGX_PREVIEW_CIF, s5k4ecgx_352_Preview_v1),
		S5K4ECGX_REGSET(S5K4ECGX_PREVIEW_VGA, s5k4ecgx_640_Preview_v1),
		S5K4ECGX_REGSET(S5K4ECGX_PREVIEW_D1, s5k4ecgx_720_Preview_v1),
	},
	.capture_size = {
		S5K4ECGX_REGSET(S5K4ECGX_CAPTURE_VGA, s5k4ecgx_VGA_Capture_v1),
		S5K4ECGX_REGSET(S5K4ECGX_CAPTURE_1MP, s5k4ecgx_1M_Capture_v1),
		S5K4ECGX_REGSET(S5K4ECGX_CAPTURE_2MP, s5k4ecgx_2M_Capture_v1),
		S5K4ECGX_REGSET(S5K4ECGX_CAPTURE_3MP, s5k4ecgx_3M_Capture_v1),
		S5K4ECGX_REGSET(S5K4ECGX_CAPTURE_5MP, s5k4ecgx_5M_Capture_v1),
	},
	.scene_mode = {
		S5K4ECGX_REGSET(SCENE_MODE_NONE, s5k4ecgx_Scene_Default_v1),
		S5K4ECGX_REGSET(SCENE_MODE_PORTRAIT,
				s5k4ecgx_Scene_Portrait_v1),
		S5K4ECGX_REGSET(SCENE_MODE_NIGHTSHOT,
				s5k4ecgx_Scene_Nightshot_v1),
		S5K4ECGX_REGSET(SCENE_MODE_LANDSCAPE,
				s5k4ecgx_Scene_Landscape_v1),
		S5K4ECGX_REGSET(SCENE_MODE_SPORTS, s5k4ecgx_Scene_Sports_v1),
		S5K4ECGX_REGSET(SCENE_MODE_PARTY_INDOOR,
				s5k4ecgx_Scene_Party_Indoor_v1),
		S5K4ECGX_REGSET(SCENE_MODE_BEACH_SNOW,
				s5k4ecgx_Scene_Beach_Snow_v1),
		S5K4ECGX_REGSET(SCENE_MODE_SUNSET, s5k4ecgx_Scene_Sunset_v1),
		S5K4ECGX_REGSET(SCENE_MODE_FIREWORKS,
				s5k4ecgx_Scene_Fireworks_v1),
		S5K4ECGX_REGSET(SCENE_MODE_CANDLE_LIGHT,
				s5k4ecgx_Scene_Candle_Light_v1),
	},
	.saturation = {
		S5K4ECGX_REGSET(SATURATION_MINUS_2,
				s5k4ecgx_Saturation_Minus_2_v1),
		S5K4ECGX_REGSET(SATURATION_MINUS_1,
				s5k4ecgx_Saturation_Minus_1_v1),
		S5K4ECGX_REGSET(SATURATION_DEFAULT,
				s5k4ecgx_Saturation_Default_v1),
		S5K4ECGX_REGSET(SATURATION_PLUS_1,
				s5k4ecgx_Saturation_Plus_1_v1),
		S5K4ECGX_REGSET(SATURATION_PLUS_2,
				s5k4ecgx_Saturation_Plus_2_v1),
	},
	.contrast = {
		S5K4ECGX_REGSET(CONTRAST_MINUS_2, s5k4ecgx_Contrast_Minus_2_v1),
		S5K4ECGX_REGSET(CONTRAST_MINUS_1, s5k4ecgx_Contrast_Minus_1_v1),
		S5K4ECGX_REGSET(CONTRAST_DEFAULT, s5k4ecgx_Contrast_Default_v1),
		S5K4ECGX_REGSET(CONTRAST_PLUS_1, s5k4ecgx_Contrast_Plus_1_v1),
		S5K4ECGX_REGSET(CONTRAST_PLUS_2, s5k4ecgx_Contrast_Plus_2_v1),
	},
	.sharpness = {
		S5K4ECGX_REGSET(SHARPNESS_MINUS_2,
				s5k4ecgx_Sharpness_Minus_2_v1),
		S5K4ECGX_REGSET(SHARPNESS_MINUS_1,
				s5k4ecgx_Sharpness_Minus_1_v1),
		S5K4ECGX_REGSET(SHARPNESS_DEFAULT,
				s5k4ecgx_Sharpness_Default_v1),
		S5K4ECGX_REGSET(SHARPNESS_PLUS_1, s5k4ecgx_Sharpness_Plus_1_v1),
		S5K4ECGX_REGSET(SHARPNESS_PLUS_2, s5k4ecgx_Sharpness_Plus_2_v1),
	},
	.preview_return = S5K4ECGX_REGSET_TABLE(s5k4ecgx_Preview_Return_v1),
	.capture_start = S5K4ECGX_REGSET_TABLE(s5k4ecgx_Capture_Start_v1),
	.af_macro_mode = S5K4ECGX_REGSET_TABLE(s5k4ecgx_AF_Macro_mode_v1),
	.af_normal_mode = S5K4ECGX_REGSET_TABLE(s5k4ecgx_AF_Normal_mode_v1),
	.dtp_start = S5K4ECGX_REGSET_TABLE(s5k4ecgx_DTP_init_v1),
	.dtp_stop = S5K4ECGX_REGSET_TABLE(s5k4ecgx_DTP_stop_v1),
	.init_reg = S5K4ECGX_REGSET_TABLE(s5k4ecgx_init_reg_v1),
	.flash_init = S5K4ECGX_REGSET_TABLE(s5k4ecgx_Flash_init_v1),
	.reset_crop = S5K4ECGX_REGSET_TABLE(s5k4ecgx_Reset_Crop_v1),
};
#endif

#ifdef CONFIG_VIDEO_S5K4ECGX_V_1_1
static const struct s5k4ecgx_regs regs_for_fw_version_1_1 = {
	.ev = {
		S5K4ECGX_REGSET(EV_MINUS_4, s5k4ecgx_EV_Minus_4),
		S5K4ECGX_REGSET(EV_MINUS_3, s5k4ecgx_EV_Minus_3),
		S5K4ECGX_REGSET(EV_MINUS_2, s5k4ecgx_EV_Minus_2),
		S5K4ECGX_REGSET(EV_MINUS_1, s5k4ecgx_EV_Minus_1),
		S5K4ECGX_REGSET(EV_DEFAULT, s5k4ecgx_EV_Default),
		S5K4ECGX_REGSET(EV_PLUS_1, s5k4ecgx_EV_Plus_1),
		S5K4ECGX_REGSET(EV_PLUS_2, s5k4ecgx_EV_Plus_2),
		S5K4ECGX_REGSET(EV_PLUS_3, s5k4ecgx_EV_Plus_3),
		S5K4ECGX_REGSET(EV_PLUS_4, s5k4ecgx_EV_Plus_4),
	},
	.metering = {
		S5K4ECGX_REGSET(METERING_MATRIX, s5k4ecgx_Metering_Matrix),
		S5K4ECGX_REGSET(METERING_CENTER, s5k4ecgx_Metering_Center),
		S5K4ECGX_REGSET(METERING_SPOT, s5k4ecgx_Metering_Spot),
	},
	.iso = {
		S5K4ECGX_REGSET(ISO_AUTO, s5k4ecgx_ISO_Auto),
		S5K4ECGX_REGSET(ISO_50, s5k4ecgx_ISO_100), /* map to 100 */
		S5K4ECGX_REGSET(ISO_100, s5k4ecgx_ISO_100),
		S5K4ECGX_REGSET(ISO_200, s5k4ecgx_ISO_200),
		S5K4ECGX_REGSET(ISO_400, s5k4ecgx_ISO_400),
		S5K4ECGX_REGSET(ISO_800, s5k4ecgx_ISO_400), /* map to 400 */
		S5K4ECGX_REGSET(ISO_1600, s5k4ecgx_ISO_400), /* map to 400 */
		S5K4ECGX_REGSET(ISO_SPORTS, s5k4ecgx_ISO_Auto),/* map to auto */
		S5K4ECGX_REGSET(ISO_NIGHT, s5k4ecgx_ISO_Auto), /* map to auto */
		S5K4ECGX_REGSET(ISO_MOVIE, s5k4ecgx_ISO_Auto), /* map to auto */
	},
	.effect = {
		S5K4ECGX_REGSET(IMAGE_EFFECT_NONE, s5k4ecgx_Effect_Normal),
		S5K4ECGX_REGSET(IMAGE_EFFECT_BNW, s5k4ecgx_Effect_Black_White),
		S5K4ECGX_REGSET(IMAGE_EFFECT_SEPIA, s5k4ecgx_Effect_Sepia),
		S5K4ECGX_REGSET(IMAGE_EFFECT_NEGATIVE,
				s5k4ecgx_Effect_Negative),
	},
	.white_balance = {
		S5K4ECGX_REGSET(WHITE_BALANCE_AUTO, s5k4ecgx_WB_Auto),
		S5K4ECGX_REGSET(WHITE_BALANCE_SUNNY, s5k4ecgx_WB_Sunny),
		S5K4ECGX_REGSET(WHITE_BALANCE_CLOUDY, s5k4ecgx_WB_Cloudy),
		S5K4ECGX_REGSET(WHITE_BALANCE_TUNGSTEN, s5k4ecgx_WB_Tungsten),
		S5K4ECGX_REGSET(WHITE_BALANCE_FLUORESCENT,
				s5k4ecgx_WB_Fluorescent),
	},
	.preview_size = {
		S5K4ECGX_REGSET(S5K4ECGX_PREVIEW_QCIF, s5k4ecgx_176_Preview),
		S5K4ECGX_REGSET(S5K4ECGX_PREVIEW_CIF, s5k4ecgx_352_Preview),
		S5K4ECGX_REGSET(S5K4ECGX_PREVIEW_VGA, s5k4ecgx_640_Preview),
		S5K4ECGX_REGSET(S5K4ECGX_PREVIEW_D1, s5k4ecgx_720_Preview),
	},
	.capture_size = {
		S5K4ECGX_REGSET(S5K4ECGX_CAPTURE_VGA, s5k4ecgx_VGA_Capture),
		S5K4ECGX_REGSET(S5K4ECGX_CAPTURE_1MP, s5k4ecgx_1M_Capture),
		S5K4ECGX_REGSET(S5K4ECGX_CAPTURE_2MP, s5k4ecgx_2M_Capture),
		S5K4ECGX_REGSET(S5K4ECGX_CAPTURE_3MP, s5k4ecgx_3M_Capture),
		S5K4ECGX_REGSET(S5K4ECGX_CAPTURE_5MP, s5k4ecgx_5M_Capture),
	},
	.scene_mode = {
		S5K4ECGX_REGSET(SCENE_MODE_NONE, s5k4ecgx_Scene_Default),
		S5K4ECGX_REGSET(SCENE_MODE_PORTRAIT, s5k4ecgx_Scene_Portrait),
		S5K4ECGX_REGSET(SCENE_MODE_NIGHTSHOT, s5k4ecgx_Scene_Nightshot),
		S5K4ECGX_REGSET(SCENE_MODE_LANDSCAPE, s5k4ecgx_Scene_Landscape),
		S5K4ECGX_REGSET(SCENE_MODE_SPORTS, s5k4ecgx_Scene_Sports),
		S5K4ECGX_REGSET(SCENE_MODE_PARTY_INDOOR,
				s5k4ecgx_Scene_Party_Indoor),
		S5K4ECGX_REGSET(SCENE_MODE_BEACH_SNOW,
				s5k4ecgx_Scene_Beach_Snow),
		S5K4ECGX_REGSET(SCENE_MODE_SUNSET, s5k4ecgx_Scene_Sunset),
		S5K4ECGX_REGSET(SCENE_MODE_FIREWORKS, s5k4ecgx_Scene_Fireworks),
		S5K4ECGX_REGSET(SCENE_MODE_CANDLE_LIGHT,
				s5k4ecgx_Scene_Candle_Light),
	},
	.saturation = {
		S5K4ECGX_REGSET(SATURATION_MINUS_2,
				s5k4ecgx_Saturation_Minus_2),
		S5K4ECGX_REGSET(SATURATION_MINUS_1,
				s5k4ecgx_Saturation_Minus_1),
		S5K4ECGX_REGSET(SATURATION_DEFAULT,
				s5k4ecgx_Saturation_Default),
		S5K4ECGX_REGSET(SATURATION_PLUS_1, s5k4ecgx_Saturation_Plus_1),
		S5K4ECGX_REGSET(SATURATION_PLUS_2, s5k4ecgx_Saturation_Plus_2),
	},
	.contrast = {
		S5K4ECGX_REGSET(CONTRAST_MINUS_2, s5k4ecgx_Contrast_Minus_2),
		S5K4ECGX_REGSET(CONTRAST_MINUS_1, s5k4ecgx_Contrast_Minus_1),
		S5K4ECGX_REGSET(CONTRAST_DEFAULT, s5k4ecgx_Contrast_Default),
		S5K4ECGX_REGSET(CONTRAST_PLUS_1, s5k4ecgx_Contrast_Plus_1),
		S5K4ECGX_REGSET(CONTRAST_PLUS_2, s5k4ecgx_Contrast_Plus_2),
	},
	.sharpness = {
		S5K4ECGX_REGSET(SHARPNESS_MINUS_2, s5k4ecgx_Sharpness_Minus_2),
		S5K4ECGX_REGSET(SHARPNESS_MINUS_1, s5k4ecgx_Sharpness_Minus_1),
		S5K4ECGX_REGSET(SHARPNESS_DEFAULT, s5k4ecgx_Sharpness_Default),
		S5K4ECGX_REGSET(SHARPNESS_PLUS_1, s5k4ecgx_Sharpness_Plus_1),
		S5K4ECGX_REGSET(SHARPNESS_PLUS_2, s5k4ecgx_Sharpness_Plus_2),
	},
	.fps = {
		S5K4ECGX_REGSET(FRAME_RATE_AUTO, s5k4ecgx_FPS_Auto),
		S5K4ECGX_REGSET(FRAME_RATE_7, s5k4ecgx_FPS_7),
		S5K4ECGX_REGSET(FRAME_RATE_15, s5k4ecgx_FPS_15),
		S5K4ECGX_REGSET(FRAME_RATE_30, s5k4ecgx_FPS_30),
	},
	.preview_return = S5K4ECGX_REGSET_TABLE(s5k4ecgx_Preview_Return),
	.capture_start = S5K4ECGX_REGSET_TABLE(s5k4ecgx_Capture_Start),
	.af_macro_mode = S5K4ECGX_REGSET_TABLE(s5k4ecgx_AF_Macro_mode),
	.af_normal_mode = S5K4ECGX_REGSET_TABLE(s5k4ecgx_AF_Normal_mode),
#if 0
	.single_af_start = S5K4ECGX_REGSET_TABLE(s5k4ecgx_Single_AF_Start),
	.single_af_off_1 = S5K4ECGX_REGSET_TABLE(s5k4ecgx_Single_AF_Off_1),
	.single_af_off_2 = S5K4ECGX_REGSET_TABLE(s5k4ecgx_Single_AF_Off_2),
#endif
	.dtp_start = S5K4ECGX_REGSET_TABLE(s5k4ecgx_DTP_init),
	.dtp_stop = S5K4ECGX_REGSET_TABLE(s5k4ecgx_DTP_stop),
	.init_reg = S5K4ECGX_REGSET_TABLE(s5k4ecgx_init_reg),
	.flash_init = S5K4ECGX_REGSET_TABLE(s5k4ecgx_Flash_init),
	.reset_crop = S5K4ECGX_REGSET_TABLE(s5k4ecgx_Reset_Crop),
};
#endif


struct s5k4ec {
	struct v4l2_subdev sd;
	struct media_pad pad;
	struct v4l2_pix_format pix;
	struct s5k4ecgx_version fw;
	struct v4l2_streamparm strm;
	struct mutex ctrl_lock;
	enum s5k4ecgx_oprmode oprmode;
	int preview_framesize_index;
	int capture_framesize_index;
	int freq;		/* MCLK in Hz */
	int check_dataline;
	bool initialized;
	const struct s5k4ecgx_regs *regs;
	enum s5k4ecgx_reg_type reg_type;
	u16 reg_addr_high;
	u16 reg_addr_low;
	struct clk *clk;
	struct v4l2_ctrl_handler handler;
	int (*s_power)(int enable);
};

static inline struct v4l2_subdev *ctrl_to_sd(struct v4l2_ctrl *ctrl)
{
	return &container_of(ctrl->handler, struct s5k4ec, handler)->sd;
}

static inline struct s5k4ec *to_s5k4ec(struct v4l2_subdev *sd)
{
	return container_of(sd, struct s5k4ec, sd);
}

static const struct v4l2_mbus_framefmt capture_fmts[] = {
	{
		.code		= V4L2_MBUS_FMT_FIXED,
		.colorspace	= V4L2_COLORSPACE_JPEG,
	},
};

/**
 * s5k4ecgx_i2c_read_twobyte: Read 2 bytes from sensor
 */
static int s5k4ecgx_i2c_read_twobyte(struct i2c_client *client,
						u16 subaddr, u16 *data)
{
	int err;
	unsigned char buf[2];
	struct i2c_msg msg[2];

	subaddr = swab16(subaddr);

	msg[0].addr = client->addr;
	msg[0].flags = 0;
	msg[0].len = 2;
	msg[0].buf = (u8 *)&subaddr;

	msg[1].addr = client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = 2;
	msg[1].buf = buf;

	err = i2c_transfer(client->adapter, msg, 2);
	if (unlikely(err != 2)) {
		dev_err(&client->dev,
			"%s: register read fail\n", __func__);
		return -EIO;
	}

	*data = ((buf[0] << 8) | buf[1]);

	return 0;
}

static int s5k4ecgx_i2c_write(struct i2c_client *client,
					 const u8 *data, u16 data_len)
{
	int retry_count = 5;
	struct i2c_msg msg = {client->addr, 0, data_len, (u8 *)data};
	int ret = 0;

	do {
		ret = i2c_transfer(client->adapter, &msg, 1);
		if (likely(ret == 1))
			break;
		msleep(POLL_TIME_MS);
		dev_err(&client->dev, "%s: I2C err %d, retry %d.\n",
			__func__, ret, retry_count);
		printk("%s: I2C err %d, retry %d.\n",
			__func__, ret, retry_count);
	} while (retry_count-- > 0);
	if (ret != 1) {
		dev_err(&client->dev, "%s: I2C is not working.\n", __func__);
		printk("%s: I2C is not working.\n", __func__);
		return -EIO;
	}

	return 0;
}


static int s5k4ecgx_i2c_write_block(struct v4l2_subdev *sd, u8 *buf, int size)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int err;

#ifdef CONFIG_VIDEO_S5K4ECGX_DEBUG
	if (s5k4ecgx_debug_mask & S5K4ECGX_DEBUG_I2C_BURSTS) {
		if ((buf[0] == 0x0F) && (buf[1] == 0x12))
			pr_info("%s : data[0,1] = 0x%02X%02X,"
				" total data size = %d\n",
				__func__, buf[2], buf[3], size-2);
		else
			pr_info("%s : 0x%02X%02X%02X%02X\n",
				__func__, buf[0], buf[1], buf[2], buf[3]);
	}
#else
	msleep(1);
#endif

	err = s5k4ecgx_i2c_write(client, buf, size);
	if (err)
		return err;

#ifdef CONFIG_VIDEO_S5K4ECGX_V_1_0
	{
		struct s5k4ec *state =
			container_of(sd, struct s5k4ec, sd);
		if (state->fw.minor == 0) {
			/* v1.0 sensor have problems sometimes if we write
			 * too much data too fast, so add a sleep. I've
			 * tried various combinations of size/delay. Checking
			 * for a larger size doesn't seem to work reliably, and
			 * a delay of 1ms sometimes isn't enough either.
			 */
			if (size > 16)
				msleep(2);
		}
	}
#endif
	return 0;
}

static int s5k4ecgx_i2c_write_two_word(struct i2c_client *client,
					 u16 addr, u16 w_data)
{
	u8 buf[4];

	addr = swab16(addr);
	w_data = swab16(w_data);

	memcpy(buf, &addr, 2);
	memcpy(buf + 2, &w_data, 2);

	s5k4ecgx_debug(S5K4ECGX_DEBUG_I2C, "%s : W(0x%02X%02X%02X%02X)\n",
		__func__, buf[0], buf[1], buf[2], buf[3]);

	return s5k4ecgx_i2c_write(client, buf, 4);
}

static int s5k4ecgx_burst_write_regs(struct v4l2_subdev *sd,
		struct s5k4ecgx_reg **start_reg,
		const u8 *regset_data, int *regset_data_idx)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct s5k4ec *state =
		container_of(sd, struct s5k4ec, sd);
	struct s5k4ecgx_reg *curr_reg = *start_reg;
	u16 addr_high, addr_low;

	u8 *burst_buf;
	int burst_len = 0;

	int ret = 0;

	addr_high = curr_reg->addr >> 16;
	addr_low = curr_reg->addr & 0xffff;

	if (state->reg_type != S5K4ECGX_REGTYPE_WRITE) {
		state->reg_addr_high = 0;
		state->reg_addr_low = 0;
		state->reg_type = S5K4ECGX_REGTYPE_WRITE;
	}

	if (state->reg_addr_high != addr_high) {
		s5k4ecgx_i2c_write_two_word(client, 0x0028, addr_high);
		state->reg_addr_high = addr_high;
		state->reg_addr_low = 0;
	}

	if (state->reg_addr_low != addr_low) {
		s5k4ecgx_i2c_write_two_word(client, 0x002A, addr_low);
		state->reg_addr_low = addr_low;
	}

	while (curr_reg->type != S5K4ECGX_REGTYPE_END) {
		burst_len += curr_reg->data_len;

		if (S5K4ECGX_REGTYPE_WRITE != (curr_reg + 1)->type)
			break;

		if (curr_reg->addr + 2 != (curr_reg + 1)->addr)
			break;

		curr_reg += 1;
	}

	burst_buf = vmalloc(burst_len + 2);
	if (burst_buf == NULL)
		return -ENOMEM;

	burst_buf[0] = 0x0F;
	burst_buf[1] = 0x12;
	memcpy(burst_buf + 2, regset_data + (*regset_data_idx), burst_len);

	ret = s5k4ecgx_i2c_write_block(sd, burst_buf, burst_len + 2);

	vfree(burst_buf);

#ifdef CONFIG_VIDEO_S5K4ECGX_DEBUG
	if (s5k4ecgx_debug_mask & S5K4ECGX_DEBUG_I2C_BURSTS) {
		u32 start_addr = (*start_reg)->addr;
		u32 end_addr = curr_reg->addr;
		int reg_count = (end_addr - start_addr) / 2;
		pr_debug("%s: burst write from 0x%08X to 0x%08X. %d regs.",
				"burst data starts with 0x0F12 and follow "
				"%d bytes.\n", __func__,
				start_addr, end_addr, reg_count, burst_len);
	}
#endif

	*start_reg = curr_reg;
	*regset_data_idx += burst_len;
	state->reg_addr_low = 0;

	return ret;
}

static int s5k4ecgx_request_reg_read(struct v4l2_subdev *sd, u32 addr)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct s5k4ec *state =
		container_of(sd, struct s5k4ec, sd);

	u16 addr_high, addr_low;

	addr_high = (addr >> 16) & 0xffff;
	addr_low = addr & 0xffff;


	if (state->reg_type != S5K4ECGX_REGTYPE_READ) {
		state->reg_addr_high = 0;
		state->reg_type = S5K4ECGX_REGTYPE_READ;
	}

	if (state->reg_addr_high != addr_high) {
		s5k4ecgx_i2c_write_two_word(client, 0x002C, addr_high);
		state->reg_addr_high = addr_high;
	}

	s5k4ecgx_i2c_write_two_word(client, 0x002E, addr_low);

	return 0;
}

static int s5k4ecgx_reg_read_16(struct i2c_client *client, u16 *value)
{
	return s5k4ecgx_i2c_read_twobyte(client, 0x0F12, value);
}

static int s5k4ecgx_write_regset(struct v4l2_subdev *sd,
				const struct s5k4ecgx_regset_table *table)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct s5k4ec *state =
		container_of(sd, struct s5k4ec, sd);
	struct s5k4ecgx_reg *curr_reg = (struct s5k4ecgx_reg *)table->regset;
	const u8 *regset_data = table->data;
	int data_idx = 0;
	int err = 0;

#ifdef CONFIG_VIDEO_S5K4ECGX_DEBUG
	dev_dbg(&client->dev, "%s: writing regset, %s...\n", __func__,
			table->name);
#endif

	if (curr_reg == NULL)
		return -EINVAL;

	while(curr_reg->type != S5K4ECGX_REGTYPE_END && err == 0) {
		switch(curr_reg->type) {
		case S5K4ECGX_REGTYPE_WRITE:
			err = s5k4ecgx_burst_write_regs(sd, &curr_reg,
					regset_data, &data_idx);
			break;

		case S5K4ECGX_REGTYPE_CMD:
			err = s5k4ecgx_i2c_write(client,
					regset_data + data_idx,
					curr_reg->data_len);
			data_idx += curr_reg->data_len;
			state->reg_type = S5K4ECGX_REGTYPE_CMD;
			break;

		case S5K4ECGX_REGTYPE_READ:
			err = s5k4ecgx_request_reg_read(sd, curr_reg->addr);
			data_idx += curr_reg->data_len;
			break;

		case S5K4ECGX_REGTYPE_DELAY:
			msleep(curr_reg->msec);
			data_idx += curr_reg->data_len;
			break;

		default:
			dev_err(&client->dev,
					"%s: Got unknown reg_type, %d!\n",
					__func__, curr_reg->type);
			err = -EINVAL;
			break;
		}
		curr_reg += 1;
	}

	if (unlikely(curr_reg->type != S5K4ECGX_REGTYPE_END)) {
		dev_err(&client->dev, "%s: fail to write regset!\n", __func__);
		return -EIO;
	}

	return err;
}

static int s5k4ecgx_enum_mbus_fmt(struct v4l2_subdev *sd,
				struct v4l2_subdev_fh *fh,
				struct v4l2_subdev_mbus_code_enum *code)
{
	pr_debug("%s: index = %d\n", __func__, code->index);

	if (code->index >= ARRAY_SIZE(capture_fmts))
		return -EINVAL;

	code->code = capture_fmts[code->index].code;

	return 0;
}

static int s5k4ecgx_enum_frame_size(struct v4l2_subdev *sd,
					struct v4l2_subdev_fh *fh,
					struct v4l2_subdev_frame_size_enum *fse)
{
	int i = ARRAY_SIZE(capture_fmts);

	if (fse->index > 0)
		return -EINVAL;

	while (--i)
		if (fse->code == capture_fmts[i].code)
			break;

	fse->code = capture_fmts[i].code;
	fse->min_width = 640;
	fse->max_width = 2560;
	fse->min_height = 480;
	fse->max_height = 1920;

	return 0;
}

static int s5k4ecgx_get_fmt(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh,
					struct v4l2_subdev_format *fmt)
{
	return 0;
}

static int s5k4ecgx_set_fmt(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh,
		struct v4l2_subdev_format *fmt)
{
	return 0;
}

static int s5k4ecgx_get_crop(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh,
		struct v4l2_subdev_crop *crop)
{
	return 0;
}

static int s5k4ecgx_set_crop(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh,
		struct v4l2_subdev_crop *crop)
{
	return 0;
}

static int s5k4ec_s_ctrl(struct v4l2_ctrl *ctrl)
{
	struct v4l2_subdev *sd = ctrl_to_sd(ctrl);
	struct s5k4ec *s5k4ec = to_s5k4ec(sd);
	struct s5k4ec *state = s5k4ec;
	struct sec_cam_parm *params =
		(struct sec_cam_parm *)&s5k4ec->strm.parm.raw_data;
	int err = 0;
	/* int idx; */

	mutex_lock(&s5k4ec->ctrl_lock);

	switch(ctrl->id) {
		case V4L2_CID_BRIGHTNESS:
			if(params->brightness != ctrl->val) {
				err = s5k4ecgx_write_regset(sd, state->regs->ev + EV_DEFAULT + ctrl->val);
				params->brightness = ctrl->val;
			}
			break;

		case V4L2_CID_CONTRAST:
			if(params->contrast != ctrl->val) {
				err = s5k4ecgx_write_regset(sd, state->regs->contrast + CONTRAST_DEFAULT + ctrl->val);
				params->contrast = ctrl->val;
			}
			break;
#if 0
		case V4L2_CID_AUTO_N_PRESET_WHITE_BALANCE:
			if(params->white_balance != ctrl->val) {
				if(ctrl->val == V4L2_WHITE_BALANCE_AUTO) {
					idx = WHITE_BALANCE_AUTO;
					params->white_balance = V4L2_WHITE_BALANCE_AUTO;
				}
				else if(ctrl->val == V4L2_WHITE_BALANCE_DAYLIGHT) {
					idx = WHITE_BALANCE_SUNNY;
					params->white_balance = V4L2_WHITE_BALANCE_DAYLIGHT;
				}
				else if(ctrl->val == V4L2_WHITE_BALANCE_CLOUDY) {
					idx = WHITE_BALANCE_CLOUDY;
					params->white_balance = V4L2_WHITE_BALANCE_CLOUDY;
				}
				else if(ctrl->val == V4L2_WHITE_BALANCE_INCANDESCENT) {
					idx = WHITE_BALANCE_TUNGSTEN;
					params->white_balance = V4L2_WHITE_BALANCE_INCANDESCENT;
				}
				else if(ctrl->val == V4L2_WHITE_BALANCE_FLUORESCENT) {
					idx = WHITE_BALANCE_FLUORESCENT;
					params->white_balance = V4L2_WHITE_BALANCE_FLUORESCENT;
				}
				else {
					idx = WHITE_BALANCE_AUTO;
					params->white_balance = V4L2_WHITE_BALANCE_AUTO;
				}

				err = s5k4ecgx_write_regset(sd, state->regs->white_balance + idx);
			}
			break;

		case V4L2_CID_ISO_SENSITIVITY_AUTO:
			if(params->iso != ctrl->val) {
				err = s5k4ecgx_write_regset(sd, state->regs->iso + 0);
				params->iso = ctrl->val;
			}
			break;

		case V4L2_CID_EXPOSURE_METERING:
			if(params->metering != ctrl->val) {
				if(ctrl->val == V4L2_EXPOSURE_METERING_AVERAGE) {
					idx = METERING_MATRIX;
					params->metering = V4L2_EXPOSURE_METERING_AVERAGE;
				}
				else if(ctrl->val == V4L2_EXPOSURE_METERING_CENTER_WEIGHTED) {
					idx = METERING_CENTER;
					params->metering = V4L2_EXPOSURE_METERING_CENTER_WEIGHTED;
				}
				else if(ctrl->val == V4L2_EXPOSURE_METERING_SPOT) {
					idx = METERING_SPOT;
					params->metering = V4L2_EXPOSURE_METERING_SPOT;
				}
				err = s5k4ecgx_write_regset(sd, state->regs->metering + idx);
			}
			break;

		case V4L2_CID_COLORFX:
			if(params->effects != ctrl->val) {
				if(ctrl->val == V4L2_COLORFX_NONE) {
					idx = IMAGE_EFFECT_NONE;
					params->effects = V4L2_COLORFX_NONE;
				}
				else if(ctrl->val == V4L2_COLORFX_BW) {
					idx = IMAGE_EFFECT_BNW;
					params->effects = V4L2_COLORFX_BW;
				}
				else if(ctrl->val == V4L2_COLORFX_SEPIA) {
					idx = IMAGE_EFFECT_SEPIA;
					params->effects = V4L2_COLORFX_SEPIA;
				}
				else if(ctrl->val == V4L2_COLORFX_NEGATIVE) {
					idx = IMAGE_EFFECT_NEGATIVE;
					params->effects = V4L2_COLORFX_NEGATIVE;
				}
				err = s5k4ecgx_write_regset(sd, state->regs->effect + idx);
			}
			break;

		case V4L2_CID_SCENE_MODE:
			if(params->scene_mode != ctrl->val) {
				if(ctrl->val == V4L2_SCENE_MODE_NONE) {
					idx = SCENE_MODE_NONE;
					params->scene_mode = V4L2_SCENE_MODE_NONE;
				}
				else if(ctrl->val == V4L2_SCENE_MODE_PORTRAIT) {
					idx = SCENE_MODE_PORTRAIT;
					params->scene_mode = V4L2_SCENE_MODE_PORTRAIT;
				}
				else if(ctrl->val == V4L2_SCENE_MODE_NIGHT) {
					idx = SCENE_MODE_NIGHTSHOT;
					params->scene_mode = V4L2_SCENE_MODE_NIGHT;
				}
				else if(ctrl->val == V4L2_SCENE_MODE_LANDSCAPE) {
					idx = SCENE_MODE_LANDSCAPE;
					params->scene_mode = V4L2_SCENE_MODE_LANDSCAPE;
				}
				else if(ctrl->val == V4L2_SCENE_MODE_SPORTS) {
					idx = SCENE_MODE_SPORTS;
					params->scene_mode = V4L2_SCENE_MODE_SPORTS;
				}
				else if(ctrl->val == V4L2_SCENE_MODE_PARTY_INDOOR) {
					idx = SCENE_MODE_PARTY_INDOOR;
					params->scene_mode = V4L2_SCENE_MODE_PARTY_INDOOR;
				}
				else if(ctrl->val == V4L2_SCENE_MODE_BEACH_SNOW) {
					idx = SCENE_MODE_BEACH_SNOW;
					params->scene_mode = V4L2_SCENE_MODE_BEACH_SNOW;
				}
				else if(ctrl->val == V4L2_SCENE_MODE_SUNSET) {
					idx = SCENE_MODE_SUNSET;
					params->scene_mode = V4L2_SCENE_MODE_SUNSET;
				}
				else if(ctrl->val == V4L2_SCENE_MODE_FIREWORKS) {
					idx = SCENE_MODE_FIREWORKS;
					params->scene_mode = V4L2_SCENE_MODE_FIREWORKS;
				}
				else if(ctrl->val == V4L2_SCENE_MODE_CANDLE_LIGHT) {
					idx = SCENE_MODE_CANDLE_LIGHT;
					params->scene_mode = V4L2_SCENE_MODE_CANDLE_LIGHT;
				}
				err = s5k4ecgx_write_regset(sd, state->regs->scene_mode + idx);
			}
			break;

		case V4L2_CID_SATURATION:
			if(params->saturation != ctrl->val) {
				err = s5k4ecgx_write_regset(sd, state->regs->saturation + ctrl->val + SATURATION_DEFAULT);
				params->saturation = ctrl->val;
			}
			break;

		case V4L2_CID_SHARPNESS:
			if(params->sharpness != ctrl->val) {
				err = s5k4ecgx_write_regset(sd, state->regs->sharpness + ctrl->val + SHARPNESS_DEFAULT);
				params->sharpness = ctrl->val;
			}
			break;
		case V4L2_CID_AUTO_FOCUS_RANGE:
			if(params->focus_mode != ctrl->val) {
				msleep(150);
				if(ctrl->val == V4L2_AUTO_FOCUS_RANGE_AUTO)
					err = s5k4ecgx_write_regset(sd, &state->regs->af_normal_mode);
				else if(ctrl->val == V4L2_AUTO_FOCUS_RANGE_MACRO)
					err = s5k4ecgx_write_regset(sd, &state->regs->af_macro_mode);
				params->focus_mode = ctrl->val;
			}
#endif
	}

	mutex_unlock(&s5k4ec->ctrl_lock);
	return err;
}


static const struct v4l2_ctrl_ops s5k4ec_ctrl_ops = {
	.s_ctrl = s5k4ec_s_ctrl,
};

static int s5k4ec_initialize_ctrls(struct s5k4ec *s5k4ec)
{
	const struct v4l2_ctrl_ops *ops = &s5k4ec_ctrl_ops;
	struct v4l2_ctrl_handler *hdl = &s5k4ec->handler;

	int ret = v4l2_ctrl_handler_init(hdl, 16);
	if (ret)
		return ret;

	v4l2_ctrl_new_std(hdl, ops, V4L2_CID_BRIGHTNESS, -4, 4, 1, 0);

	v4l2_ctrl_new_std(hdl, ops, V4L2_CID_CONTRAST, -2, 2, 1, 0);

	v4l2_ctrl_new_std(hdl, ops, V4L2_CID_SATURATION, -2, 2, 1, 0);

#if 0
	v4l2_ctrl_new_std_menu(hdl, ops, V4L2_CID_AUTO_N_PRESET_WHITE_BALANCE, V4L2_WHITE_BALANCE_SHADE, ~0x14E, V4L2_WHITE_BALANCE_AUTO);

	v4l2_ctrl_new_std_menu(hdl, ops, V4L2_CID_ISO_SENSITIVITY_AUTO, V4L2_ISO_SENSITIVITY_AUTO, ~0x2, V4L2_ISO_SENSITIVITY_AUTO);

	v4l2_ctrl_new_std_menu(hdl, ops, V4L2_CID_EXPOSURE_METERING, V4L2_EXPOSURE_METERING_SPOT, 0, V4L2_EXPOSURE_METERING_CENTER_WEIGHTED);

	v4l2_ctrl_new_std_menu(hdl, ops, V4L2_CID_COLORFX, V4L2_COLORFX_NEGATIVE, ~0xF, V4L2_COLORFX_NONE);

	v4l2_ctrl_new_std_menu(hdl, ops, V4L2_CID_SCENE_MODE, V4L2_SCENE_MODE_TEXT, ~0x17CD, V4L2_SCENE_MODE_NONE);

	v4l2_ctrl_new_std(hdl, ops, V4L2_CID_SATURATION, -2, 2, 1, 0);

	v4l2_ctrl_new_std(hdl, ops, V4L2_CID_SHARPNESS, -2, 2, 1, 0);

	v4l2_ctrl_new_std_menu(hdl, ops, V4L2_CID_AUTO_FOCUS_RANGE, V4L2_AUTO_FOCUS_RANGE_MACRO, ~0x5, V4L2_AUTO_FOCUS_RANGE_AUTO);
#endif
	if (hdl->error) {
		ret = hdl->error;
		v4l2_ctrl_handler_free(hdl);
		printk("\n %s function failed", __FUNCTION__);
		return ret;
	}

	s5k4ec->sd.ctrl_handler = hdl;
	return 0;
}

int s5k4ecgx_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	dev_dbg(&client->dev, "%s: stream %s\n", __func__,
			enable ? "on" : "off");

	/* do nothing here! cause of the sensor streaming on automatically
	 * after finish it's init sequence and can off by power down
	 */

	return 0;
}

/* returns the real iso currently used by sensor due to lighting
 * conditions, not the requested iso we sent using s_ctrl.
 */
static int s5k4ecgx_init_regs(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct s5k4ec *state =
		container_of(sd, struct s5k4ec, sd);
	u16 read_value = 0;

	/* we'd prefer to do this in probe, but the framework hasn't
	 * turned on the camera yet so our i2c operations would fail
	 * if we tried to do it in probe, so we have to do it here
	 * and keep track if we succeeded or not.
	 */
	s5k4ecgx_request_reg_read(sd, 0x700001A6);
	s5k4ecgx_reg_read_16(client, &read_value);

	pr_debug("%s : revision %08X\n", __func__, read_value);

#ifdef CONFIG_VIDEO_S5K4ECGX_V_1_0
	if (read_value == S5K4ECGX_VERSION_1_0) {
		state->regs = &regs_for_fw_version_1_0;
		state->initialized = true;
		return 0;
	}
#endif
#ifdef CONFIG_VIDEO_S5K4ECGX_V_1_1
	if (read_value == S5K4ECGX_VERSION_1_1) {
		state->fw.minor = 1;
		state->regs = &regs_for_fw_version_1_1;
		state->initialized = true;
		return 0;
	}
#endif

	dev_err(&client->dev, "%s: unknown fw version 0x%x\n",
		__func__, read_value);
	return -ENODEV;
}

static int s5k4ecgx_init(struct v4l2_subdev *sd, u32 val)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct s5k4ec *state =
		container_of(sd, struct s5k4ec, sd);

	dev_dbg(&client->dev, "%s: start\n", __func__);

	if (s5k4ecgx_init_regs(&state->sd) < 0)
		return -ENODEV;

	dev_dbg(&client->dev, "%s: state->check_dataline : %d\n",
		__func__, state->check_dataline);

	if (s5k4ecgx_write_regset(sd, &state->regs->init_reg) < 0)
		return -EIO;

	if (s5k4ecgx_write_regset(sd, &state->regs->flash_init) < 0)
		return -EIO;

	if (state->check_dataline
		&& s5k4ecgx_write_regset(sd, &state->regs->dtp_start) < 0)
		return -EIO;

	dev_dbg(&client->dev, "%s: end\n", __func__);

	return 0;
}

static int s5k4ec_set_power(struct v4l2_subdev *sd, int on)
{
	struct s5k4ec *state =
		container_of(sd, struct s5k4ec, sd);
	if(on == 1 ) {
		if(state->s_power)
			state->s_power(1);
		s5k4ecgx_init(sd, 0);
		v4l2_ctrl_handler_setup(sd->ctrl_handler);
	}
	else {
		if(state->s_power)
			state->s_power(0);
	}
	return 0;
}

static int s5k4ec_log_status(struct v4l2_subdev *sd)
{
	v4l2_ctrl_handler_log_status(sd->ctrl_handler, sd->name);
	return 0;
}

static const struct v4l2_subdev_core_ops s5k4ecgx_core_ops = {
	.s_power = s5k4ec_set_power,
	.log_status = s5k4ec_log_status,
};

static const struct v4l2_subdev_pad_ops s5k4ecgx_pad_ops = {
	.enum_mbus_code		= s5k4ecgx_enum_mbus_fmt,
	.enum_frame_size	= s5k4ecgx_enum_frame_size,
	.get_fmt		= s5k4ecgx_get_fmt,
	.set_fmt		= s5k4ecgx_set_fmt,
	.get_crop		= s5k4ecgx_get_crop,
	.set_crop		= s5k4ecgx_set_crop,
};

static int s5k4ec_g_frame_interval(struct v4l2_subdev *sd,
		struct v4l2_subdev_frame_interval *fi)
{
	struct s5k4ec *s5k4ec = to_s5k4ec(sd);
	struct sec_cam_parm *parms =
		(struct sec_cam_parm *)&s5k4ec->strm.parm.raw_data;

	fi->interval.denominator = parms->fps;
	fi->interval.numerator = 1;
	return 0;

}

static int s5k4ec_s_frame_interval(struct v4l2_subdev *sd,
		struct v4l2_subdev_frame_interval *fi)
{
	struct s5k4ec *s5k4ec = to_s5k4ec(sd);
	struct sec_cam_parm *parms =
		(struct sec_cam_parm *)&s5k4ec->strm.parm.raw_data;
	int ret;

	unsigned int fps = fi->interval.denominator / fi->interval.numerator;

	if(fps < 10) {
		ret = s5k4ecgx_write_regset(sd, s5k4ec->regs->fps + 1);
		parms->fps = 7;
	}
	else if(fps < 20) {
		ret = s5k4ecgx_write_regset(sd, s5k4ec->regs->fps + 2);
		parms->fps = 15;
	}
	else if(fps <= 30) {
		ret = s5k4ecgx_write_regset(sd, s5k4ec->regs->fps + 3);
		parms->fps = 30;
	}
	else {
		ret = s5k4ecgx_write_regset(sd, s5k4ec->regs->fps + 0);
		parms->fps = 15;
	}
	return ret;
}

static const struct v4l2_subdev_video_ops s5k4ecgx_video_ops = {
	.g_frame_interval = s5k4ec_g_frame_interval,
	.s_frame_interval = s5k4ec_s_frame_interval,
	.s_stream = s5k4ecgx_s_stream,
};

static const struct v4l2_subdev_ops s5k4ecgx_ops = {
	.core = &s5k4ecgx_core_ops,
	.video = &s5k4ecgx_video_ops,
	.pad = &s5k4ecgx_pad_ops,
};


/*
 * s5k4ecgx_probe
 * Fetching platform data is being done with s_config subdev call.
 * In probe routine, we just register subdev device
 */

static int s5k4ecgx_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct v4l2_subdev *sd;
	struct s5k4ec *state;
	struct s5k4ecgx_platform_data *pdata = client->dev.platform_data;
	int ret;


	state = kzalloc(sizeof(struct s5k4ec), GFP_KERNEL);
	if (state == NULL)
		return -ENOMEM;

	mutex_init(&state->ctrl_lock);

	sd = &state->sd;
	strcpy(sd->name, S5K4ECGX_DRIVER_NAME);

	/*
	 * Assign default format and resolution
	 * Use configured default information in platform data
	 * or without them, use default information in driver
	 */
	if (pdata) {
		state->pix.width = pdata->default_width;
		state->pix.height = pdata->default_height;

		if (!pdata->pixelformat)
			state->pix.pixelformat = DEFAULT_PIX_FMT;
		else
			state->pix.pixelformat = pdata->pixelformat;

		if (!pdata->freq)
			state->freq = DEFAULT_MCLK;	/* 24MHz default */
		else
			state->freq = pdata->freq;

		if(pdata)
			state->s_power = pdata->set_power;
	} else {
		state->pix.width = 640;
		state->pix.height = 480;
		state->pix.pixelformat = DEFAULT_PIX_FMT;
		state->freq = DEFAULT_MCLK;	/* 24MHz default */
	}

	/* Registering subdev */
	v4l2_i2c_subdev_init(sd, client, &s5k4ecgx_ops);

	state->pad.flags = MEDIA_PAD_FL_SOURCE;
	sd->entity.type = MEDIA_ENT_T_V4L2_SUBDEV_SENSOR;
	ret = media_entity_init(&sd->entity, 1, &state->pad, 0);
	if (ret)
		goto out_err;

	dev_dbg(&client->dev, "5MP camera S5K4ECGX loaded.\n");


	ret = s5k4ec_initialize_ctrls(state);
	if (ret)
		goto out_err;
	return 0;

out_err:
	media_entity_cleanup(&state->sd.entity);
	kfree(state);
	return ret;
}

static int s5k4ecgx_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct s5k4ec *state =
		container_of(sd, struct s5k4ec, sd);

	printk("\n At %s file %s fn %d line\n", __FILE__, __FUNCTION__, __LINE__);
	v4l2_device_unregister_subdev(sd);
	mutex_destroy(&state->ctrl_lock);
	kfree(state);

	dev_dbg(&client->dev, "Unloaded camera sensor S5K4ECGX.\n");

	return 0;
}

static const struct i2c_device_id s5k4ecgx_id[] = {
	{ S5K4ECGX_DRIVER_NAME, 0 },
	{}
};

MODULE_DEVICE_TABLE(i2c, s5k4ecgx_id);

static struct i2c_driver v4l2_i2c_driver = {
	.driver.name = S5K4ECGX_DRIVER_NAME,
	.probe = s5k4ecgx_probe,
	.remove = s5k4ecgx_remove,
	.id_table = s5k4ecgx_id,
};

static int __init v4l2_i2c_drv_init(void)
{
	return i2c_add_driver(&v4l2_i2c_driver);
}

static void __exit v4l2_i2c_drv_cleanup(void)
{
	i2c_del_driver(&v4l2_i2c_driver);
}

module_init(v4l2_i2c_drv_init);
module_exit(v4l2_i2c_drv_cleanup);

MODULE_DESCRIPTION("LSI S5K4ECGX 5MP SOC camera driver");
MODULE_AUTHOR("Seok-Young Jang <quartz.jang@samsung.com>");
MODULE_LICENSE("GPL");

