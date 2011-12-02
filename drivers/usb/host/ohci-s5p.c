/*
 * SAMSUNG S5P USB HOST OHCI Controller
 *
 * Copyright (C) 2011 Samsung Electronics Co.Ltd
 * Author: Jingoo Han <jg1.han@samsung.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */

#include <linux/clk.h>
#include <linux/platform_device.h>
#include <plat/ohci.h>
#include <plat/usb-phy.h>

struct s5p_ohci_hcd {
	struct device *dev;
	struct usb_hcd *hcd;
	struct clk *clk;
};

static int ohci_s5p_start(struct usb_hcd *hcd)
{
	struct ohci_hcd *ohci = hcd_to_ohci(hcd);
	int ret;

	ohci_dbg(ohci, "ohci_s5p_start, ohci:%p", ohci);

	ret = ohci_init(ohci);
	if (ret < 0)
		return ret;

	ret = ohci_run(ohci);
	if (ret < 0) {
		err("can't start %s", hcd->self.bus_name);
		ohci_stop(hcd);
		return ret;
	}

	return 0;
}

static const struct hc_driver s5p_ohci_hc_driver = {
	.description		= hcd_name,
	.product_desc		= "S5P OHCI Host Controller",
	.hcd_priv_size		= sizeof(struct ohci_hcd),

	.irq			= ohci_irq,
	.flags			= HCD_MEMORY|HCD_USB11,

	.start			= ohci_s5p_start,
	.stop			= ohci_stop,
	.shutdown		= ohci_shutdown,

	.get_frame_number	= ohci_get_frame,

	.urb_enqueue		= ohci_urb_enqueue,
	.urb_dequeue		= ohci_urb_dequeue,
	.endpoint_disable	= ohci_endpoint_disable,

	.hub_status_data	= ohci_hub_status_data,
	.hub_control		= ohci_hub_control,
#ifdef	CONFIG_PM
	.bus_suspend		= ohci_bus_suspend,
	.bus_resume		= ohci_bus_resume,
#endif
	.start_port_reset	= ohci_start_port_reset,
};

static int __devinit s5p_ohci_probe(struct platform_device *pdev)
{
	struct s5p_ohci_platdata *pdata;
	struct s5p_ohci_hcd *s5p_ohci;
	struct usb_hcd *hcd;
	struct ohci_hcd *ohci;
	struct resource *res;
	int irq;
	int err;

	pdata = pdev->dev.platform_data;
	if (!pdata) {
		dev_err(&pdev->dev, "No platform data defined\n");
		return -EINVAL;
	}

	s5p_ohci = kzalloc(sizeof(struct s5p_ohci_hcd), GFP_KERNEL);
	if (!s5p_ohci)
		return -ENOMEM;

	s5p_ohci->dev = &pdev->dev;

	hcd = usb_create_hcd(&s5p_ohci_hc_driver, &pdev->dev,
					dev_name(&pdev->dev));
	if (!hcd) {
		dev_err(&pdev->dev, "Unable to create HCD\n");
		err = -ENOMEM;
		goto fail_hcd;
	}

	s5p_ohci->hcd = hcd;
	s5p_ohci->clk = clk_get(&pdev->dev, "usbhost");

	if (IS_ERR(s5p_ohci->clk)) {
		dev_err(&pdev->dev, "Failed to get usbhost clock\n");
		err = PTR_ERR(s5p_ohci->clk);
		goto fail_clk;
	}

	err = clk_enable(s5p_ohci->clk);
	if (err)
		goto fail_clken;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "Failed to get I/O memory\n");
		err = -ENXIO;
		goto fail_io;
	}

	hcd->rsrc_start = res->start;
	hcd->rsrc_len = resource_size(res);
	hcd->regs = ioremap(res->start, resource_size(res));
	if (!hcd->regs) {
		dev_err(&pdev->dev, "Failed to remap I/O memory\n");
		err = -ENOMEM;
		goto fail_io;
	}

	irq = platform_get_irq(pdev, 0);
	if (!irq) {
		dev_err(&pdev->dev, "Failed to get IRQ\n");
		err = -ENODEV;
		goto fail;
	}

	if (pdata->phy_init)
		pdata->phy_init(pdev, S5P_USB_PHY_HOST);

	ohci = hcd_to_ohci(hcd);
	ohci_hcd_init(ohci);

	err = usb_add_hcd(hcd, irq, IRQF_SHARED);
	if (err) {
		dev_err(&pdev->dev, "Failed to add USB HCD\n");
		goto fail;
	}

	platform_set_drvdata(pdev, s5p_ohci);

	return 0;

fail:
	iounmap(hcd->regs);
fail_io:
	clk_disable(s5p_ohci->clk);
fail_clken:
	clk_put(s5p_ohci->clk);
fail_clk:
	usb_put_hcd(hcd);
fail_hcd:
	kfree(s5p_ohci);
	return err;
}

static int __devexit s5p_ohci_remove(struct platform_device *pdev)
{
	struct s5p_ohci_platdata *pdata = pdev->dev.platform_data;
	struct s5p_ohci_hcd *s5p_ohci = platform_get_drvdata(pdev);
	struct usb_hcd *hcd = s5p_ohci->hcd;

	usb_remove_hcd(hcd);

	if (pdata && pdata->phy_exit)
		pdata->phy_exit(pdev, S5P_USB_PHY_HOST);

	iounmap(hcd->regs);

	clk_disable(s5p_ohci->clk);
	clk_put(s5p_ohci->clk);

	usb_put_hcd(hcd);
	kfree(s5p_ohci);

	return 0;
}

static void s5p_ohci_shutdown(struct platform_device *pdev)
{
	struct s5p_ohci_hcd *s5p_ohci = platform_get_drvdata(pdev);
	struct usb_hcd *hcd = s5p_ohci->hcd;

	if (hcd->driver->shutdown)
		hcd->driver->shutdown(hcd);
}

#ifdef CONFIG_PM
static int s5p_ohci_suspend(struct device *dev)
{
	struct s5p_ohci_hcd *s5p_ohci = dev_get_drvdata(dev);
	struct usb_hcd *hcd = s5p_ohci->hcd;
	struct ohci_hcd *ohci = hcd_to_ohci(hcd);
	struct platform_device *pdev = to_platform_device(dev);
	struct s5p_ohci_platdata *pdata = pdev->dev.platform_data;
	unsigned long flags;
	int rc = 0;

	/* Root hub was already suspended. Disable irq emission and
	 * mark HW unaccessible, bail out if RH has been resumed. Use
	 * the spinlock to properly synchronize with possible pending
	 * RH suspend or resume activity.
	 *
	 * This is still racy as hcd->state is manipulated outside of
	 * any locks =P But that will be a different fix.
	 */
	spin_lock_irqsave(&ohci->lock, flags);
	if (hcd->state != HC_STATE_SUSPENDED && hcd->state != HC_STATE_HALT) {
		rc = -EINVAL;
		goto fail;
	}

	clear_bit(HCD_FLAG_HW_ACCESSIBLE, &hcd->flags);

	if (pdata && pdata->phy_exit)
		pdata->phy_exit(pdev, S5P_USB_PHY_HOST);
fail:
	spin_unlock_irqrestore(&ohci->lock, flags);

	return rc;
}

static int s5p_ohci_resume(struct device *dev)
{
	struct s5p_ohci_hcd *s5p_ohci = dev_get_drvdata(dev);
	struct usb_hcd *hcd = s5p_ohci->hcd;
	struct platform_device *pdev = to_platform_device(dev);
	struct s5p_ohci_platdata *pdata = pdev->dev.platform_data;

	if (pdata && pdata->phy_init)
		pdata->phy_init(pdev, S5P_USB_PHY_HOST);

	/* Mark hardware accessible again as we are out of D3 state by now */
	set_bit(HCD_FLAG_HW_ACCESSIBLE, &hcd->flags);

	ohci_finish_controller_resume(hcd);

	return 0;
}
#else
#define s5p_ohci_suspend	NULL
#define s5p_ohci_resume		NULL
#endif

static const struct dev_pm_ops s5p_ohci_pm_ops = {
	.suspend	= s5p_ohci_suspend,
	.resume		= s5p_ohci_resume,
};

static struct platform_driver s5p_ohci_driver = {
	.probe		= s5p_ohci_probe,
	.remove		= __devexit_p(s5p_ohci_remove),
	.shutdown	= s5p_ohci_shutdown,
	.driver = {
		.name	= "s5p-ohci",
		.owner	= THIS_MODULE,
		.pm	= &s5p_ohci_pm_ops,
	}
};

MODULE_ALIAS("platform:s5p-ohci");
MODULE_AUTHOR("Jingoo Han <jg1.han@samsung.com>");
