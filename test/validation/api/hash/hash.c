/* Copyright (c) 2015-2018, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */

#include "config.h"

#include <odp_api.h>
#include <odp_cunit_common.h>

#define CRC32C_INIT 0xffffffff
#define CRC32C_XOR  0xffffffff

typedef struct hash_test_vector_t {
	const uint8_t  *data;
	uint32_t  len;

	union {
		uint32_t u32;
		uint8_t  u8[4];
	} result;

} hash_test_vector_t;

/*
 * Test vectors 0-4 from RFC 7143.
 */
static const uint8_t test_data_0[] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static const uint8_t test_data_1[] = {
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
};

static const uint8_t test_data_2[] = {
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
	0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
	0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
	0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f
};

static const uint8_t test_data_3[] = {
	0x1f, 0x1e, 0x1d, 0x1c, 0x1b, 0x1a, 0x19, 0x18,
	0x17, 0x16, 0x15, 0x14, 0x13, 0x12, 0x11, 0x10,
	0x0f, 0x0e, 0x0d, 0x0c, 0x0b, 0x0a, 0x09, 0x08,
	0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0x00
};

static const uint8_t test_data_4[] = {
	0x01, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x14, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00,
	0x00, 0x00, 0x00, 0x14, 0x00, 0x00, 0x00, 0x18,
	0x28, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

/* Various length strings. Terminating null character is not included into
 * crc calculation. */
static const uint8_t test_data_5[] = "abcd";

static const uint8_t test_data_6[] = "abcdefgh";

static const uint8_t test_data_7[] =
	"The quick brown fox jumps over the lazy dog.";

static const hash_test_vector_t crc32c_test_vector[] = {
	{ .data = test_data_0,
	  .len = sizeof(test_data_0),
	  .result.u8 = {0xaa, 0x36, 0x91, 0x8a}
	},
	{ .data = test_data_1,
	  .len = sizeof(test_data_1),
	  .result.u8 = {0x43, 0xab, 0xa8, 0x62}
	},
	{ .data = test_data_2,
	  .len = sizeof(test_data_2),
	  .result.u8 = {0x4e, 0x79, 0xdd, 0x46}
	},
	{ .data = test_data_3,
	  .len = sizeof(test_data_3),
	  .result.u8 = {0x5c, 0xdb, 0x3f, 0x11}
	},
	{ .data = test_data_4,
	  .len = sizeof(test_data_4),
	  .result.u8 = {0x56, 0x3a, 0x96, 0xd9}
	},
	{ .data = test_data_5,
	  .len = sizeof(test_data_5) - 1,
	  .result.u8 = {0x31, 0x0a, 0xc8, 0x92}
	},
	{ .data = test_data_6,
	  .len = sizeof(test_data_6) - 1,
	  .result.u8 = {0xb7, 0x21, 0x94, 0x0a}
	},
	{ .data = test_data_7,
	  .len = sizeof(test_data_7) - 1,
	  .result.u8 = {0xb3, 0x97, 0x00, 0x19}
	}
};

static void hash_test_crc32c(void)
{
	uint32_t ret, result;
	int i;
	int num = sizeof(crc32c_test_vector) / sizeof(hash_test_vector_t);

	for (i = 0; i < num; i++) {
		ret = odp_hash_crc32c(crc32c_test_vector[i].data,
				      crc32c_test_vector[i].len,
				      CRC32C_INIT);

		result = CRC32C_XOR ^ ret;
		CU_ASSERT(result == crc32c_test_vector[i].result.u32);
	}
}

odp_testinfo_t hash_suite[] = {
	ODP_TEST_INFO(hash_test_crc32c),
	ODP_TEST_INFO_NULL,
};

odp_suiteinfo_t hash_suites[] = {
	{"Hash", NULL, NULL, hash_suite},
	ODP_SUITE_INFO_NULL
};

int main(int argc, char *argv[])
{
	int ret;

	/* parse common options: */
	if (odp_cunit_parse_options(argc, argv))
		return -1;

	ret = odp_cunit_register(hash_suites);

	if (ret == 0)
		ret = odp_cunit_run();

	return ret;
}
