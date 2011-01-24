/*
 * V4L2 Driver for OMAP4 camera host
 *
 * Copyright (C) 2011, Texas Instruments
 *
 * Author: Sergio Aguirre <saaguirre@ti.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/version.h>

#include <media/v4l2-common.h>
#include <media/v4l2-dev.h>
#include <media/videobuf2-dma-contig.h>
#include <media/soc_camera.h>
#include <media/soc_mediabus.h>
#include <media/omap4_camera.h>

#include <linux/videodev2.h>

#include "omap4_camera_regs.h"

#define OMAP4_CAM_VERSION_CODE KERNEL_VERSION(0, 0, 1)
#define OMAP4_CAM_DRV_NAME "omap4-camera"

static const char *omap4_camera_driver_description = "OMAP4_Camera";

enum omap4_camera_memresource {
	OMAP4_CAM_MEM_TOP,
	OMAP4_CAM_MEM_CSI2_A_REGS1,
	OMAP4_CAM_MEM_CAMERARX_CORE1,
	OMAP4_CAM_MEM_LAST,
};

enum omap4_camera_csi2_ctxfmt {
	ISS_CSI2_CTXFMT_YUV422_8 = 0x1E,
	ISS_CSI2_CTXFMT_RAW_8 = 0x2A,
	ISS_CSI2_CTXFMT_RAW_10 = 0x2B,
	ISS_CSI2_CTXFMT_RAW_10_EXP16 = 0xAB,
};

struct omap4_camera_iss_csi2_regbases {
	void __iomem *regs1;
};

struct omap4_camera_iss_regbases {
	void __iomem				*top;
	void __iomem				*csi2phy;
	struct omap4_camera_iss_csi2_regbases	csi2a;
};

/* per video frame buffer */
struct omap4_camera_buffer {
	struct vb2_buffer vb; /* v4l buffer must be first */
	struct list_head queue;
	enum v4l2_mbus_pixelcode code;
};

struct omap4_camera_dev {
	struct soc_camera_host		soc_host;
	/*
	 * OMAP4 is only supposed to handle one camera on its CSI2A
	 * interface. If anyone ever builds hardware to enable more than
	 * one camera, they will have to modify this driver too
	 */
	struct soc_camera_device	*icd;

	struct device			*dev;

	struct resource			*res[OMAP4_CAM_MEM_LAST];
	unsigned int			irq;
	struct omap4_camera_pdata	*pdata;
	struct v4l2_subdev_sensor_interface_parms if_parms;

	/* lock used to protect videobuf */
	spinlock_t			lock;
	struct list_head		capture;
	struct vb2_buffer		*active;
	struct vb2_alloc_ctx		*alloc_ctx;

	int				sequence;

	struct clk			*iss_fck;
	struct clk			*iss_ctrlclk;
	struct clk			*ducati_clk_mux_ck;

	struct omap4_camera_iss_regbases regs;

	u32				skip_frames;
	u32				pixcode;

	u8				streaming;
};

static struct omap4_camera_buffer *to_omap4_camera_vb(struct vb2_buffer *vb)
{
	return container_of(vb, struct omap4_camera_buffer, vb);
}

static int omap4_camera_check_frame(u32 width, u32 height)
{
	/* limit to omap4 hardware capabilities */
	return height > 8192 || width > 8192;
}

static void omap4_camera_top_reset(struct omap4_camera_dev *dev)
{
	writel(readl(dev->regs.top + ISS_HL_SYSCONFIG) |
		ISS_HL_SYSCONFIG_SOFTRESET,
		dev->regs.top + ISS_HL_SYSCONFIG);

	/* Endless loop to wait for ISS HL reset */
	for (;;) {
		mdelay(1);
		/* If ISS_HL_SYSCONFIG.SOFTRESET == 0, reset is done */
		if (!(readl(dev->regs.top + ISS_HL_SYSCONFIG) &
				ISS_HL_SYSCONFIG_SOFTRESET))
			break;
	}
}

static int omap4_camera_csiphy_init(struct omap4_camera_dev *dev)
{
	int ret = 0;
	int timeout;
	unsigned int tmpreg, ddr_freq;

	if (dev->if_parms.parms.serial.phy_rate == 0)
		return -EINVAL;

	ddr_freq = dev->if_parms.parms.serial.phy_rate / 2 / 1000000;

	/* De-assert the CSIPHY reset */
	writel(readl(dev->regs.csi2a.regs1 + CSI2_COMPLEXIO_CFG) |
		CSI2_COMPLEXIO_CFG_RESET_CTRL,
		dev->regs.csi2a.regs1 + CSI2_COMPLEXIO_CFG);

	/* CSI-PHY config */
	writel((readl(dev->regs.csi2phy + REGISTER0) &
		~(REGISTER0_THS_TERM_MASK | REGISTER0_THS_SETTLE_MASK)) |
		(((((12500 * ddr_freq + 1000000) / 1000000) - 1) & 0xFF) <<
		 REGISTER0_THS_TERM_SHIFT) |
		(((((90000 * ddr_freq + 1000000) / 1000000) + 3) & 0xFF)),
		dev->regs.csi2phy + REGISTER0);

	/* Assert the FORCERXMODE signal */
	writel((readl(dev->regs.csi2a.regs1 + CSI2_TIMING) &
		~(CSI2_TIMING_STOP_STATE_COUNTER_IO1_MASK |
		  CSI2_TIMING_STOP_STATE_X4_IO1)) |
		 CSI2_TIMING_FORCE_RX_MODE_IO1 |
		 CSI2_TIMING_STOP_STATE_X16_IO1 |
		 (0x1d6),
		dev->regs.csi2a.regs1 + CSI2_TIMING);

	/* Enable OCP and ComplexIO error IRQs */
	writel(readl(dev->regs.csi2a.regs1 + CSI2_IRQENABLE) |
		CSI2_IRQ_OCP_ERR |
		CSI2_IRQ_COMPLEXIO_ERR,
		dev->regs.csi2a.regs1 + CSI2_IRQENABLE);

	/* Enable all ComplexIO IRQs */
	writel(readl(dev->regs.csi2a.regs1 + CSI2_COMPLEXIO_IRQENABLE) |
		0xFFFFFFFF,
		dev->regs.csi2a.regs1 + CSI2_COMPLEXIO_IRQENABLE);

	/* Power up the CSIPHY */
	writel((readl(dev->regs.csi2a.regs1 + CSI2_COMPLEXIO_CFG) &
		~CSI2_COMPLEXIO_CFG_PWD_CMD_MASK) |
		CSI2_COMPLEXIO_CFG_PWD_CMD_ON,
		dev->regs.csi2a.regs1 + CSI2_COMPLEXIO_CFG);

	/* Wait for CSIPHY power transition */
	timeout = 1000;
	do {
		mdelay(1);
		tmpreg = readl(dev->regs.csi2a.regs1 + CSI2_COMPLEXIO_CFG) &
				CSI2_COMPLEXIO_CFG_PWD_STATUS_MASK;
		if (tmpreg == CSI2_COMPLEXIO_CFG_PWD_STATUS_ON)
			break;

	} while (--timeout > 0);

	if (timeout == 0) {
		dev_err(dev->dev, "CSIPHY power on transition timeout!\n");
		ret = -EBUSY;
		goto out;
	}

out:
	return ret;
}

static int omap4_camera_csi2a_reset(struct omap4_camera_dev *dev)
{
	int ret = 0;
	int timeout;

	/* Do a CSI2A RX soft reset */
	writel(readl(dev->regs.csi2a.regs1 + CSI2_SYSCONFIG) |
		CSI2_SYSCONFIG_SOFT_RESET,
		dev->regs.csi2a.regs1 + CSI2_SYSCONFIG);

	/* Wait for completion */
	timeout = 5; /* Retry 5 times as per TRM suggestion */
	do {
		mdelay(1);
		if (readl(dev->regs.csi2a.regs1 + CSI2_SYSSTATUS) &
				CSI2_SYSSTATUS_RESET_DONE)
			break;
	} while (--timeout > 0);

	if (timeout == 0) {
		dev_err(dev->dev, "CSI2 reset timeout!");
		ret = -EBUSY;
		goto out;
	}

	/* Default configs */
	/*
	 * MSTANDBY_MODE = 1 - No standby
	 * AUTO_IDLE = 0 - OCP clock is free-running
	 */
	writel((readl(dev->regs.csi2a.regs1 + CSI2_SYSCONFIG) &
		~(CSI2_SYSCONFIG_MSTANDBY_MODE_MASK |
		  CSI2_SYSCONFIG_AUTO_IDLE)) |
		CSI2_SYSCONFIG_MSTANDBY_MODE_NO,
		dev->regs.csi2a.regs1 + CSI2_SYSCONFIG);

out:
	return ret;
}

static void omap4_camera_csi2_init_ctx(struct omap4_camera_dev *dev)
{
	struct omap4_camera_csi2_config *cfg = &dev->pdata->csi2cfg;

	BUG_ON(dev->if_parms.parms.serial.lanes < 1);

	/* Lane config (pos/pol) */
	/*
	 * Clock: Position 1 (d[x/y]0)
	 * Data lane 1: Position 2 (d[x/y]1)
	 */
	writel((readl(dev->regs.csi2a.regs1 + CSI2_COMPLEXIO_CFG) &
			 ~(CSI2_COMPLEXIO_CFG_DATA4_POSITION_MASK |
			   CSI2_COMPLEXIO_CFG_DATA3_POSITION_MASK |
			   CSI2_COMPLEXIO_CFG_DATA2_POSITION_MASK |
			   CSI2_COMPLEXIO_CFG_DATA1_POSITION_MASK |
			   CSI2_COMPLEXIO_CFG_CLOCK_POSITION_MASK |
			   CSI2_COMPLEXIO_CFG_DATA4_POL |
			   CSI2_COMPLEXIO_CFG_DATA3_POL |
			   CSI2_COMPLEXIO_CFG_DATA2_POL |
			   CSI2_COMPLEXIO_CFG_DATA1_POL |
			   CSI2_COMPLEXIO_CFG_CLOCK_POL)) |
			((dev->if_parms.parms.serial.lanes > 3 ?
				cfg->lanes.data[3].pos : 0) <<
				CSI2_COMPLEXIO_CFG_DATA4_POSITION_SHIFT) |
			((dev->if_parms.parms.serial.lanes > 2 ?
				cfg->lanes.data[2].pos : 0) <<
				CSI2_COMPLEXIO_CFG_DATA3_POSITION_SHIFT) |
			((dev->if_parms.parms.serial.lanes > 1 ?
				cfg->lanes.data[1].pos : 0) <<
				CSI2_COMPLEXIO_CFG_DATA2_POSITION_SHIFT) |
			(cfg->lanes.data[0].pos <<
				CSI2_COMPLEXIO_CFG_DATA1_POSITION_SHIFT) |
			(cfg->lanes.clock.pos <<
				CSI2_COMPLEXIO_CFG_CLOCK_POSITION_SHIFT) |
			(cfg->lanes.data[3].pol *
				CSI2_COMPLEXIO_CFG_DATA4_POL) |
			(cfg->lanes.data[2].pol *
				CSI2_COMPLEXIO_CFG_DATA3_POL) |
			(cfg->lanes.data[1].pol *
				CSI2_COMPLEXIO_CFG_DATA2_POL) |
			(cfg->lanes.data[0].pol *
				CSI2_COMPLEXIO_CFG_DATA1_POL) |
			(cfg->lanes.clock.pol *
				CSI2_COMPLEXIO_CFG_CLOCK_POL),
		dev->regs.csi2a.regs1 + CSI2_COMPLEXIO_CFG);

	writel((readl(dev->regs.csi2a.regs1 + CSI2_CTX_CTRL2(0)) &
		~(CSI2_CTX_CTRL2_VIRTUAL_ID_MASK)) |
	       (dev->if_parms.parms.serial.channel <<
		CSI2_CTX_CTRL2_VIRTUAL_ID_SHIFT),
	       dev->regs.csi2a.regs1 + CSI2_CTX_CTRL2(0));

	/* Activate respective IRQs */
	/* Enable Context #0 IRQ */
	writel(readl(dev->regs.csi2a.regs1 + CSI2_IRQENABLE) |
			CSI2_IRQ_CONTEXT0,
		dev->regs.csi2a.regs1 + CSI2_IRQENABLE);

	/* Enable Context #0 Frame End IRQ */
	writel(readl(dev->regs.csi2a.regs1 + CSI2_CTX_IRQENABLE(0)) |
			CSI2_CTX_IRQ_FE,
		dev->regs.csi2a.regs1 + CSI2_CTX_IRQENABLE(0));

	/* Enable some CSI2 DMA settings */
	writel((readl(dev->regs.csi2a.regs1 + CSI2_CTRL) &
		~(CSI2_CTRL_MFLAG_LEVH_MASK | CSI2_CTRL_MFLAG_LEVL_MASK)) |
	       CSI2_CTRL_BURST_SIZE_EXPAND |
	       CSI2_CTRL_NON_POSTED_WRITE |
	       CSI2_CTRL_FRAME |
	       CSI2_CTRL_BURST_SIZE_MASK |
	       CSI2_CTRL_ENDIANNESS |
	       (2 << CSI2_CTRL_MFLAG_LEVH_SHIFT) |
	       (4 << CSI2_CTRL_MFLAG_LEVL_SHIFT),
	       dev->regs.csi2a.regs1 + CSI2_CTRL);
}

static int omap4_camera_update_buf(struct omap4_camera_dev *dev);

static irqreturn_t omap4_camera_isr(int irq, void *arg)
{
	struct omap4_camera_dev *dev = (struct omap4_camera_dev *)arg;
	unsigned int temp_intr = readl(dev->regs.top + ISS_HL_IRQSTATUS_5);

	writel(temp_intr, dev->regs.top + ISS_HL_IRQSTATUS_5);
	if (temp_intr & ISS_HL_IRQ_CSIA) {
		temp_intr = readl(dev->regs.csi2a.regs1 + CSI2_IRQSTATUS);
		writel(temp_intr, dev->regs.csi2a.regs1 + CSI2_IRQSTATUS);
		if (temp_intr & CSI2_IRQ_CONTEXT0) {
			temp_intr = readl(dev->regs.csi2a.regs1 +
					  CSI2_CTX_IRQSTATUS(0));
			writel(temp_intr,
			       dev->regs.csi2a.regs1 + CSI2_CTX_IRQSTATUS(0));
			if (temp_intr & CSI2_CTX_IRQ_FE) {
				struct vb2_buffer *vb = dev->active;
				int ret;

				dev_dbg(dev->dev, "Frame received!\n");

				if (dev->skip_frames > 0) {
					dev->skip_frames--;
					goto out;
				}

				if (!vb)
					goto out;
				spin_lock(&dev->lock);
				list_del_init(&to_omap4_camera_vb(vb)->queue);

				if (!list_empty(&dev->capture))
					dev->active = &list_entry(
						dev->capture.next,
						struct omap4_camera_buffer,
						queue)->vb;
				else
					dev->active = NULL;

				writel(readl(dev->regs.csi2a.regs1 +
					     CSI2_CTRL) &
					~CSI2_CTRL_IF_EN,
					dev->regs.csi2a.regs1 +
					CSI2_CTRL);
				ret = omap4_camera_update_buf(dev);
				do_gettimeofday(&vb->v4l2_buf.timestamp);
				if (!ret) {
					writel(readl(dev->regs.csi2a.regs1 +
						     CSI2_CTRL) |
						CSI2_CTRL_IF_EN,
						dev->regs.csi2a.regs1 +
						CSI2_CTRL);
					vb->v4l2_buf.field = 0;
					vb->v4l2_buf.sequence = dev->sequence++;
				}
				vb2_buffer_done(vb,
						ret < 0 ? VB2_BUF_STATE_ERROR :
							  VB2_BUF_STATE_DONE);

				spin_unlock(&dev->lock);
			} else if (temp_intr) {
				dev_err(dev->dev,
					"Unknown CSI2_CTX0_IRQ(0x%x)\n",
					temp_intr);
			}
		} else if (temp_intr & CSI2_IRQ_COMPLEXIO_ERR) {
			temp_intr = readl(dev->regs.csi2a.regs1 +
					  CSI2_COMPLEXIO_IRQSTATUS);
			writel(temp_intr, dev->regs.csi2a.regs1 +
			       CSI2_COMPLEXIO_IRQSTATUS);
			dev_err(dev->dev, "COMPLEXIO ERR(0x%x)\n",
				temp_intr);
		} else if (temp_intr) {
			dev_err(dev->dev, "Unknown CSI2_IRQ(0x%x)\n",
				temp_intr);
		}
	}

out:
	return IRQ_HANDLED;
}

static int omap4_camera_add_device(struct soc_camera_device *icd)
{
	struct soc_camera_host *ici = to_soc_camera_host(icd->parent);
	struct omap4_camera_dev *dev = ici->priv;

	if (dev->icd)
		return -EBUSY;

	dev->icd = icd;

	dev_dbg(icd->parent, "OMAP4 Camera driver attached to camera %d\n",
		 icd->devnum);

	return 0;
}

void omap4_camera_streamoff(struct soc_camera_device *icd);

/* Called with .video_lock held */
static void omap4_camera_remove_device(struct soc_camera_device *icd)
{
	struct soc_camera_host *ici = to_soc_camera_host(icd->parent);
	struct omap4_camera_dev *dev = ici->priv;

	BUG_ON(icd != dev->icd);

	dev_dbg(icd->parent,
		 "OMAP4 Camera driver detached from camera %d\n",
		 icd->devnum);

	if (dev->streaming)
		omap4_camera_streamoff(icd);

	dev->icd = NULL;
}

static int omap4_camera_set_fmt(struct soc_camera_device *icd,
			      struct v4l2_format *f)
{
	struct device *dev = icd->parent;
	struct v4l2_subdev *sd = soc_camera_to_subdev(icd);
	struct soc_camera_host *ici = to_soc_camera_host(icd->parent);
	struct omap4_camera_dev *omap4cam_dev = ici->priv;
	const struct soc_camera_format_xlate *xlate = NULL;
	struct v4l2_pix_format *pix = &f->fmt.pix;
	struct v4l2_mbus_framefmt mf;
	int ret;
	u32 skip_frames = 0;

	xlate = soc_camera_xlate_by_fourcc(icd, pix->pixelformat);
	if (!xlate) {
		dev_warn(dev, "Format %x not found\n", pix->pixelformat);
		return -EINVAL;
	}

	mf.width	= pix->width;
	mf.height	= pix->height;
	mf.field	= pix->field;
	mf.colorspace	= pix->colorspace;
	mf.code		= xlate->code;

	ret = v4l2_subdev_call(sd, video, s_mbus_fmt, &mf);

	if (mf.code != xlate->code)
		return -EINVAL;

	if (ret < 0) {
		dev_warn(dev, "Failed to configure for format %x\n",
			 pix->pixelformat);
	} else if (omap4_camera_check_frame(mf.width, mf.height)) {
		dev_warn(dev,
			 "Camera driver produced an unsupported frame %dx%d\n",
			 mf.width, mf.height);
		ret = -EINVAL;
	}

	if (ret < 0)
		return ret;

	ret = v4l2_subdev_call(sd, sensor, g_skip_frames, &skip_frames);
	if (ret < 0 && ret != -ENOIOCTLCMD)
		return ret;

	omap4cam_dev->skip_frames = skip_frames;
	omap4cam_dev->pixcode = mf.code;

	pix->width		= mf.width;
	pix->height		= mf.height;
	pix->field		= mf.field;
	pix->colorspace		= mf.colorspace;
	icd->current_fmt	= xlate;

	return ret;
}

static int omap4_camera_try_fmt(struct soc_camera_device *icd,
			      struct v4l2_format *f)
{
	struct v4l2_subdev *sd = soc_camera_to_subdev(icd);
	const struct soc_camera_format_xlate *xlate;
	struct v4l2_pix_format *pix = &f->fmt.pix;
	struct v4l2_mbus_framefmt mf;
	__u32 pixfmt = pix->pixelformat;
	int ret;

	xlate = soc_camera_xlate_by_fourcc(icd, pixfmt);
	if (!xlate) {
		dev_warn(icd->parent, "Format %x not found\n", pixfmt);
		return -EINVAL;
	}

	pix->bytesperline = soc_mbus_bytes_per_line(pix->width,
						    xlate->host_fmt);
	if (pix->bytesperline < 0)
		return pix->bytesperline;
	pix->sizeimage = pix->height * pix->bytesperline;

	/* limit to sensor capabilities */
	mf.width	= pix->width;
	mf.height	= pix->height;
	mf.field	= pix->field;
	mf.colorspace	= pix->colorspace;
	mf.code		= xlate->code;

	ret = v4l2_subdev_call(sd, video, try_mbus_fmt, &mf);
	if (ret < 0)
		return ret;

	pix->width	= mf.width;
	pix->height	= mf.height;
	pix->colorspace	= mf.colorspace;

	switch (mf.field) {
	case V4L2_FIELD_ANY:
	case V4L2_FIELD_NONE:
		pix->field	= V4L2_FIELD_NONE;
		break;
	default:
		dev_err(icd->parent, "Field type %d unsupported.\n",
			mf.field);
		return -EINVAL;
	}

	switch (mf.code) {
	case V4L2_MBUS_FMT_YUYV8_2X8:
	case V4L2_MBUS_FMT_UYVY8_2X8:
	case V4L2_MBUS_FMT_SBGGR10_1X10:
	case V4L2_MBUS_FMT_SBGGR10_2X8_PADLO_LE:
	case V4L2_MBUS_FMT_SBGGR8_1X8:
		/* Above formats are supported */
		break;
	default:
		dev_err(icd->parent, "Sensor format code %d unsupported.\n",
			mf.code);
		return -EINVAL;
	}

	return ret;
}

static int omap4_camera_get_parm(struct soc_camera_device *icd,
				 struct v4l2_streamparm *a)
{
	struct v4l2_subdev *sd = soc_camera_to_subdev(icd);

	return v4l2_subdev_call(sd, video, g_parm, a);
}

static int omap4_camera_set_parm(struct soc_camera_device *icd,
				 struct v4l2_streamparm *a)
{
	struct v4l2_subdev *sd = soc_camera_to_subdev(icd);

	return v4l2_subdev_call(sd, video, s_parm, a);
}

/*
 *  Videobuf operations
 */
static int omap4_videobuf_setup(struct vb2_queue *vq,
				unsigned int *count, unsigned int *num_planes,
				unsigned long sizes[], void *alloc_ctxs[])
{
	struct soc_camera_device *icd = soc_camera_from_vb2q(vq);
	struct soc_camera_host *ici = to_soc_camera_host(icd->parent);
	struct omap4_camera_dev *dev = ici->priv;
	int bytes_per_line = soc_mbus_bytes_per_line(icd->user_width,
						icd->current_fmt->host_fmt);

	if (bytes_per_line < 0)
		return bytes_per_line;

	*num_planes = 1;

	dev->sequence = 0;
	sizes[0] = bytes_per_line * icd->user_height;
	alloc_ctxs[0] = dev->alloc_ctx;

	if (!*count)
		*count = 2;

	dev_dbg(icd->parent, "count=%d, size=%lu\n", *count, sizes[0]);

	return 0;
}

static int omap4_camera_update_buf(struct omap4_camera_dev *dev)
{
	dma_addr_t phys_addr;
	dma_addr_t phys_ofst;

	if (!dev->active)
		return -ENOMEM;

	phys_addr = vb2_dma_contig_plane_paddr(dev->active, 0);

	writel(phys_addr & CSI2_CTX_PING_ADDR_MASK,
		dev->regs.csi2a.regs1 + CSI2_CTX_PING_ADDR(0));
	writel(phys_addr & CSI2_CTX_PONG_ADDR_MASK,
		dev->regs.csi2a.regs1 + CSI2_CTX_PONG_ADDR(0));

	/* REVISIT: What about custom strides? */
	phys_ofst = 0;

	writel(phys_ofst & CSI2_CTX_DAT_OFST_MASK,
		dev->regs.csi2a.regs1 + CSI2_CTX_DAT_OFST(0));

	return 0;
}
/*
 * return value doesn't reflex the success/failure to queue the new buffer,
 * but rather the status of the previous buffer.
 */
static int omap4_camera_capture(struct omap4_camera_dev *dev)
{
	int ret = 0;
	u32 fmt_reg = 0;

	if (!dev->active)
		return ret;

	/* Configure format */
	switch (dev->pixcode) {
	case V4L2_MBUS_FMT_YUYV8_2X8:
	case V4L2_MBUS_FMT_UYVY8_2X8:
		fmt_reg = ISS_CSI2_CTXFMT_YUV422_8;
		break;
	case V4L2_MBUS_FMT_SBGGR10_1X10:
		fmt_reg = ISS_CSI2_CTXFMT_RAW_10;
		break;
	case V4L2_MBUS_FMT_SBGGR10_2X8_PADLO_LE:
		fmt_reg = ISS_CSI2_CTXFMT_RAW_10_EXP16;
		break;
	case V4L2_MBUS_FMT_SBGGR8_1X8:
		fmt_reg = ISS_CSI2_CTXFMT_RAW_8;
		break;
	default:
		/* This shouldn't happen if s_fmt is correctly implemented */
		BUG_ON(1);
		break;
	}

	writel((readl(dev->regs.csi2a.regs1 + CSI2_CTX_CTRL2(0)) &
				~(CSI2_CTX_CTRL2_FORMAT_MASK)) |
			(fmt_reg << CSI2_CTX_CTRL2_FORMAT_SHIFT),
		dev->regs.csi2a.regs1 + CSI2_CTX_CTRL2(0));

	/* Enable Context #0 */
	writel(readl(dev->regs.csi2a.regs1 + CSI2_CTX_CTRL1(0)) |
			CSI2_CTX_CTRL1_CTX_EN,
		dev->regs.csi2a.regs1 + CSI2_CTX_CTRL1(0));

	/* Enable CSI2 Interface */
	writel(readl(dev->regs.csi2a.regs1 + CSI2_CTRL) |
			CSI2_CTRL_IF_EN,
		dev->regs.csi2a.regs1 + CSI2_CTRL);

	return ret;
}

#if 0 /* REVISIT: How to do this USERPTR stuff with videobuf2?? */
static dma_addr_t omap4_get_phy_from_user(struct videobuf_buffer *vb,
					  unsigned long *psize,
					  unsigned long *poffset)
{
	struct mm_struct *mm = current->mm;
	struct vm_area_struct *vma;
	unsigned long this_pfn, prev_pfn;
	unsigned long pages_done, user_address, prev_ofst = 0;
	unsigned int offset;
	dma_addr_t dma_handle = 0;
	int ret = 0, const_ofst = 0;

	offset = vb->baddr & ~PAGE_MASK;
	*psize = PAGE_ALIGN(vb->size + offset);

	down_read(&mm->mmap_sem);

	vma = find_vma(mm, vb->baddr);
	if (!vma)
		goto out_up;

	if ((vb->baddr + *psize) > vma->vm_end)
		goto out_up;

	pages_done = 0;
	prev_pfn = 0; /* kill warning */
	user_address = vb->baddr;

	while (pages_done < (*psize >> PAGE_SHIFT)) {
		ret = follow_pfn(vma, user_address, &this_pfn);
		if (ret)
			break;

		if (pages_done == 0) {
			dma_handle = (this_pfn << PAGE_SHIFT) + offset;
			const_ofst = 0;
		} else {
			*poffset = (this_pfn - prev_pfn) << PAGE_SHIFT;

			/* Make sure the offset between pages is constant */
			if (prev_ofst && (prev_ofst != *poffset)) {
				*poffset = 0;
				dma_handle = 0;
				break;
			}

			prev_ofst = *poffset;
		}

		prev_pfn = this_pfn;
		user_address += PAGE_SIZE;
		pages_done++;
	}

out_up:
	up_read(&mm->mmap_sem);
	return dma_handle;
}
#endif

static int omap4_videobuf_prepare(struct vb2_buffer *vb)
{
	struct soc_camera_device *icd = soc_camera_from_vb2q(vb->vb2_queue);
	struct omap4_camera_buffer *buf;
	int bytes_per_line = soc_mbus_bytes_per_line(icd->user_width,
						icd->current_fmt->host_fmt);
	unsigned long size;

	if (bytes_per_line < 0)
		return bytes_per_line;

	buf = to_omap4_camera_vb(vb);

	dev_dbg(icd->parent, "%s (vb=0x%p) 0x%p %lu\n", __func__,
		vb, vb2_plane_vaddr(vb, 0), vb2_get_plane_payload(vb, 0));

	/* Added list head initialization on alloc */
	WARN(!list_empty(&buf->queue), "Buffer %p on queue!\n", vb);

	BUG_ON(NULL == icd->current_fmt);

	size = icd->user_height * bytes_per_line;

	if (vb2_plane_size(vb, 0) < size) {
		dev_err(icd->parent, "Buffer too small (%lu < %lu)\n",
			vb2_plane_size(vb, 0), size);
		return -ENOBUFS;
	}

#if 0 /* REVISIT: How to do this USERPTR stuff with videobuf2?? */
	if (vb->state == VIDEOBUF_NEEDS_INIT) {
		if (vb->memory == V4L2_MEMORY_USERPTR) {
			struct soc_camera_host *ici =
					to_soc_camera_host(icd->parent);
			struct omap4_camera_dev *dev = ici->priv;
			unsigned long psize = vb->size;
			unsigned long poffset = 0;
			dma_addr_t paddr;

			paddr = omap4_get_phy_from_user(vb, &psize, &poffset);
			if (!paddr) {
				ret = -EFAULT;
				goto fail;
			}
			dev->buf_phy_addr[vb->i] = paddr;
			dev->buf_phy_dat_ofst[vb->i] = poffset;
		}

		/* Do nothing since everything is already pre-alocated */
		vb->state = VIDEOBUF_PREPARED;
	}
#endif

	vb2_set_plane_payload(vb, 0, size);

	return 0;
}

static void omap4_videobuf_queue(struct vb2_buffer *vb)
{
	struct soc_camera_device *icd = soc_camera_from_vb2q(vb->vb2_queue);
	struct soc_camera_host *ici = to_soc_camera_host(icd->parent);
	struct omap4_camera_dev *dev = ici->priv;
	struct omap4_camera_buffer *buf = to_omap4_camera_vb(vb);
	unsigned long flags;

	dev_dbg(icd->parent, "%s (vb=0x%p) 0x%p %lu\n", __func__,
		vb, vb2_plane_vaddr(vb, 0), vb2_get_plane_payload(vb, 0));

	spin_lock_irqsave(&dev->lock, flags);
	list_add_tail(&buf->queue, &dev->capture);

	if (!dev->active) {
		/*
		 * Because there were no active buffer at this moment,
		 * we are not interested in the return value of
		 * omap4_capture here.
		 */
		dev->active = vb;

		/*
		 * Failure in below functions is impossible after above
		 * conditions
		 */
		omap4_camera_update_buf(dev);
		omap4_camera_capture(dev);
	}

	spin_unlock_irqrestore(&dev->lock, flags);
}

static void omap4_videobuf_release(struct vb2_buffer *vb)
{
	struct soc_camera_device *icd = soc_camera_from_vb2q(vb->vb2_queue);
	struct soc_camera_host *ici = to_soc_camera_host(icd->parent);
	struct omap4_camera_buffer *buf = to_omap4_camera_vb(vb);
	struct omap4_camera_dev *dev = ici->priv;
	unsigned long flags;

	spin_lock_irqsave(&dev->lock, flags);

	/* Doesn't hurt also if the list is empty */
	list_del_init(&buf->queue);

	spin_unlock_irqrestore(&dev->lock, flags);
}

static int omap4_videobuf_init(struct vb2_buffer *vb)
{
	/* This is for locking debugging only */
	INIT_LIST_HEAD(&to_omap4_camera_vb(vb)->queue);
	return 0;
}

static struct vb2_ops omap4_videobuf_ops = {
	.queue_setup	= omap4_videobuf_setup,
	.buf_prepare	= omap4_videobuf_prepare,
	.buf_queue	= omap4_videobuf_queue,
	.buf_cleanup	= omap4_videobuf_release,
	.buf_init	= omap4_videobuf_init,
	.wait_prepare	= soc_camera_unlock,
	.wait_finish	= soc_camera_lock,
};

static int omap4_camera_init_videobuf(struct vb2_queue *q,
				       struct soc_camera_device *icd)
{
	q->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	q->io_modes = VB2_MMAP;
	q->drv_priv = icd;
	q->ops = &omap4_videobuf_ops;
	q->mem_ops = &vb2_dma_contig_memops;
	q->buf_struct_size = sizeof(struct omap4_camera_buffer);

	return vb2_queue_init(q);
}

static unsigned int omap4_camera_poll(struct file *file, poll_table *pt)
{
	struct soc_camera_device *icd = file->private_data;

	return vb2_poll(&icd->vb2_vidq, file, pt);
}

static int omap4_camera_querycap(struct soc_camera_host *ici,
				struct v4l2_capability *cap)
{
	strlcpy(cap->card, omap4_camera_driver_description, sizeof(cap->card));
	cap->version = OMAP4_CAM_VERSION_CODE;
	cap->capabilities = V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_STREAMING;

	return 0;
}

static int omap4_camera_set_bus_param(struct soc_camera_device *icd,
					__u32 pixfmt)
{
	/* TODO: This basically queries the sensor bus parameters,
		and finds the compatible configuration with the
		one set from the platform_data. Add this here. */

	return 0;
}

void omap4_camera_streamon(struct soc_camera_device *icd)
{
	struct soc_camera_host *ici = to_soc_camera_host(icd->parent);
	struct v4l2_subdev *sd = soc_camera_to_subdev(icd);
	struct omap4_camera_dev *dev = ici->priv;
	struct v4l2_subdev_sensor_interface_parms if_parms;
	int ret = 0;

	ret = v4l2_subdev_call(sd, sensor, g_interface_parms, &if_parms);
	if (ret < 0) {
		dev_err(icd->parent, "Error on g_interface_params (%d)\n",
			 ret);
		return;
	}

	dev->if_parms = if_parms;

	clk_enable(dev->iss_ctrlclk);
	clk_enable(dev->iss_fck);

	omap4_camera_top_reset(dev);

	writel(readl(dev->regs.top + ISS_CLKCTRL) |
		ISS_CLKCTRL_CSI2_A | ISS_CLKCTRL_ISP,
		dev->regs.top + ISS_CLKCTRL);

	/* Wait for HW assertion */
	for (;;) {
		mdelay(1);
		if ((readl(dev->regs.top + ISS_CLKSTAT) &
		     (ISS_CLKCTRL_CSI2_A | ISS_CLKCTRL_ISP)) ==
		    (ISS_CLKCTRL_CSI2_A | ISS_CLKCTRL_ISP))
			break;
	}

	/* Enable HL interrupts */
	writel(ISS_HL_IRQ_CSIA | ISS_HL_IRQ_BTE | ISS_HL_IRQ_CBUFF,
		dev->regs.top + ISS_HL_IRQENABLE_5_SET);

	ret = omap4_camera_csi2a_reset(dev);
	if (ret)
		return;

	ret = omap4_camera_csiphy_init(dev);
	if (ret)
		return;

	omap4_camera_csi2_init_ctx(dev);

	dev->streaming = 1;
}

void omap4_camera_streamoff(struct soc_camera_device *icd)
{
	struct soc_camera_host *ici = to_soc_camera_host(icd->parent);
	struct omap4_camera_dev *dev = ici->priv;

	/* Disable CSI2 Interface */
	writel(readl(dev->regs.csi2a.regs1 + CSI2_CTRL) &
			~CSI2_CTRL_IF_EN,
		dev->regs.csi2a.regs1 + CSI2_CTRL);

	dev->active = NULL;

	/* Disable HL interrupts */
	writel(ISS_HL_IRQ_CSIA | ISS_HL_IRQ_BTE | ISS_HL_IRQ_CBUFF,
		dev->regs.top + ISS_HL_IRQENABLE_5_CLR);

	/* Disable clocks */
	writel(readl(dev->regs.top + ISS_CLKCTRL) &
		~(ISS_CLKCTRL_CSI2_A | ISS_CLKCTRL_ISP),
		dev->regs.top + ISS_CLKCTRL);

	clk_disable(dev->iss_ctrlclk);
	clk_disable(dev->iss_fck);
	dev->streaming = 0;
}

static struct soc_camera_host_ops omap4_soc_camera_host_ops = {
	.owner		= THIS_MODULE,
	.add		= omap4_camera_add_device,
	.remove		= omap4_camera_remove_device,
	.set_fmt	= omap4_camera_set_fmt,
	.try_fmt	= omap4_camera_try_fmt,
	.get_parm	= omap4_camera_get_parm,
	.set_parm	= omap4_camera_set_parm,
	.init_videobuf2	= omap4_camera_init_videobuf,
	.poll		= omap4_camera_poll,
	.querycap	= omap4_camera_querycap,
	.set_bus_param	= omap4_camera_set_bus_param,
	.streamon	= omap4_camera_streamon,
	.streamoff	= omap4_camera_streamoff,
};

static int __devinit omap4_camera_probe(struct platform_device *pdev)
{
	struct omap4_camera_dev *omap4cam_dev;
	int irq;
	int err = 0;
	struct resource *res[OMAP4_CAM_MEM_LAST];
	int i;

	if (!pdev->dev.platform_data) {
		dev_err(&pdev->dev,
			"Platform data not available. Please declare it.\n");
		err = -ENODEV;
		goto exit;
	}

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		err = -ENODEV;
		goto exit;
	}

	for (i = 0; i < OMAP4_CAM_MEM_LAST; i++) {
		res[i] = platform_get_resource(pdev, IORESOURCE_MEM, i);
		if (!res[i]) {
			err = -ENODEV;
			goto exit;
		}

		if (!request_mem_region(res[i]->start, resource_size(res[i]),
					OMAP4_CAM_DRV_NAME)) {
			err = -EBUSY;
			goto exit;
		}
	}

	omap4cam_dev = kzalloc(sizeof(*omap4cam_dev), GFP_KERNEL);
	if (!omap4cam_dev) {
		dev_err(&pdev->dev, "Could not allocate omap4cam_dev\n");
		err = -ENOMEM;
		goto exit;
	}

	INIT_LIST_HEAD(&omap4cam_dev->capture);
	spin_lock_init(&omap4cam_dev->lock);

	for (i = 0; i < OMAP4_CAM_MEM_LAST; i++)
		omap4cam_dev->res[i] = res[i];
	omap4cam_dev->irq = irq;
	omap4cam_dev->pdata = pdev->dev.platform_data;

	omap4cam_dev->regs.top = ioremap(res[OMAP4_CAM_MEM_TOP]->start,
					resource_size(res[OMAP4_CAM_MEM_TOP]));
	if (!omap4cam_dev->regs.top) {
		dev_err(&pdev->dev,
			"Unable to mmap TOP register region\n");
		goto exit_kfree;
	}

	omap4cam_dev->regs.csi2a.regs1 = ioremap(
				res[OMAP4_CAM_MEM_CSI2_A_REGS1]->start,
				resource_size(res[OMAP4_CAM_MEM_CSI2_A_REGS1]));
	if (!omap4cam_dev->regs.csi2a.regs1) {
		dev_err(&pdev->dev,
			"Unable to mmap CSI2_A_REGS1 register region\n");
		goto exit_mmap1;
	}

	omap4cam_dev->regs.csi2phy = ioremap(
			res[OMAP4_CAM_MEM_CAMERARX_CORE1]->start,
			resource_size(res[OMAP4_CAM_MEM_CAMERARX_CORE1]));
	if (!omap4cam_dev->regs.csi2phy) {
		dev_err(&pdev->dev,
			"Unable to mmap CAMERARX_CORE1 register region\n");
		goto exit_mmap2;
	}

	omap4cam_dev->iss_fck = clk_get(&pdev->dev, "iss_fck");
	if (IS_ERR(omap4cam_dev->iss_fck)) {
		dev_err(&pdev->dev, "Unable to get iss_fck clock info\n");
		err = -ENODEV;
		goto exit_mmap3;
	}

	omap4cam_dev->iss_ctrlclk = clk_get(&pdev->dev, "iss_ctrlclk");
	if (IS_ERR(omap4cam_dev->iss_ctrlclk)) {
		dev_err(&pdev->dev, "Unable to get iss_ctrlclk clock info\n");
		err = -ENODEV;
		goto exit_iss_fck;
	}

	omap4cam_dev->ducati_clk_mux_ck = clk_get(&pdev->dev,
						  "ducati_clk_mux_ck");
	if (IS_ERR(omap4cam_dev->ducati_clk_mux_ck)) {
		dev_err(&pdev->dev,
			"Unable to get ducati_clk_mux_ck clock info\n");
		err = -ENODEV;
		goto exit_iss_ctrlclk;
	}

	/* Register IRQ */
	err = request_irq(omap4cam_dev->irq, omap4_camera_isr, 0,
			OMAP4_CAM_DRV_NAME, omap4cam_dev);
	if (err) {
		dev_err(&pdev->dev,
			"could not install interrupt service routine\n");
		goto exit_ducati_clk_mux_ck;
	}

	omap4cam_dev->dev = &pdev->dev;
	omap4cam_dev->dev->coherent_dma_mask = DMA_BIT_MASK(32);

	omap4cam_dev->soc_host.drv_name	= OMAP4_CAM_DRV_NAME;
	omap4cam_dev->soc_host.ops		= &omap4_soc_camera_host_ops;
	omap4cam_dev->soc_host.priv		= omap4cam_dev;
	omap4cam_dev->soc_host.v4l2_dev.dev	= &pdev->dev;
	omap4cam_dev->soc_host.nr		= pdev->id;

	omap4cam_dev->alloc_ctx = vb2_dma_contig_init_ctx(&pdev->dev);
	if (IS_ERR(omap4cam_dev->alloc_ctx)) {
		err = PTR_ERR(omap4cam_dev->alloc_ctx);
		goto exit_free_irq;
	}

	err = soc_camera_host_register(&omap4cam_dev->soc_host);
	if (err) {
		dev_err(&pdev->dev, "SoC camera registration error\n");
		goto exit_free_ctx;
	}

	dev_info(&pdev->dev, "OMAP4 Camera driver loaded\n");
	return 0;

exit_free_ctx:
	vb2_dma_contig_cleanup_ctx(omap4cam_dev->alloc_ctx);
exit_free_irq:
	free_irq(omap4cam_dev->irq, omap4cam_dev);
exit_ducati_clk_mux_ck:
	clk_put(omap4cam_dev->ducati_clk_mux_ck);
exit_iss_ctrlclk:
	clk_put(omap4cam_dev->iss_ctrlclk);
exit_iss_fck:
	clk_put(omap4cam_dev->iss_fck);
exit_mmap3:
	iounmap(omap4cam_dev->regs.csi2phy);
exit_mmap2:
	iounmap(omap4cam_dev->regs.csi2a.regs1);
exit_mmap1:
	iounmap(omap4cam_dev->regs.top);
exit_kfree:
	for (i = 0; i < OMAP4_CAM_MEM_LAST; i++) {
		if (omap4cam_dev->res[i]) {
			release_mem_region(omap4cam_dev->res[i]->start,
					resource_size(omap4cam_dev->res[i]));
		}
	}

	kfree(omap4cam_dev);
exit:
	return err;
}

static int __devexit omap4_camera_remove(struct platform_device *pdev)
{
	struct soc_camera_host *soc_host = to_soc_camera_host(&pdev->dev);
	struct omap4_camera_dev *omap4cam_dev = container_of(soc_host,
					struct omap4_camera_dev, soc_host);
	int i;

	soc_camera_host_unregister(soc_host);

	vb2_dma_contig_cleanup_ctx(omap4cam_dev->alloc_ctx);
	free_irq(omap4cam_dev->irq, omap4cam_dev);

	clk_put(omap4cam_dev->ducati_clk_mux_ck);
	clk_put(omap4cam_dev->iss_ctrlclk);
	clk_put(omap4cam_dev->iss_fck);

	iounmap(omap4cam_dev->regs.csi2phy);
	iounmap(omap4cam_dev->regs.csi2a.regs1);
	iounmap(omap4cam_dev->regs.top);

	for (i = 0; i < OMAP4_CAM_MEM_LAST; i++) {
		if (omap4cam_dev->res[i]) {
			release_mem_region(omap4cam_dev->res[i]->start,
					resource_size(omap4cam_dev->res[i]));
		}
	}

	kfree(omap4cam_dev);

	dev_info(&pdev->dev, "OMAP4 Camera driver unloaded\n");

	return 0;
}

static struct platform_driver omap4_camera_driver = {
	.driver = {
		.name	= OMAP4_CAM_DRV_NAME,
	},
	.probe		= omap4_camera_probe,
	.remove		= __devexit_p(omap4_camera_remove),
};

static int __init omap4_camera_init(void)
{
	return platform_driver_register(&omap4_camera_driver);
}

static void __exit omap4_camera_exit(void)
{
	platform_driver_unregister(&omap4_camera_driver);
}

/*
 * FIXME: Had to make it late_initcall. Strangely while being module_init,
 * The I2C communication was failing in the sensor, because no XCLK was
 * provided.
 */
late_initcall(omap4_camera_init);
module_exit(omap4_camera_exit);

MODULE_DESCRIPTION("OMAP4 SoC Camera Host driver");
MODULE_AUTHOR("Sergio Aguirre <saaguirre@ti.com>");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:" OMAP4_CAM_DRV_NAME);
