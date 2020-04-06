/* Copyright (c) 2020, Nokia
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <odp_api.h>
#include "odp_cunit_common.h"

#define MAGIC_U64  0x8b7438fa56c82e96
#define MAGIC_U32  0x74a13b94
#define MAGIC_U16  0x25bf
#define MAGIC_U8   0xab

#define NUM_U64    1024
#define NUM_U32    1024
#define NUM_U16    1024
#define NUM_U8     256

#define CACHE_SIZE 8
#define BURST      32
#define MAX_RETRY  1024
#define RETRY_MSEC 100

typedef struct num_obj_t {
	uint32_t u64;
	uint32_t u32;
	uint32_t u16;
	uint32_t u8;

} num_obj_t;

typedef struct global_t {
	odp_stash_capability_t capa_default;
	odp_stash_capability_t capa_fifo;
	int fifo_supported;
	num_obj_t num_default;
	num_obj_t num_fifo;
	uint32_t  cache_size_default;

} global_t;

static global_t global;

static int stash_suite_init(void)
{
	odp_stash_capability_t *capa_default = &global.capa_default;
	odp_stash_capability_t *capa_fifo = &global.capa_fifo;

	if (odp_stash_capability(capa_default, ODP_STASH_TYPE_DEFAULT)) {
		printf("stash capability failed for the default type\n");
		return -1;
	}

	global.num_default.u64 = NUM_U64;
	global.num_default.u32 = NUM_U32;
	global.num_default.u16 = NUM_U16;
	global.num_default.u8  = NUM_U8;

	if (global.num_default.u64 > capa_default->max_num_obj)
		global.num_default.u64 = capa_default->max_num_obj;
	if (global.num_default.u32 > capa_default->max_num_obj)
		global.num_default.u32 = capa_default->max_num_obj;
	if (global.num_default.u16 > capa_default->max_num_obj)
		global.num_default.u16 = capa_default->max_num_obj;
	if (global.num_default.u8 > capa_default->max_num_obj)
		global.num_default.u8 = capa_default->max_num_obj;

	global.cache_size_default = CACHE_SIZE;
	if (global.cache_size_default > capa_default->max_cache_size)
		global.cache_size_default = capa_default->max_cache_size;

	global.fifo_supported = 0;
	if (odp_stash_capability(capa_fifo, ODP_STASH_TYPE_FIFO) == 0) {
		if (capa_fifo->max_stashes)
			global.fifo_supported = 1;
	}

	if (global.fifo_supported) {
		global.num_fifo.u64 = NUM_U64;
		global.num_fifo.u32 = NUM_U32;
		global.num_fifo.u16 = NUM_U16;
		global.num_fifo.u8  = NUM_U8;

		if (global.num_fifo.u64 > capa_fifo->max_num_obj)
			global.num_fifo.u64 = capa_fifo->max_num_obj;
		if (global.num_fifo.u32 > capa_fifo->max_num_obj)
			global.num_fifo.u32 = capa_fifo->max_num_obj;
		if (global.num_fifo.u16 > capa_fifo->max_num_obj)
			global.num_fifo.u16 = capa_fifo->max_num_obj;
		if (global.num_fifo.u8 > capa_fifo->max_num_obj)
			global.num_fifo.u8 = capa_fifo->max_num_obj;
	}

	return 0;
}

static void stash_capability(void)
{
	odp_stash_capability_t capa;
	int ret;

	memset(&capa, 0, sizeof(odp_stash_capability_t));
	CU_ASSERT(odp_stash_capability(&capa, ODP_STASH_TYPE_DEFAULT) == 0);
	CU_ASSERT(capa.max_stashes_any_type > 0);
	CU_ASSERT(capa.max_stashes > 0);
	CU_ASSERT(capa.max_num_obj > 0);
	CU_ASSERT(capa.max_obj_size >= sizeof(uint32_t));

	memset(&capa, 0, sizeof(odp_stash_capability_t));
	ret = odp_stash_capability(&capa, ODP_STASH_TYPE_FIFO);
	CU_ASSERT(ret == 0);
	CU_ASSERT(capa.max_stashes_any_type > 0);
	if (ret == 0 && capa.max_stashes) {
		CU_ASSERT(capa.max_num_obj > 0);
		CU_ASSERT(capa.max_obj_size >= sizeof(uint32_t));
	}
}

static void stash_param_defaults(void)
{
	odp_stash_param_t param;

	memset(&param, 0xff, sizeof(odp_stash_param_t));
	odp_stash_param_init(&param);
	CU_ASSERT(param.type == ODP_STASH_TYPE_DEFAULT);
	CU_ASSERT(param.put_mode == ODP_STASH_OP_MT);
	CU_ASSERT(param.get_mode == ODP_STASH_OP_MT);
	CU_ASSERT(param.cache_size == 0);
}

static void stash_create_u64(void)
{
	odp_stash_t stash, lookup;
	odp_stash_param_t param;
	uint32_t num = global.num_default.u64;

	odp_stash_param_init(&param);
	param.num_obj  = num;
	param.obj_size = sizeof(uint64_t);

	stash = odp_stash_create("test_stash_u64", &param);

	CU_ASSERT_FATAL(stash != ODP_STASH_INVALID);

	printf("\n    Stash handle: 0x%" PRIx64 "\n", odp_stash_to_u64(stash));

	lookup = odp_stash_lookup("test_stash_u64");
	CU_ASSERT(lookup != ODP_STASH_INVALID);
	CU_ASSERT(stash == lookup);
	CU_ASSERT(odp_stash_lookup("foo") == ODP_STASH_INVALID);

	CU_ASSERT_FATAL(odp_stash_destroy(stash) == 0);
}

static void stash_create_u32(void)
{
	odp_stash_t stash, lookup;
	odp_stash_param_t param;
	uint32_t num = global.num_default.u32;

	odp_stash_param_init(&param);
	param.num_obj  = num;
	param.obj_size = sizeof(uint32_t);

	stash = odp_stash_create("test_stash_u32", &param);

	CU_ASSERT_FATAL(stash != ODP_STASH_INVALID);

	printf("\n    Stash handle: 0x%" PRIx64 "\n", odp_stash_to_u64(stash));

	lookup = odp_stash_lookup("test_stash_u32");
	CU_ASSERT(lookup != ODP_STASH_INVALID);
	CU_ASSERT(stash == lookup);
	CU_ASSERT(odp_stash_lookup("foo") == ODP_STASH_INVALID);

	CU_ASSERT_FATAL(odp_stash_destroy(stash) == 0);
}

static void stash_create_u64_all(void)
{
	odp_stash_param_t param;
	uint64_t input, output;
	uint32_t i, retry;
	int32_t ret;
	uint32_t num_obj = global.num_default.u32;
	uint32_t num_stash = global.capa_default.max_stashes;
	odp_stash_t stash[num_stash];

	CU_ASSERT_FATAL(num_stash > 0);

	odp_stash_param_init(&param);
	param.num_obj  = num_obj;
	param.obj_size = sizeof(uint64_t);

	for (i = 0; i < num_stash; i++) {
		stash[i] = odp_stash_create("test_stash_u64_all", &param);
		CU_ASSERT_FATAL(stash[i] != ODP_STASH_INVALID);
		CU_ASSERT_FATAL(odp_stash_get(stash[i], &output, 1) == 0);

		input = i;
		CU_ASSERT(odp_stash_put(stash[i], &input, 1) == 1);
	}

	for (i = 0; i < num_stash; i++) {
		ret = 0;

		for (retry = 0; retry < RETRY_MSEC; retry++) {
			/* Extra delay allows HW latency from put() to get() */
			odp_time_wait_ns(ODP_TIME_MSEC_IN_NS);
			ret = odp_stash_get(stash[i], &output, 1);
			if (ret)
				break;
		}

		CU_ASSERT(ret == 1);
		CU_ASSERT(output == i);
	}

	for (i = 0; i < num_stash; i++)
		CU_ASSERT_FATAL(odp_stash_destroy(stash[i]) == 0);
}

static void stash_create_u32_all(void)
{
	odp_stash_param_t param;
	uint32_t i, retry, input, output;
	int32_t ret;
	uint32_t num_obj = global.num_default.u32;
	uint32_t num_stash = global.capa_default.max_stashes;
	odp_stash_t stash[num_stash];

	CU_ASSERT_FATAL(num_stash > 0);

	odp_stash_param_init(&param);
	param.num_obj  = num_obj;
	param.obj_size = sizeof(uint32_t);

	for (i = 0; i < num_stash; i++) {
		stash[i] = odp_stash_create("test_stash_u32_all", &param);
		CU_ASSERT_FATAL(stash[i] != ODP_STASH_INVALID);
		CU_ASSERT_FATAL(odp_stash_get(stash[i], &output, 1) == 0);

		input = i;
		CU_ASSERT(odp_stash_put(stash[i], &input, 1) == 1);
	}

	for (i = 0; i < num_stash; i++) {
		ret = 0;

		for (retry = 0; retry < RETRY_MSEC; retry++) {
			/* Extra delay allows HW latency from put() to get() */
			odp_time_wait_ns(ODP_TIME_MSEC_IN_NS);
			ret = odp_stash_get(stash[i], &output, 1);
			if (ret)
				break;
		}

		CU_ASSERT(ret == 1);
		CU_ASSERT(output == i);
	}

	for (i = 0; i < num_stash; i++)
		CU_ASSERT_FATAL(odp_stash_destroy(stash[i]) == 0);
}

static void stash_create_fifo_u64_all(void)
{
	odp_stash_param_t param;
	uint64_t input, output;
	uint32_t i, retry;
	int32_t ret;
	uint32_t num_obj = global.num_fifo.u64;
	uint32_t num_stash = global.capa_fifo.max_stashes;
	odp_stash_t stash[num_stash];

	CU_ASSERT_FATAL(num_stash > 0);

	odp_stash_param_init(&param);
	param.type     = ODP_STASH_TYPE_FIFO;
	param.num_obj  = num_obj;
	param.obj_size = sizeof(uint64_t);

	for (i = 0; i < num_stash; i++) {
		stash[i] = odp_stash_create(NULL, &param);
		CU_ASSERT_FATAL(stash[i] != ODP_STASH_INVALID);
		CU_ASSERT_FATAL(odp_stash_get(stash[i], &output, 1) == 0);

		input = i;
		CU_ASSERT(odp_stash_put(stash[i], &input, 1) == 1);
	}

	for (i = 0; i < num_stash; i++) {
		ret = 0;

		for (retry = 0; retry < RETRY_MSEC; retry++) {
			/* Extra delay allows HW latency from put() to get() */
			odp_time_wait_ns(ODP_TIME_MSEC_IN_NS);
			ret = odp_stash_get(stash[i], &output, 1);
			if (ret)
				break;
		}

		CU_ASSERT(ret == 1);
		CU_ASSERT(output == i);
	}

	for (i = 0; i < num_stash; i++)
		CU_ASSERT_FATAL(odp_stash_destroy(stash[i]) == 0);
}

static void stash_create_fifo_u32_all(void)
{
	odp_stash_param_t param;
	uint32_t i, retry, input, output;
	int32_t ret;
	uint32_t num_obj = global.num_fifo.u32;
	uint32_t num_stash = global.capa_fifo.max_stashes;
	odp_stash_t stash[num_stash];

	CU_ASSERT_FATAL(num_stash > 0);

	odp_stash_param_init(&param);
	param.type     = ODP_STASH_TYPE_FIFO;
	param.num_obj  = num_obj;
	param.obj_size = sizeof(uint32_t);

	for (i = 0; i < num_stash; i++) {
		stash[i] = odp_stash_create(NULL, &param);
		CU_ASSERT_FATAL(stash[i] != ODP_STASH_INVALID);
		CU_ASSERT_FATAL(odp_stash_get(stash[i], &output, 1) == 0);

		input = i;
		CU_ASSERT(odp_stash_put(stash[i], &input, 1) == 1);
	}

	for (i = 0; i < num_stash; i++) {
		ret = 0;

		for (retry = 0; retry < RETRY_MSEC; retry++) {
			/* Extra delay allows HW latency from put() to get() */
			odp_time_wait_ns(ODP_TIME_MSEC_IN_NS);
			ret = odp_stash_get(stash[i], &output, 1);
			if (ret)
				break;
		}

		CU_ASSERT(ret == 1);
		CU_ASSERT(output == i);
	}

	for (i = 0; i < num_stash; i++)
		CU_ASSERT_FATAL(odp_stash_destroy(stash[i]) == 0);
}

static void stash_default_put(uint32_t size, int32_t burst)
{
	odp_stash_t stash;
	odp_stash_param_t param;
	int32_t i, ret, retry, num_left;
	int32_t num;
	void *input, *output;
	uint64_t input_u64[burst];
	uint64_t output_u64[burst];
	uint32_t input_u32[burst];
	uint32_t output_u32[burst];
	uint16_t input_u16[burst];
	uint16_t output_u16[burst];
	uint8_t input_u8[burst];
	uint8_t output_u8[burst];

	if (size == sizeof(uint64_t)) {
		num    = global.num_default.u64;
		input  = input_u64;
		output = output_u64;
	} else if (size == sizeof(uint32_t)) {
		num    = global.num_default.u32;
		input  = input_u32;
		output = output_u32;
	} else if (size == sizeof(uint16_t)) {
		num    = global.num_default.u16;
		input  = input_u16;
		output = output_u16;
	} else {
		num    = global.num_default.u8;
		input  = input_u8;
		output = output_u8;
	}

	for (i = 0; i < burst; i++) {
		input_u64[i] = MAGIC_U64;
		input_u32[i] = MAGIC_U32;
		input_u16[i] = MAGIC_U16;
		input_u8[i]  = MAGIC_U8;
	}

	odp_stash_param_init(&param);
	param.num_obj    = num;
	param.obj_size   = size;
	param.cache_size = global.cache_size_default;

	stash = odp_stash_create("test_stash_default", &param);

	CU_ASSERT_FATAL(stash != ODP_STASH_INVALID);

	/* Stash is empty */
	CU_ASSERT_FATAL(odp_stash_get(stash, output, 1) == 0);

	retry = MAX_RETRY;
	num_left = num;
	while (num_left) {
		ret = odp_stash_put(stash, input, burst);
		CU_ASSERT_FATAL(ret >= 0);
		CU_ASSERT_FATAL(ret <= burst);

		if (ret) {
			num_left -= ret;
			retry = MAX_RETRY;
		} else {
			retry--;
			CU_ASSERT_FATAL(retry > 0);
		}
	}

	retry = MAX_RETRY;
	num_left = num;
	while (num_left) {
		memset(output, 0, burst * size);

		ret = odp_stash_get(stash, output, burst);
		CU_ASSERT_FATAL(ret >= 0);
		CU_ASSERT_FATAL(ret <= burst);

		if (ret) {
			for (i = 0; i < ret; i++) {
				if (size == sizeof(uint64_t)) {
					/* CU_ASSERT needs brackets around it */
					CU_ASSERT(output_u64[i] == MAGIC_U64);
				} else if (size == sizeof(uint32_t)) {
					CU_ASSERT(output_u32[i] == MAGIC_U32);
				} else if (size == sizeof(uint16_t)) {
					CU_ASSERT(output_u16[i] == MAGIC_U16);
				} else {
					CU_ASSERT(output_u8[i] == MAGIC_U8);
				}
			}

			num_left -= ret;
			retry = MAX_RETRY;
		} else {
			retry--;
			CU_ASSERT_FATAL(retry > 0);
		}
	}

	/* Stash is empty again */
	CU_ASSERT(odp_stash_get(stash, output, 1) == 0);
	CU_ASSERT(odp_stash_flush_cache(stash) == 0);

	CU_ASSERT_FATAL(odp_stash_destroy(stash) == 0);
}

static void stash_fifo_put(uint32_t size, int32_t burst)
{
	odp_stash_t stash;
	odp_stash_param_t param;
	int32_t i, ret, retry, num_left;
	int32_t num;
	void *input, *output;
	uint64_t input_u64[burst];
	uint64_t output_u64[burst];
	uint32_t input_u32[burst];
	uint32_t output_u32[burst];
	uint16_t input_u16[burst];
	uint16_t output_u16[burst];
	uint8_t input_u8[burst];
	uint8_t output_u8[burst];

	if (size == sizeof(uint64_t)) {
		num    = global.num_fifo.u64;
		input  = input_u64;
		output = output_u64;
	} else if (size == sizeof(uint32_t)) {
		num    = global.num_fifo.u32;
		input  = input_u32;
		output = output_u32;
	} else if (size == sizeof(uint16_t)) {
		num    = global.num_fifo.u16;
		input  = input_u16;
		output = output_u16;
	} else {
		num    = global.num_fifo.u8;
		input  = input_u8;
		output = output_u8;
	}

	odp_stash_param_init(&param);
	param.type     = ODP_STASH_TYPE_FIFO;
	param.num_obj  = num;
	param.obj_size = size;

	stash = odp_stash_create("test_stash_fifo", &param);

	CU_ASSERT_FATAL(stash != ODP_STASH_INVALID);

	/* Stash is empty */
	CU_ASSERT_FATAL(odp_stash_get(stash, output, 1) == 0);

	retry = MAX_RETRY;
	num_left = num;
	while (num_left) {
		for (i = 0; i < burst; i++) {
			if (size == sizeof(uint64_t))
				input_u64[i] = MAGIC_U64 + num_left - i;
			else if (size == sizeof(uint32_t))
				input_u32[i] = MAGIC_U32 + num_left - i;
			else if (size == sizeof(uint16_t))
				input_u16[i] = MAGIC_U16 + num_left - i;
			else
				input_u8[i] = MAGIC_U8 + num_left - i;
		}

		ret = odp_stash_put(stash, input, burst);
		CU_ASSERT_FATAL(ret >= 0);

		if (ret) {
			CU_ASSERT_FATAL(ret <= burst);
			num_left -= ret;
			retry = MAX_RETRY;
		} else {
			retry--;
			CU_ASSERT_FATAL(retry > 0);
		}
	}

	retry = MAX_RETRY;
	num_left = num;
	while (num_left) {
		memset(output, 0, burst * size);

		ret = odp_stash_get(stash, output, burst);
		CU_ASSERT_FATAL(ret >= 0);

		if (ret) {
			CU_ASSERT_FATAL(ret <= burst);
			for (i = 0; i < ret; i++) {
				if (size == sizeof(uint64_t)) {
					uint64_t val = MAGIC_U64 + num_left - i;

					CU_ASSERT(output_u64[i] == val);
				} else if (size == sizeof(uint32_t)) {
					uint32_t val = MAGIC_U32 + num_left - i;

					CU_ASSERT(output_u32[i] == val);
				} else if (size == sizeof(uint16_t)) {
					uint16_t val = MAGIC_U16 + num_left - i;

					CU_ASSERT(output_u16[i] == val);
				} else {
					uint8_t val = MAGIC_U8 + num_left - i;

					CU_ASSERT(output_u8[i] == val);
				}
			}

			num_left -= ret;
			retry = MAX_RETRY;
		} else {
			retry--;
			CU_ASSERT_FATAL(retry > 0);
		}
	}

	/* Stash is empty again */
	CU_ASSERT(odp_stash_get(stash, output, 1) == 0);
	CU_ASSERT(odp_stash_flush_cache(stash) == 0);

	CU_ASSERT_FATAL(odp_stash_destroy(stash) == 0);
}

static int check_support_64(void)
{
	if (global.capa_default.max_obj_size >= sizeof(uint64_t))
		return ODP_TEST_ACTIVE;

	return ODP_TEST_INACTIVE;
}

static int check_support_fifo_64(void)
{
	if (global.fifo_supported &&
	    global.capa_fifo.max_obj_size >= sizeof(uint64_t))
		return ODP_TEST_ACTIVE;

	return ODP_TEST_INACTIVE;
}

static int check_support_fifo(void)
{
	if (global.fifo_supported)
		return ODP_TEST_ACTIVE;

	return ODP_TEST_INACTIVE;
}

static void stash_default_put_u64_1(void)
{
	stash_default_put(sizeof(uint64_t), 1);
}

static void stash_default_put_u64_n(void)
{
	stash_default_put(sizeof(uint64_t), BURST);
}

static void stash_default_put_u32_1(void)
{
	stash_default_put(sizeof(uint32_t), 1);
}

static void stash_default_put_u32_n(void)
{
	stash_default_put(sizeof(uint32_t), BURST);
}

static void stash_default_put_u16_1(void)
{
	stash_default_put(sizeof(uint16_t), 1);
}

static void stash_default_put_u16_n(void)
{
	stash_default_put(sizeof(uint16_t), BURST);
}

static void stash_default_put_u8_1(void)
{
	stash_default_put(sizeof(uint8_t), 1);
}

static void stash_default_put_u8_n(void)
{
	stash_default_put(sizeof(uint8_t), BURST);
}

static void stash_fifo_put_u64_1(void)
{
	stash_fifo_put(sizeof(uint64_t), 1);
}

static void stash_fifo_put_u64_n(void)
{
	stash_fifo_put(sizeof(uint64_t), BURST);
}

static void stash_fifo_put_u32_1(void)
{
	stash_fifo_put(sizeof(uint32_t), 1);
}

static void stash_fifo_put_u32_n(void)
{
	stash_fifo_put(sizeof(uint32_t), BURST);
}

static void stash_fifo_put_u16_1(void)
{
	stash_fifo_put(sizeof(uint16_t), 1);
}

static void stash_fifo_put_u16_n(void)
{
	stash_fifo_put(sizeof(uint16_t), BURST);
}

static void stash_fifo_put_u8_1(void)
{
	stash_fifo_put(sizeof(uint8_t), 1);
}

static void stash_fifo_put_u8_n(void)
{
	stash_fifo_put(sizeof(uint8_t), BURST);
}

odp_testinfo_t stash_suite[] = {
	ODP_TEST_INFO(stash_capability),
	ODP_TEST_INFO(stash_param_defaults),
	ODP_TEST_INFO_CONDITIONAL(stash_create_u64, check_support_64),
	ODP_TEST_INFO(stash_create_u32),
	ODP_TEST_INFO_CONDITIONAL(stash_default_put_u64_1, check_support_64),
	ODP_TEST_INFO_CONDITIONAL(stash_default_put_u64_n, check_support_64),
	ODP_TEST_INFO(stash_default_put_u32_1),
	ODP_TEST_INFO(stash_default_put_u32_n),
	ODP_TEST_INFO(stash_default_put_u16_1),
	ODP_TEST_INFO(stash_default_put_u16_n),
	ODP_TEST_INFO(stash_default_put_u8_1),
	ODP_TEST_INFO(stash_default_put_u8_n),
	ODP_TEST_INFO_CONDITIONAL(stash_create_u64_all, check_support_64),
	ODP_TEST_INFO(stash_create_u32_all),
	ODP_TEST_INFO_CONDITIONAL(stash_fifo_put_u64_1, check_support_fifo_64),
	ODP_TEST_INFO_CONDITIONAL(stash_fifo_put_u64_n, check_support_fifo_64),
	ODP_TEST_INFO_CONDITIONAL(stash_fifo_put_u32_1, check_support_fifo),
	ODP_TEST_INFO_CONDITIONAL(stash_fifo_put_u32_n, check_support_fifo),
	ODP_TEST_INFO_CONDITIONAL(stash_fifo_put_u16_1, check_support_fifo),
	ODP_TEST_INFO_CONDITIONAL(stash_fifo_put_u16_n, check_support_fifo),
	ODP_TEST_INFO_CONDITIONAL(stash_fifo_put_u8_1,  check_support_fifo),
	ODP_TEST_INFO_CONDITIONAL(stash_fifo_put_u8_n,  check_support_fifo),
	ODP_TEST_INFO_CONDITIONAL(stash_create_fifo_u64_all,
				  check_support_fifo_64),
	ODP_TEST_INFO_CONDITIONAL(stash_create_fifo_u32_all,
				  check_support_fifo),
	ODP_TEST_INFO_NULL
};

odp_suiteinfo_t stash_suites[] = {
		{"Stash", stash_suite_init, NULL, stash_suite},
		ODP_SUITE_INFO_NULL
};

int main(int argc, char *argv[])
{
	int ret;

	/* parse common options: */
	if (odp_cunit_parse_options(argc, argv))
		return -1;

	ret = odp_cunit_register(stash_suites);

	if (ret == 0)
		ret = odp_cunit_run();

	return ret;
}
