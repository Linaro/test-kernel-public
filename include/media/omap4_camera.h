/*
 * Driver header for the OMAP4 Camera Interface
 *
 * Copyright (C) 2011, Sergio Aguirre <saaguirre@ti.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _OMAP4_CAMERA_H_
#define _OMAP4_CAMERA_H_

enum omap4_camera_csiphy_laneposition {
	OMAP4_ISS_CSIPHY_LANEPOS_DISABLED,
	OMAP4_ISS_CSIPHY_LANEPOS_DXY0,
	OMAP4_ISS_CSIPHY_LANEPOS_DXY1,
	OMAP4_ISS_CSIPHY_LANEPOS_DXY2,
	OMAP4_ISS_CSIPHY_LANEPOS_DXY3,
	OMAP4_ISS_CSIPHY_LANEPOS_DXY4,
};

enum omap4_camera_csiphy_lanepolarity {
	OMAP4_ISS_CSIPHY_LANEPOL_DXPOS_DYNEG,
	OMAP4_ISS_CSIPHY_LANEPOL_DXNEG_DYPOS,
};

struct omap4_camera_csiphy_laneconfig {
	enum omap4_camera_csiphy_laneposition	pos;
	enum omap4_camera_csiphy_lanepolarity	pol;
};

struct omap4_camera_csiphy_lanesconfig {
	struct omap4_camera_csiphy_laneconfig	data[4];
	struct omap4_camera_csiphy_laneconfig	clock;
};

/*
 * struct omap4_camera_csi2_config - CSI2 Rx configuration
 * @lanes: Physical link characteristics
 * @vchannel: Virtual channel to use
 * @ddr_freq: DDR frequency in MHz coming from the sensor
 */
struct omap4_camera_csi2_config {
	struct omap4_camera_csiphy_lanesconfig	lanes;
};

struct omap4_camera_pdata {
	struct omap4_camera_csi2_config		csi2cfg;
};

#endif /* _OMAP4_CAMERA_H_ */
