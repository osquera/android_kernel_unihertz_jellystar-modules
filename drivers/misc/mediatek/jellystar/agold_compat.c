// SPDX-License-Identifier: GPL-2.0
/*
 * Agold compatibility hooks for the Unihertz Jelly Star (MT6789).
 *
 * Unihertz's ODM (Agold) added a few exported functions to MediaTek's kernel modules, and their
 * own driver modules that are still stock binaries (fuel gauge, board info) import them. The
 * MediaTek modules built from source don't have them, so this module provides them.
 *
 * Written without Agold's source or binaries' code: the prototypes, including parameter names, are
 * the ones whose symbol versions (gendwarfksyms CRCs) match what the stock modules import. Their
 * behaviour follows from the names and from what the stock firmware shows at runtime.
 */

#include <linux/module.h>

/*
 * Drivers register a line of device information (e.g. the fuel gauge chip name) under a key. Stock
 * collects it for Agold's device-info app (/proc/driver/agold_other_device_info, from
 * agold_pm_info); nothing reads it here.
 */
void agold_other_device_info_append(char *info, char *key)
{
}
EXPORT_SYMBOL(agold_other_device_info_append);

/*
 * Panel information that the stock panel driver exported for Agold's board info module. Our panel
 * driver doesn't fill it in; it is only shown by Agold's device-info app.
 */
bool gms_screen;
EXPORT_SYMBOL(gms_screen);

MODULE_DESCRIPTION("Agold ODM compatibility hooks for the Unihertz Jelly Star");
MODULE_LICENSE("GPL");
