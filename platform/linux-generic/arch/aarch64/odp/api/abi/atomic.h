/* Copyright (c) 2021, ARM Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */

#ifdef __ARM_FEATURE_ATOMICS
#define _ODP_LOCK_FREE_128BIT_ATOMICS
#endif

#include <odp/api/abi-default/atomic.h>
#include <odp/api/plat/atomic_inlines.h>
