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
#include <linux/pci.h>
#include <scsi/scsi_host.h>

#include "ufs.h"
#include "ufshci.h"

/**
 * ufshcd_pci_set_dma_mask - Set dma mask based on the controller
 *			 addressing capability
 * @ihba: pointer to host platform data
 *
 * Returns 0 for success, non-zero for failure
 */
static int ufshcd_pci_set_dma_mask(struct pci_dev *pdev)
{
	int err;
	u64 dma_mask;

	/*
	 * If controller supports 64 bit addressing mode, then set the DMA
	 * mask to 64-bit, else set the DMA mask to 32-bit
	 */
	dma_mask = DMA_BIT_MASK(64);
	err = pci_set_dma_mask(pdev, dma_mask);
	if (err) {
		dma_mask = DMA_BIT_MASK(32);
		err = pci_set_dma_mask(pdev, dma_mask);
	}

	if (err)
		return err;

	err = pci_set_consistent_dma_mask(pdev, dma_mask);

	return err;
}

static int __devinit ufshcd_pci_probe(struct pci_dev *pdev,
				  const struct pci_device_id *entries)
{
	struct ufs_hba *uninitialized_var(hba);
	int err;
	void __iomem *mmio_base;

	err = pci_enable_device(pdev);
	if (err) {
		dev_err(&pdev->dev, "pci_enable_device failed\n");
		goto err_return;
	}

	pci_set_master(pdev);

	err = pci_request_regions(pdev, "ufshcd-pci");
	if (err < 0) {
		dev_err(&pdev->dev, "request regions failed\n");
		goto err_disable;
	}

	mmio_base = pci_ioremap_bar(pdev, 0);
	if (!mmio_base) {
		dev_err(&pdev->dev, "memory map failed\n");
		err = -ENOMEM;
		goto err_release;
	}

	err = ufshcd_pci_set_dma_mask(pdev);
	if (err) {
		dev_err(&pdev->dev, "set dma mask failed\n");
		goto err_iounmap;
	}

	err = ufshcd_drv_init(&hba, &pdev->dev, pdev->irq, mmio_base);
	if (err)
		goto err_iounmap;

	pci_set_drvdata(pdev, hba);

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
err_release:
	pci_release_regions(pdev);
err_disable:
	pci_clear_master(pdev);
	pci_disable_device(pdev);
err_return:
	return err;
}

static void __devexit ufshcd_pci_remove(struct pci_dev *pdev)
{
	struct ufs_hba *hba = pci_get_drvdata(pdev);

	ufshcd_drv_exit(hba);
	iounmap(hba->mmio_base);
	pci_set_drvdata(pdev, NULL);
	pci_clear_master(pdev);
	pci_release_regions(pdev);
	pci_disable_device(pdev);
}

#ifdef CONFIG_PM
static int ufshcd_pci_suspend(struct device *dev)
{
	int ret;
	struct pci_dev *pdev = to_pci_dev(dev);
	struct ufs_hba *hba = pci_get_drvdata(pdev);

	ret = ufshcd_suspend(hba);
	return ret;
}

static int ufshcd_pci_resume(struct device *dev)
{
	int ret;
	struct pci_dev *pdev = to_pci_dev(dev);
	struct ufs_hba *hba = pci_get_drvdata(pdev);

	ret = ufshcd_resume(hba);
	return ret;
}
#else
#define ufshcd_pci_suspend	NULL
#define ufshcd_pci_resume	NULL
#endif /* CONFIG_PM */

static SIMPLE_DEV_PM_OPS(ufshcd_pci_pmops,
			 ufshcd_pci_suspend,
			 ufshcd_pci_resume);

static DEFINE_PCI_DEVICE_TABLE(ufshcd_pci_id) = {
	{ PCI_VENDOR_ID_SAMSUNG, 0xC00C, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0 },
	{}
};
MODULE_DEVICE_TABLE(pci, ufshcd_pci_id);

static struct pci_driver ufshcd_pci_driver = {
	.name		= "ufshcd-pci",
	.id_table	= ufshcd_pci_id,
	.probe		= ufshcd_pci_probe,
	.remove		= __devexit_p(ufshcd_pci_remove),
	.driver		=	{
		.pm =   &ufshcd_pci_pmops
	},
};

module_pci_driver(ufshcd_pci_driver);

MODULE_DESCRIPTION("UFS Host Controller PCI Interface driver");
MODULE_AUTHOR("Girish K S <ks.giri@samsung.com>");
MODULE_LICENSE("GPL");
