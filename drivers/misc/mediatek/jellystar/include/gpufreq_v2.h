/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Jelly Star: MT6789 is an "entry level" platform and runs MediaTek's gpufreq v2_legacy driver.
 * Some power/throttling modules include gpufreq_v2.h unconditionally; building them against the
 * v2 API gives symbol versions the legacy wrapper doesn't export. Map the header to the legacy API.
 */
#ifndef __JELLYSTAR_GPUFREQ_V2_SHIM_H__
#define __JELLYSTAR_GPUFREQ_V2_SHIM_H__
#include <gpufreq_v2_legacy.h>
#endif
