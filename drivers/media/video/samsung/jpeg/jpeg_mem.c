/* linux/drivers/media/video/samsung/jpeg/jpeg_mem.c
 *
 * Copyright (c) 2010 Samsung Electronics Co., Ltd.
 * http://www.samsung.com/
 *
 * Managent memory of the jpeg driver for encoder/docoder.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <asm/page.h>
#include <linux/vmalloc.h>
#include <linux/mm.h>
#include <linux/dma-mapping.h>

#include "jpeg_mem.h"
#include "jpeg_core.h"

int jpeg_mem_init(struct device *dev, struct jpeg_mem *mem)
{
	int size = PAGE_ALIGN(8 * 1024 * 1024); /* hardcoded to 8 MB */
	dma_addr_t map_dma;
	void *base_vrt;

	base_vrt = dma_alloc_writecombine(dev, size,
					&map_dma, GFP_KERNEL);

	if (!base_vrt) {
		jpeg_err("%s(): No memory\n", __func__);
		return -ENOMEM;
	}

	mem->phy_base = (unsigned int) map_dma;
	mem->virt_base = (unsigned int) base_vrt;
	mem->size = size;

	return 0;
}

int jpeg_mem_free(struct device *dev, struct jpeg_mem *mem)
{
	if (mem->virt_base)
		dma_free_writecombine(dev, PAGE_ALIGN(mem->size),
			(void*) mem->virt_base, (dma_addr_t) mem->phy_base);

	return 0;
}

unsigned long jpeg_get_stream_buf(unsigned long arg)
{
	return arg + JPEG_MAIN_START;
}

unsigned long jpeg_get_frame_buf(unsigned long arg)
{
	return arg + JPEG_S_BUF_SIZE;
}

void jpeg_set_stream_buf(unsigned int *str_buf, unsigned int base)
{
	*str_buf = base;
}

void jpeg_set_frame_buf(unsigned int *fra_buf, unsigned int base)
{
	*fra_buf = base + JPEG_S_BUF_SIZE;
}

void jpeg_set_stream_base(unsigned int base)
{
	return;
}

void jpeg_set_frame_base(unsigned int base)
{
	return;
}

