/*
 * ti_hdmi.h
 *
 * HDMI driver definition for TI OMAP4, DM81xx, DM38xx  Processor.
 *
 * Copyright (C) 2010-2011 Texas Instruments Incorporated - http://www.ti.com/
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _TI_HDMI_H
#define _TI_HDMI_H

struct hdmi_ip_data;

enum hdmi_pll_pwr {
	HDMI_PLLPWRCMD_ALLOFF = 0,
	HDMI_PLLPWRCMD_PLLONLY = 1,
	HDMI_PLLPWRCMD_BOTHON_ALLCLKS = 2,
	HDMI_PLLPWRCMD_BOTHON_NOPHYCLK = 3
};

enum hdmi_core_hdmi_dvi {
	HDMI_DVI = 0,
	HDMI_HDMI = 1
};

enum hdmi_clk_refsel {
	HDMI_REFSEL_PCLK = 0,
	HDMI_REFSEL_REF1 = 1,
	HDMI_REFSEL_REF2 = 2,
	HDMI_REFSEL_SYSCLK = 3
};

enum hdmi_deep_color_mode {
	HDMI_DEEP_COLOR_24BIT = 0,
	HDMI_DEEP_COLOR_30BIT = 1,
	HDMI_DEEP_COLOR_36BIT = 2,
};

enum hdmi_range {
	HDMI_LIMITED_RANGE = 0,
	HDMI_FULL_RANGE,
};

enum hdmi_s3d_frame_structure {
	HDMI_S3D_FRAME_PACKING          = 0,
	HDMI_S3D_FIELD_ALTERNATIVE      = 1,
	HDMI_S3D_LINE_ALTERNATIVE       = 2,
	HDMI_S3D_SIDE_BY_SIDE_FULL      = 3,
	HDMI_S3D_L_DEPTH                = 4,
	HDMI_S3D_L_DEPTH_GP_GP_DEPTH    = 5,
	HDMI_S3D_SIDE_BY_SIDE_HALF      = 8
};

/* Subsampling types used for Stereoscopic 3D over HDMI. Below HOR
stands for Horizontal, QUI for Quinxcunx Subsampling, O for odd fields,
E for Even fields, L for left view and R for Right view*/
enum hdmi_s3d_subsampling_type {
	HDMI_S3D_HOR_OL_OR = 0,
	HDMI_S3D_HOR_OL_ER = 1,
	HDMI_S3D_HOR_EL_OR = 2,
	HDMI_S3D_HOR_EL_ER = 3,
	HDMI_S3D_QUI_OL_OR = 4,
	HDMI_S3D_QUI_OL_ER = 5,
	HDMI_S3D_QUI_EL_OR = 6,
	HDMI_S3D_QUI_EL_ER = 7
};

struct hdmi_s3d_info {
	bool subsamp;
	enum hdmi_s3d_frame_structure  frame_struct;
	enum hdmi_s3d_subsampling_type  subsamp_pos;
	bool vsi_enabled;
};

struct hdmi_video_timings {
	u16 x_res;
	u16 y_res;
	/* Unit: KHz */
	u32 pixel_clock;
	u16 hsw;
	u16 hfp;
	u16 hbp;
	u16 vsw;
	u16 vfp;
	u16 vbp;
};

/* HDMI timing structure */
struct hdmi_timings {
	struct hdmi_video_timings timings;
	int vsync_pol;
	int hsync_pol;
};

struct hdmi_cm {
	int	code;
	int	mode;
};

struct hdmi_config {
	struct hdmi_timings timings;
	u16	interlace;
	struct hdmi_cm cm;
	bool s3d_enabled;
	struct hdmi_s3d_info s3d_info;
	enum hdmi_deep_color_mode deep_color;
	enum hdmi_range range;
	bool hdmi_phy_tx_enabled;
};

/* HDMI PLL structure */
struct hdmi_pll_info {
	u16 regn;
	u16 regm;
	u32 regmf;
	u16 regm2;
	u16 regsd;
	u16 dcofreq;
	enum hdmi_clk_refsel refsel;
};

struct hdmi_irq_vector {
	u8      pll_recal;
	u8      pll_unlock;
	u8      pll_lock;
	u8      phy_disconnect;
	u8      phy_connect;
	u8      phy_short_5v;
	u8      video_end_fr;
	u8      video_vsync;
	u8      fifo_sample_req;
	u8      fifo_overflow;
	u8      fifo_underflow;
	u8      ocp_timeout;
	u8      core;
};

struct ti_hdmi_ip_ops {

	void (*video_configure)(struct hdmi_ip_data *ip_data);

	int (*phy_enable)(struct hdmi_ip_data *ip_data);

	void (*phy_disable)(struct hdmi_ip_data *ip_data);

	int (*read_edid)(struct hdmi_ip_data *ip_data, u8 *edid, int len);

	bool (*detect)(struct hdmi_ip_data *ip_data);

	int (*pll_enable)(struct hdmi_ip_data *ip_data);

	void (*pll_disable)(struct hdmi_ip_data *ip_data);

	void (*video_enable)(struct hdmi_ip_data *ip_data, bool start);

	void (*dump_wrapper)(struct hdmi_ip_data *ip_data, struct seq_file *s);

	void (*dump_core)(struct hdmi_ip_data *ip_data, struct seq_file *s);

	void (*dump_pll)(struct hdmi_ip_data *ip_data, struct seq_file *s);

	void (*dump_phy)(struct hdmi_ip_data *ip_data, struct seq_file *s);

#if defined(CONFIG_SND_OMAP_SOC_OMAP4_HDMI) || \
	defined(CONFIG_SND_OMAP_SOC_OMAP4_HDMI_MODULE)
	void (*audio_enable)(struct hdmi_ip_data *ip_data, bool start);
#endif

	int (*irq_handler) (struct hdmi_ip_data *ip_data);

	int (*irq_process) (struct hdmi_ip_data *ip_data);

	int (*configure_range)(struct hdmi_ip_data *ip_data);

	int (*notify_hpd)(struct hdmi_ip_data *ip_data, bool hpd_state);
};

/*
 * Refer to section 8.2 in HDMI 1.3 specification for
 * details about infoframe databytes
 */
struct hdmi_core_infoframe_avi {
	/* Y0, Y1 rgb,yCbCr */
	u8	db1_format;
	/* A0  Active information Present */
	u8	db1_active_info;
	/* B0, B1 Bar info data valid */
	u8	db1_bar_info_dv;
	/* S0, S1 scan information */
	u8	db1_scan_info;
	/* C0, C1 colorimetry */
	u8	db2_colorimetry;
	/* M0, M1 Aspect ratio (4:3, 16:9) */
	u8	db2_aspect_ratio;
	/* R0...R3 Active format aspect ratio */
	u8	db2_active_fmt_ar;
	/* ITC IT content. */
	u8	db3_itc;
	/* EC0, EC1, EC2 Extended colorimetry */
	u8	db3_ec;
	/* Q1, Q0 Quantization range */
	u8	db3_q_range;
	/* SC1, SC0 Non-uniform picture scaling */
	u8	db3_nup_scaling;
	/* VIC0..6 Video format identification */
	u8	db4_videocode;
	/* PR0..PR3 Pixel repetition factor */
	u8	db5_pixel_repeat;
	/* Line number end of top bar */
	u16	db6_7_line_eoftop;
	/* Line number start of bottom bar */
	u16	db8_9_line_sofbottom;
	/* Pixel number end of left bar */
	u16	db10_11_pixel_eofleft;
	/* Pixel number start of right bar */
	u16	db12_13_pixel_sofright;
};

struct hdmi_ip_data {
	void __iomem	*base_wp;	/* HDMI wrapper */
	unsigned long	core_sys_offset;
	unsigned long	core_av_offset;
	unsigned long	pll_offset;
	unsigned long	phy_offset;
	const struct ti_hdmi_ip_ops *ops;
	struct hdmi_config cfg;
	struct hdmi_pll_info pll_data;

	/* ti_hdmi_4xxx_ip private data. These should be in a separate struct */
	int hpd_gpio;
	bool phy_tx_enabled;
	struct hdmi_core_infoframe_avi avi_cfg;
	bool has_irq;

#if defined(CONFIG_SND_OMAP_SOC_OMAP4_HDMI) || \
        defined(CONFIG_SND_OMAP_SOC_OMAP4_HDMI_MODULE)
        struct omap_hwmod *oh;
#endif
};
int ti_hdmi_4xxx_phy_enable(struct hdmi_ip_data *ip_data);
void ti_hdmi_4xxx_phy_disable(struct hdmi_ip_data *ip_data);
int ti_hdmi_4xxx_phy_poweron(struct hdmi_ip_data *ip_data);
int ti_hdmi_4xxx_read_edid(struct hdmi_ip_data *ip_data, u8 *edid, int len);
bool ti_hdmi_4xxx_detect(struct hdmi_ip_data *ip_data);
void ti_hdmi_4xxx_wp_video_start(struct hdmi_ip_data *ip_data, bool start);
int ti_hdmi_4xxx_pll_enable(struct hdmi_ip_data *ip_data);
void ti_hdmi_4xxx_pll_disable(struct hdmi_ip_data *ip_data);
int ti_hdmi_4xxx_irq_handler(struct hdmi_ip_data *ip_data);
void ti_hdmi_4xxx_basic_configure(struct hdmi_ip_data *ip_data);
void ti_hdmi_4xxx_wp_dump(struct hdmi_ip_data *ip_data, struct seq_file *s);
void ti_hdmi_4xxx_pll_dump(struct hdmi_ip_data *ip_data, struct seq_file *s);
void ti_hdmi_4xxx_core_dump(struct hdmi_ip_data *ip_data, struct seq_file *s);
void ti_hdmi_4xxx_phy_dump(struct hdmi_ip_data *ip_data, struct seq_file *s);
#if defined(CONFIG_SND_OMAP_SOC_OMAP4_HDMI) || \
	defined(CONFIG_SND_OMAP_SOC_OMAP4_HDMI_MODULE)
void ti_hdmi_4xxx_wp_audio_enable(struct hdmi_ip_data *ip_data, bool enable);
#endif
int ti_hdmi_4xxx_notify_hpd(struct hdmi_ip_data *ip_data, bool hpd_state);
void ti_hdmi_5xxx_basic_configure(struct hdmi_ip_data *ip_data);
void ti_hdmi_5xxx_core_dump(struct hdmi_ip_data *ip_data, struct seq_file *s);
int ti_hdmi_5xxx_read_edid(struct hdmi_ip_data *ip_data,
				u8 *edid, int len);
int ti_hdmi_5xxx_irq_process(struct hdmi_ip_data *ip_data);
int ti_hdmi_5xxx_configure_range(struct hdmi_ip_data *ip_data);
#endif
