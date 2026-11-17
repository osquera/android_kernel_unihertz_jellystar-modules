// SPDX-License-Identifier: GPL-2.0
/*
 * Agold compatibility hooks for the Unihertz Jelly Star (MT6789).
 *
 * Unihertz's ODM (Agold) added a few exported functions to MediaTek's kernel modules, and their
 * own driver modules (fuel gauge, charger, modem board detection, device info) import them. The
 * MediaTek modules built from source don't have them, so this module provides them.
 *
 * Written without Agold's source or binaries' code: the prototypes, including parameter names, are
 * the ones whose symbol versions (gendwarfksyms CRCs) match what the stock modules import. Their
 * behaviour follows from the names and from what the stock firmware shows at runtime.
 */

#include <linux/gpio/consumer.h>
#include <linux/gpio/driver.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/string.h>

/* MediaTek's pinctrl driver (pinctrl-paris) registers the SoC GPIO controller under this label. */
#define MTK_GPIO_LABEL "pinctrl_mtk_v2"

static DEFINE_MUTEX(info_lock);
static char other_device_info[PAGE_SIZE];

/*
 * Drivers register a line of device information (e.g. the charger or fuel gauge chip name) under a
 * key. Stock shows it in Agold's device-info app; here it is only kept for
 * agold_get_other_device_info().
 */
void agold_other_device_info_append(char *info, char *key)
{
	size_t len;

	if (!info)
		return;
	mutex_lock(&info_lock);
	len = strlen(other_device_info);
	scnprintf(other_device_info + len, sizeof(other_device_info) - len, "%s: %s\n",
		  key ? key : "", info);
	mutex_unlock(&info_lock);
}
EXPORT_SYMBOL(agold_other_device_info_append);

char *agold_get_other_device_info(void)
{
	return other_device_info;
}
EXPORT_SYMBOL(agold_get_other_device_info);

/*
 * Panel information that the stock panel driver exported for Agold's board and info modules.
 * Our panel driver doesn't fill them in; they are only shown by Agold's device-info app.
 */
bool gms_screen;
EXPORT_SYMBOL(gms_screen);

static char lcm_driver_name[1];

char *agold_get_lcm_driver_name(void)
{
	return lcm_driver_name;
}
EXPORT_SYMBOL(agold_get_lcm_driver_name);

static int match_mtk_gpio(struct gpio_chip *gc, const void *data)
{
	return gc->label && !strcmp(gc->label, data);
}

/* Returns the input level of SoC GPIO @offset, or a negative error code. */
int agold_gpio_get_in(unsigned int offset)
{
	struct gpio_device *gdev;
	struct gpio_desc *desc;
	int ret;

	gdev = gpio_device_find(MTK_GPIO_LABEL, match_mtk_gpio);
	if (!gdev)
		return -ENODEV;
	desc = gpio_device_get_desc(gdev, offset);
	ret = IS_ERR(desc) ? PTR_ERR(desc) : gpiod_get_raw_value_cansleep(desc);
	gpio_device_put(gdev);
	return ret;
}
EXPORT_SYMBOL(agold_gpio_get_in);

MODULE_DESCRIPTION("Agold ODM compatibility hooks for the Unihertz Jelly Star");
MODULE_LICENSE("GPL");
