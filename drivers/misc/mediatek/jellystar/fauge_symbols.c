// SPDX-License-Identifier: GPL-2.0
/*
 * Build-time stand-in for the variables that the stock Jelly Star fuel-gauge module (fauge, OZ8806)
 * exports. mtk_charger_framework links against these; only fauge is shipped, so at runtime the
 * symbols resolve to fauge's copies. Both are plain ints (their symbol CRC is that of
 * `variable int`), matching what the stock mtk_charger_framework.ko imports.
 */
#include <linux/module.h>

int g_is_charger_on;
EXPORT_SYMBOL(g_is_charger_on);

int g_charger_type;
EXPORT_SYMBOL(g_charger_type);

MODULE_DESCRIPTION("Jelly Star: build-only fauge symbol stand-in (not shipped)");
MODULE_LICENSE("GPL");
