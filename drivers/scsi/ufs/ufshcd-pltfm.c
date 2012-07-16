/*
 * Universal Flash Storage Host controller driver
 *
 * This code is based on drivers/scsi/ufs/ufshcd.c
 * Copyright (C) 2011-2012 Samsung India Software Operations
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * NO WARRANTY
 * THE PROGRAM IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OR
 * CONDITIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED INCLUDING, WITHOUT
 * LIMITATION, ANY WARRANTIES OR CONDITIONS OF TITLE, NON-INFRINGEMENT,
 * MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE. Each Recipient is
 * solely responsible for determining the appropriateness of using and
 * distributing the Program and assumes all risks associated with its
 * exercise of rights under this Agreement, including but not limited to
 * the risks and costs of program errors, damage to or loss of data,
 * programs or equipment, and unavailability or interruption of operations.

 * DISCLAIMER OF LIABILITY
 * NEITHER RECIPIENT NOR ANY CONTRIBUTORS SHALL HAVE ANY LIABILITY FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING WITHOUT LIMITATION LOST PROFITS), HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 * USE OR DISTRIBUTION OF THE PROGRAM OR THE EXERCISE OF ANY RIGHTS GRANTED
 * HEREUNDER, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGES

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
 * USA.
 */

#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <scsi/scsi_host.h>

#include "ufs.h"
#include "ufshci.h"

/**
 * ufshcd_pltfmset_dma_mask - Set dma mask based on the controller
 *			 addressing capability
 * @ihba: pointer to host platform data
 *
 * Returns 0 for success, non-zero for failure
 */
static int ufshcd_pltfm_set_dma_mask(struct platform_device *pdev)
{
	int err;
	u64 dma_mask;

	dma_mask = DMA_BIT_MASK(64);
	err = dma_set_coherent_mask(&pdev->dev, dma_mask);
	if (err) {
		dma_mask = DMA_BIT_MASK(32);
		err = dma_set_coherent_mask(&pdev->dev, dma_mask);
	}

	return err;
}

static int __devinit ufshcd_pltfm_probe(struct platform_device *pdev)
{
	struct ufs_hba *uninitialized_var(hba);
	struct resource	*regs;
	int err, irq;
	void __iomem *mmio_base;

	regs = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!regs)
		return -ENXIO;

	irq = platform_get_irq(pdev, 0);
	if (irq < 0)
		return irq;

	mmio_base = ioremap(regs->start, resource_size(regs));
	if (!mmio_base)
		return -ENOMEM;

	err = ufshcd_pltfm_set_dma_mask(pdev);
	if (err) {
		dev_err(&pdev->dev, "set dma mask failed\n");
		goto  err_iounmap;
	}

	err = ufshcd_drv_init(&hba, &pdev->dev, irq, mmio_base);
	if (err)
		goto err_iounmap;

	platform_set_drvdata(pdev, hba);

	/* Initialization routine */
	err = ufshcd_initialize_hba(hba);
	if (err) {
		dev_err(&pdev->dev, "Initialization failed\n");
		goto err_remove;
	}

	return err;

err_remove:
	ufshcd_drv_exit(hba);
err_iounmap:
	iounmap(hba->mmio_base);
	return err;
}

static int __devexit ufshcd_pltfm_remove(struct platform_device *pdev)
{
	struct ufs_hba *hba = platform_get_drvdata(pdev);

	ufshcd_drv_exit(hba);
	iounmap(hba->mmio_base);
	platform_set_drvdata(pdev, NULL);
	return 0;
}

#ifdef CONFIG_PM
static int ufshcd_pltfm_suspend(struct device *dev)
{
	int ret;
	struct ufs_hba *hba = dev_get_drvdata(dev);

	ret = ufshcd_suspend(hba);

	return ret;
}

static int ufshcd_pltfm_resume(struct device *dev)
{
	int ret;
	struct ufs_hba *hba = dev_get_drvdata(dev);

	ret = ufshcd_resume(hba);

	return ret;
}
#else
#define ufshcd_pltfm_suspend	NULL
#define ufshcd_pltfm_resume	NULL
#endif /* CONFIG_PM */

static SIMPLE_DEV_PM_OPS(ufshcd_pltfm_pmops,
			 ufshcd_pltfm_suspend,
			 ufshcd_pltfm_resume);

static struct platform_driver ufshcd_pltfm_driver = {
	.probe		= ufshcd_pltfm_probe,
	.remove		= __devexit_p(ufshcd_pltfm_remove),
	.driver		= {
		.name		= "ufshcd-pltfm",
		.pm		= &ufshcd_pltfm_pmops,
	},
};

module_platform_driver(ufshcd_pltfm_driver);

MODULE_DESCRIPTION("UFS Host Controller platform Interface driver");
MODULE_AUTHOR("Girish K S <ks.giri@samsung.com>");
MODULE_LICENSE("GPL");
