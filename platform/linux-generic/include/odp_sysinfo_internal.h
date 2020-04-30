/* Copyright (c) 2013-2018, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */

#ifndef ODP_SYSINFO_INTERNAL_H_
#define ODP_SYSINFO_INTERNAL_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <odp_global_data.h>
#include <odp_debug_internal.h>
#include <inttypes.h>
#include <string.h>

int cpuinfo_parser(FILE *file, system_info_t *sysinfo);
uint64_t odp_cpu_hz_current(int id);
uint64_t odp_cpu_arch_hz_current(int id);
void sys_info_print_arch(void);

static inline int _odp_dummy_cpuinfo(system_info_t *sysinfo)
{
	uint64_t cpu_hz_max = sysinfo->default_cpu_hz_max;
	int i;

	ODP_DBG("Warning: use dummy values for freq and model string\n");
	for (i = 0; i < CONFIG_NUM_CPU_IDS; i++) {
		ODP_PRINT("WARN: cpu[%i] uses default max frequency of "
			  "%" PRIu64 " Hz from config file\n", i, cpu_hz_max);
		sysinfo->cpu_hz_max[i] = cpu_hz_max;
		strcpy(sysinfo->model_str[i], "UNKNOWN");
	}

	return 0;
}

#ifdef __cplusplus
}
#endif

#endif
