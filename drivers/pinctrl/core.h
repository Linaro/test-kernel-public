/*
 * Core private header for the pin control subsystem
 *
 * Copyright (C) 2011 ST-Ericsson SA
 * Written on behalf of Linaro for ST-Ericsson
 *
 * Author: Linus Walleij <linus.walleij@linaro.org>
 *
 * License terms: GNU General Public License (GPL) version 2
 */

#include <linux/pinctrl/pinconf.h>

struct pinctrl_gpio_range;
struct pinmux_group;

/**
 * struct pinctrl_dev - pin control class device
 * @node: node to include this pin controller in the global pin controller list
 * @desc: the pin controller descriptor supplied when initializing this pin
 *	controller
 * @pin_desc_tree: each pin descriptor for this pin controller is stored in
 *	this radix tree
 * @gpio_ranges: a list of GPIO ranges that is handled by this pin controller,
 *	ranges are added to this list at runtime
 * @gpio_ranges_lock: lock for the GPIO ranges list
 * @dev: the device entry for this pin controller
 * @owner: module providing the pin controller, used for refcounting
 * @driver_data: driver data for drivers registering to the pin controller
 *	subsystem
 * @p: result of pinctrl_get() for this device
 * @device_root: debugfs root for this device
 */
struct pinctrl_dev {
	struct list_head node;
	struct pinctrl_desc *desc;
	struct radix_tree_root pin_desc_tree;
	struct list_head gpio_ranges;
	struct mutex gpio_ranges_lock;
	struct device *dev;
	struct module *owner;
	void *driver_data;
	struct pinctrl *p;
#ifdef CONFIG_DEBUG_FS
	struct dentry *device_root;
#endif
};

/**
 * struct pinctrl - per-device pin control state holder
 * @node: global list node
 * @dev: the device using this pin control handle
 * @usecount: the number of active users of this pin controller setting, used
 *	to keep track of nested use cases
 * @pctldev: pin control device handling this pin control handle
 * @mutex: a lock for the pin control state holder
 * @groups: the group selectors for the pinmux device and
 *	selector combination handling this pinmux, this is a list that
 *	will be traversed on all pinmux operations such as
 *	get/put/enable/disable
 */
struct pinctrl {
	struct list_head node;
	struct device *dev;
	unsigned usecount;
	struct pinctrl_dev *pctldev;
	struct mutex mutex;
#ifdef CONFIG_PINMUX
	struct list_head groups;
#endif
};

/**
 * struct pin_desc - pin descriptor for each physical pin in the arch
 * @pctldev: corresponding pin control device
 * @name: a name for the pin, e.g. the name of the pin/pad/finger on a
 *	datasheet or such
 * @dynamic_name: if the name of this pin was dynamically allocated
 * @lock: a lock to protect the descriptor structure
 * @owner: the device holding this pin or NULL of no device has claimed it
 * @grp: the pinmux group used for this pin if it is used by a pin group
 *	or NULL
 */
struct pin_desc {
	struct pinctrl_dev *pctldev;
	const char *name;
	bool dynamic_name;
	spinlock_t lock;
	/* These fields only added when supporting pinmux drivers */
#ifdef CONFIG_PINMUX
	const char *owner;
	struct pinmux_group *grp;
#endif
};

struct pinctrl_dev *get_pinctrl_dev_from_devname(const char *dev_name);
int pin_get_from_name(struct pinctrl_dev *pctldev, const char *name);
int pinctrl_get_group_selector(struct pinctrl_dev *pctldev,
			       const char *pin_group);

static inline struct pin_desc *pin_desc_get(struct pinctrl_dev *pctldev,
					    unsigned int pin)
{
	return radix_tree_lookup(&pctldev->pin_desc_tree, pin);
}
