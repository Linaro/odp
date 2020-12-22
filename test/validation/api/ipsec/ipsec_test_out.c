/* Copyright (c) 2017-2018, Linaro Limited
 * Copyright (c) 2020, Marvell
 * Copyright (c) 2020, Nokia
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */

#include "ipsec.h"

#include "test_vectors.h"

struct cipher_param {
	const char *name;
	odp_cipher_alg_t algo;
	const odp_crypto_key_t *key;
	const odp_crypto_key_t *key_extra;
};

struct auth_param {
	const char *name;
	odp_auth_alg_t algo;
	const odp_crypto_key_t *key;
	const odp_crypto_key_t *key_extra;
};

#define ALG(alg, key, key_extra) { #alg, alg, key, key_extra }

/*
 * Ciphers that can be used in ESP and combined with any integrity
 * algorithm. This excludes combined mode algorithms such as AES-GCM.
 */
static struct cipher_param ciphers[] = {
	ALG(ODP_CIPHER_ALG_NULL, NULL, NULL),
	ALG(ODP_CIPHER_ALG_DES, &key_des_64, NULL),
	ALG(ODP_CIPHER_ALG_3DES_CBC, &key_des_192, NULL),
	ALG(ODP_CIPHER_ALG_AES_CBC, &key_a5_128, NULL),
	ALG(ODP_CIPHER_ALG_AES_CBC, &key_a5_192, NULL),
	ALG(ODP_CIPHER_ALG_AES_CBC, &key_a5_256, NULL),
	ALG(ODP_CIPHER_ALG_AES_CTR, &key_a5_128, &key_mcgrew_gcm_salt_3),
	ALG(ODP_CIPHER_ALG_AES_CTR, &key_a5_192, &key_mcgrew_gcm_salt_3),
	ALG(ODP_CIPHER_ALG_AES_CTR, &key_a5_256, &key_mcgrew_gcm_salt_3)
};

/*
 * Integrity algorithms that can be used in ESP and AH. This excludes
 * AES-GMAC which is defined for ESP as a combined-mode algorithm.
 */
static struct auth_param auths[] = {
	ALG(ODP_AUTH_ALG_NULL, NULL, NULL),
	ALG(ODP_AUTH_ALG_MD5_HMAC, &key_5a_128, NULL),
	ALG(ODP_AUTH_ALG_SHA1_HMAC, &key_5a_160, NULL),
	ALG(ODP_AUTH_ALG_SHA256_HMAC, &key_5a_256, NULL),
	ALG(ODP_AUTH_ALG_SHA384_HMAC, &key_5a_384, NULL),
	ALG(ODP_AUTH_ALG_SHA512_HMAC, &key_5a_512, NULL),
	ALG(ODP_AUTH_ALG_AES_XCBC_MAC, &key_5a_128, NULL)
};

struct cipher_auth_comb_param {
	struct cipher_param cipher;
	struct auth_param auth;
};

static struct cipher_auth_comb_param cipher_auth_comb[] = {
	{
		ALG(ODP_CIPHER_ALG_AES_GCM, &key_a5_128, &key_mcgrew_gcm_salt_2),
		ALG(ODP_AUTH_ALG_AES_GCM, NULL, NULL),
	},
	{
		ALG(ODP_CIPHER_ALG_AES_GCM, &key_a5_192, &key_mcgrew_gcm_salt_2),
		ALG(ODP_AUTH_ALG_AES_GCM, NULL, NULL),
	},
	{
		ALG(ODP_CIPHER_ALG_AES_GCM, &key_a5_256, &key_mcgrew_gcm_salt_2),
		ALG(ODP_AUTH_ALG_AES_GCM, NULL, NULL),
	},
	{
		ALG(ODP_CIPHER_ALG_NULL, NULL, NULL),
		ALG(ODP_AUTH_ALG_AES_GMAC, &key_a5_128, &key_mcgrew_gcm_salt_2),
	},
	{
		ALG(ODP_CIPHER_ALG_NULL, NULL, NULL),
		ALG(ODP_AUTH_ALG_AES_GMAC, &key_a5_192, &key_mcgrew_gcm_salt_2),
	},
	{
		ALG(ODP_CIPHER_ALG_NULL, NULL, NULL),
		ALG(ODP_AUTH_ALG_AES_GMAC, &key_a5_256, &key_mcgrew_gcm_salt_2),
	},
	{
		ALG(ODP_CIPHER_ALG_AES_CCM, &key_a5_128, &key_3byte_salt),
		ALG(ODP_AUTH_ALG_AES_CCM, NULL, NULL),
	},
	{
		ALG(ODP_CIPHER_ALG_AES_CCM, &key_a5_192, &key_3byte_salt),
		ALG(ODP_AUTH_ALG_AES_CCM, NULL, NULL),
	},
	{
		ALG(ODP_CIPHER_ALG_AES_CCM, &key_a5_256, &key_3byte_salt),
		ALG(ODP_AUTH_ALG_AES_CCM, NULL, NULL),
	},
	{
		ALG(ODP_CIPHER_ALG_CHACHA20_POLY1305, &key_rfc7634, &key_rfc7634_salt),
		ALG(ODP_AUTH_ALG_CHACHA20_POLY1305, NULL, NULL),
	},
};

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

static void test_out_ipv4_ah_sha256(void)
{
	odp_ipsec_sa_param_t param;
	odp_ipsec_sa_t sa;

	ipsec_sa_param_fill(&param,
			    false, true, 123, NULL,
			    ODP_CIPHER_ALG_NULL, NULL,
			    ODP_AUTH_ALG_SHA256_HMAC, &key_5a_256,
			    NULL, NULL);

	sa = odp_ipsec_sa_create(&param);

	CU_ASSERT_NOT_EQUAL_FATAL(ODP_IPSEC_SA_INVALID, sa);

	ipsec_test_part test = {
		.pkt_in = &pkt_ipv4_icmp_0,
		.num_pkt = 1,
		.out = {
			{ .status.warn.all = 0,
			  .status.error.all = 0,
			  .pkt_res = &pkt_ipv4_icmp_0_ah_sha256_1 },
		},
	};

	ipsec_check_out_one(&test, sa);

	ipsec_sa_destroy(sa);
}

#define IPV4ADDR(a, b, c, d) odp_cpu_to_be_32((a << 24) | \
					      (b << 16) | \
					      (c << 8) | \
					      (d << 0))

static void test_out_ipv4_ah_sha256_tun_ipv4(void)
{
	odp_ipsec_tunnel_param_t tunnel;
	odp_ipsec_sa_param_t param;
	odp_ipsec_sa_t sa;
	uint32_t src = IPV4ADDR(10, 0, 111, 2);
	uint32_t dst = IPV4ADDR(10, 0, 222, 2);

	memset(&tunnel, 0, sizeof(odp_ipsec_tunnel_param_t));
	tunnel.type = ODP_IPSEC_TUNNEL_IPV4;
	tunnel.ipv4.src_addr = &src;
	tunnel.ipv4.dst_addr = &dst;
	tunnel.ipv4.ttl = 64;

	ipsec_sa_param_fill(&param,
			    false, true, 123, &tunnel,
			    ODP_CIPHER_ALG_NULL, NULL,
			    ODP_AUTH_ALG_SHA256_HMAC, &key_5a_256,
			    NULL, NULL);

	sa = odp_ipsec_sa_create(&param);

	CU_ASSERT_NOT_EQUAL_FATAL(ODP_IPSEC_SA_INVALID, sa);

	ipsec_test_part test = {
		.pkt_in = &pkt_ipv4_icmp_0,
		.num_pkt = 1,
		.out = {
			{ .status.warn.all = 0,
			  .status.error.all = 0,
			  .pkt_res = &pkt_ipv4_icmp_0_ah_tun_ipv4_sha256_1 },
		},
	};

	ipsec_check_out_one(&test, sa);

	ipsec_sa_destroy(sa);
}

static void test_out_ipv4_ah_sha256_tun_ipv6(void)
{
	odp_ipsec_tunnel_param_t tunnel;
	odp_ipsec_sa_param_t param;
	odp_ipsec_sa_t sa;
	uint8_t src[16] = {
		0x20, 0x01, 0x0d, 0xb8, 0x00, 0x00, 0x00, 0x00,
		0x02, 0x11, 0x43, 0xff, 0xfe, 0x4a, 0xd7, 0x0a,
	};
	uint8_t dst[16] = {
		0x20, 0x01, 0x0d, 0xb8, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x16,
	};

	memset(&tunnel, 0, sizeof(odp_ipsec_tunnel_param_t));
	tunnel.type = ODP_IPSEC_TUNNEL_IPV6;
	tunnel.ipv6.src_addr = src;
	tunnel.ipv6.dst_addr = dst;
	tunnel.ipv6.hlimit = 64;

	ipsec_sa_param_fill(&param,
			    false, true, 123, &tunnel,
			    ODP_CIPHER_ALG_NULL, NULL,
			    ODP_AUTH_ALG_SHA256_HMAC, &key_5a_256,
			    NULL, NULL);

	sa = odp_ipsec_sa_create(&param);

	CU_ASSERT_NOT_EQUAL_FATAL(ODP_IPSEC_SA_INVALID, sa);

	ipsec_test_part test = {
		.pkt_in = &pkt_ipv4_icmp_0,
		.num_pkt = 1,
		.out = {
			{ .status.warn.all = 0,
			  .status.error.all = 0,
			  .pkt_res = &pkt_ipv4_icmp_0_ah_tun_ipv6_sha256_1 },
		},
	};

	ipsec_check_out_one(&test, sa);

	ipsec_sa_destroy(sa);
}

static void test_out_ipv4_esp_null_sha256(void)
{
	odp_ipsec_sa_param_t param;
	odp_ipsec_sa_t sa;

	ipsec_sa_param_fill(&param,
			    false, false, 123, NULL,
			    ODP_CIPHER_ALG_NULL, NULL,
			    ODP_AUTH_ALG_SHA256_HMAC, &key_5a_256,
			    NULL, NULL);

	sa = odp_ipsec_sa_create(&param);

	CU_ASSERT_NOT_EQUAL_FATAL(ODP_IPSEC_SA_INVALID, sa);

	ipsec_test_part test = {
		.pkt_in = &pkt_ipv4_icmp_0,
		.num_pkt = 1,
		.out = {
			{ .status.warn.all = 0,
			  .status.error.all = 0,
			  .pkt_res = &pkt_ipv4_icmp_0_esp_null_sha256_1 },
		},
	};

	ipsec_check_out_one(&test, sa);

	ipsec_sa_destroy(sa);
}

static void test_out_ipv4_esp_null_sha256_tun_ipv4(void)
{
	odp_ipsec_tunnel_param_t tunnel;
	odp_ipsec_sa_param_t param;
	odp_ipsec_sa_t sa;
	uint32_t src = IPV4ADDR(10, 0, 111, 2);
	uint32_t dst = IPV4ADDR(10, 0, 222, 2);

	memset(&tunnel, 0, sizeof(odp_ipsec_tunnel_param_t));
	tunnel.type = ODP_IPSEC_TUNNEL_IPV4;
	tunnel.ipv4.src_addr = &src;
	tunnel.ipv4.dst_addr = &dst;
	tunnel.ipv4.ttl = 64;

	ipsec_sa_param_fill(&param,
			    false, false, 123, &tunnel,
			    ODP_CIPHER_ALG_NULL, NULL,
			    ODP_AUTH_ALG_SHA256_HMAC, &key_5a_256,
			    NULL, NULL);

	sa = odp_ipsec_sa_create(&param);

	CU_ASSERT_NOT_EQUAL_FATAL(ODP_IPSEC_SA_INVALID, sa);

	ipsec_test_part test = {
		.pkt_in = &pkt_ipv4_icmp_0,
		.num_pkt = 1,
		.out = {
			{ .status.warn.all = 0,
			  .status.error.all = 0,
			  .pkt_res =
				  &pkt_ipv4_icmp_0_esp_tun_ipv4_null_sha256_1 },
		},
	};

	ipsec_check_out_one(&test, sa);

	ipsec_sa_destroy(sa);
}

static void test_out_ipv4_esp_null_sha256_tun_ipv6(void)
{
	odp_ipsec_tunnel_param_t tunnel;
	odp_ipsec_sa_param_t param;
	odp_ipsec_sa_t sa;
	uint8_t src[16] = {
		0x20, 0x01, 0x0d, 0xb8, 0x00, 0x00, 0x00, 0x00,
		0x02, 0x11, 0x43, 0xff, 0xfe, 0x4a, 0xd7, 0x0a,
	};
	uint8_t dst[16] = {
		0x20, 0x01, 0x0d, 0xb8, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x16,
	};

	memset(&tunnel, 0, sizeof(odp_ipsec_tunnel_param_t));
	tunnel.type = ODP_IPSEC_TUNNEL_IPV6;
	tunnel.ipv6.src_addr = src;
	tunnel.ipv6.dst_addr = dst;
	tunnel.ipv6.hlimit = 64;

	ipsec_sa_param_fill(&param,
			    false, false, 123, &tunnel,
			    ODP_CIPHER_ALG_NULL, NULL,
			    ODP_AUTH_ALG_SHA256_HMAC, &key_5a_256,
			    NULL, NULL);

	sa = odp_ipsec_sa_create(&param);

	CU_ASSERT_NOT_EQUAL_FATAL(ODP_IPSEC_SA_INVALID, sa);

	ipsec_test_part test = {
		.pkt_in = &pkt_ipv4_icmp_0,
		.num_pkt = 1,
		.out = {
			{ .status.warn.all = 0,
			  .status.error.all = 0,
			  .pkt_res =
				  &pkt_ipv4_icmp_0_esp_tun_ipv6_null_sha256_1 },
		},
	};

	ipsec_check_out_one(&test, sa);

	ipsec_sa_destroy(sa);
}

static void test_ipsec_stats_zero_assert(odp_ipsec_stats_t *stats)
{
	CU_ASSERT_EQUAL(stats->success, 0);
	CU_ASSERT_EQUAL(stats->proto_err, 0);
	CU_ASSERT_EQUAL(stats->auth_err, 0);
	CU_ASSERT_EQUAL(stats->antireplay_err, 0);
	CU_ASSERT_EQUAL(stats->alg_err, 0);
	CU_ASSERT_EQUAL(stats->mtu_err, 0);
	CU_ASSERT_EQUAL(stats->hard_exp_bytes_err, 0);
	CU_ASSERT_EQUAL(stats->hard_exp_pkts_err, 0);
}

static void test_ipsec_stats_test_assert(odp_ipsec_stats_t *stats,
					 enum ipsec_test_stats test)
{
	if (test == IPSEC_TEST_STATS_SUCCESS) {
		/* Braces needed by CU macro */
		CU_ASSERT_EQUAL(stats->success, 1);
	} else {
		/* Braces needed by CU macro */
		CU_ASSERT_EQUAL(stats->success, 0);
	}

	if (test == IPSEC_TEST_STATS_PROTO_ERR) {
		/* Braces needed by CU macro */
		CU_ASSERT_EQUAL(stats->proto_err, 1);
	} else {
		/* Braces needed by CU macro */
		CU_ASSERT_EQUAL(stats->proto_err, 0);
	}

	if (test == IPSEC_TEST_STATS_AUTH_ERR) {
		/* Braces needed by CU macro */
		CU_ASSERT_EQUAL(stats->auth_err, 1);
	} else {
		/* Braces needed by CU macro */
		CU_ASSERT_EQUAL(stats->auth_err, 0);
	}

	CU_ASSERT_EQUAL(stats->antireplay_err, 0);
	CU_ASSERT_EQUAL(stats->alg_err, 0);
	CU_ASSERT_EQUAL(stats->mtu_err, 0);
	CU_ASSERT_EQUAL(stats->hard_exp_bytes_err, 0);
	CU_ASSERT_EQUAL(stats->hard_exp_pkts_err, 0);
}

static void test_out_in_common(ipsec_test_flags *flags,
			       odp_cipher_alg_t cipher,
			       const odp_crypto_key_t *cipher_key,
			       odp_auth_alg_t auth,
			       const odp_crypto_key_t *auth_key,
			       const odp_crypto_key_t *cipher_key_extra,
			       const odp_crypto_key_t *auth_key_extra)
{
	odp_ipsec_sa_param_t param;
	odp_ipsec_stats_t stats;
	odp_ipsec_sa_t sa_out;
	odp_ipsec_sa_t sa_in;

	CU_ASSERT_NOT_EQUAL_FATAL(flags, NULL);

	/* ICV won't be generated for NULL AUTH */
	if ((flags->stats == IPSEC_TEST_STATS_AUTH_ERR) &&
	    (auth == ODP_AUTH_ALG_NULL))
		return;

	ipsec_sa_param_fill(&param,
			    false, flags->ah, 123, NULL,
			    cipher, cipher_key,
			    auth, auth_key,
			    cipher_key_extra, auth_key_extra);

	sa_out = odp_ipsec_sa_create(&param);

	CU_ASSERT_NOT_EQUAL_FATAL(ODP_IPSEC_SA_INVALID, sa_out);

	ipsec_sa_param_fill(&param,
			    true, flags->ah, 123, NULL,
			    cipher, cipher_key,
			    auth, auth_key,
			    cipher_key_extra, auth_key_extra);

	sa_in = odp_ipsec_sa_create(&param);

	CU_ASSERT_NOT_EQUAL_FATAL(ODP_IPSEC_SA_INVALID, sa_in);

	ipsec_test_part test = {
		.pkt_in = &pkt_ipv4_icmp_0,
		.num_pkt = 1,
		.out = {
			{ .status.warn.all = 0,
			  .status.error.all = 0,
			  .l3_type = ODP_PROTO_L3_TYPE_IPV4,
			  .l4_type = ODP_PROTO_L4_TYPE_ICMPV4,
			  .pkt_res = &pkt_ipv4_icmp_0 },
		},
		.in = {
			{ .status.warn.all = 0,
			  .status.error.all = 0,
			  .l3_type = ODP_PROTO_L3_TYPE_IPV4,
			  .l4_type = ODP_PROTO_L4_TYPE_ICMPV4,
			  .pkt_res = &pkt_ipv4_icmp_0 },
		},
	};

	test.flags = *flags;

	if (flags->stats == IPSEC_TEST_STATS_PROTO_ERR)
		test.in[0].status.error.proto = 1;
	if (flags->stats == IPSEC_TEST_STATS_AUTH_ERR)
		test.in[0].status.error.auth = 1;

	if (flags->stats != IPSEC_TEST_STATS_NONE) {
		CU_ASSERT_EQUAL(odp_ipsec_stats(sa_out, &stats), 0);
		test_ipsec_stats_zero_assert(&stats);
		CU_ASSERT_EQUAL(odp_ipsec_stats(sa_in, &stats), 0);
		test_ipsec_stats_zero_assert(&stats);
	}

	ipsec_check_out_in_one(&test, sa_out, sa_in);

	if (flags->stats == IPSEC_TEST_STATS_SUCCESS) {
		CU_ASSERT_EQUAL(odp_ipsec_stats(sa_in, &stats), 0);
		test_ipsec_stats_test_assert(&stats, flags->stats);
	}

	if (flags->stats != IPSEC_TEST_STATS_NONE) {
		/* All stats tests have outbound operation success and inbound
		 * varying.
		 */
		CU_ASSERT_EQUAL(odp_ipsec_stats(sa_out, &stats), 0);
		test_ipsec_stats_test_assert(&stats, IPSEC_TEST_STATS_SUCCESS);

		CU_ASSERT_EQUAL(odp_ipsec_stats(sa_in, &stats), 0);
		test_ipsec_stats_test_assert(&stats, flags->stats);
	}

	ipsec_sa_destroy(sa_out);
	ipsec_sa_destroy(sa_in);
}

static void test_esp_out_in(struct cipher_param *cipher,
			    struct auth_param *auth,
			    ipsec_test_flags *flags)
{
	int cipher_keylen = cipher->key ? 8 * cipher->key->length : 0;
	int auth_keylen = auth->key ? 8 * auth->key->length : 0;

	if (ipsec_check_esp(cipher->algo, cipher_keylen,
			    auth->algo, auth_keylen) != ODP_TEST_ACTIVE)
		return;

	if (flags->display_algo)
		printf("\n    %s (keylen %d) %s (keylen %d) ",
		       cipher->name, cipher_keylen, auth->name, auth_keylen);

	test_out_in_common(flags, cipher->algo, cipher->key,
			   auth->algo, auth->key,
			   cipher->key_extra, auth->key_extra);
}

static void test_esp_out_in_all(ipsec_test_flags *flags)
{
	uint32_t c;
	uint32_t a;

	flags->ah = false;

	for (c = 0; c < ARRAY_SIZE(ciphers); c++)
		for (a = 0; a < ARRAY_SIZE(auths); a++)
			test_esp_out_in(&ciphers[c], &auths[a], flags);

	for (c = 0; c < ARRAY_SIZE(cipher_auth_comb); c++)
		test_esp_out_in(&cipher_auth_comb[c].cipher,
				&cipher_auth_comb[c].auth,
				flags);
}

/*
 * Test ESP output followed by input with all combinations of ciphers and
 * integrity algorithms.
 */
static void test_esp_out_in_all_basic(void)
{
	ipsec_test_flags flags;

	memset(&flags, 0, sizeof(flags));
	flags.display_algo = true;

	test_esp_out_in_all(&flags);

	printf("\n  ");
}

static void test_ah_out_in(struct auth_param *auth)
{
	int auth_keylen = auth->key ? 8 * auth->key->length : 0;
	ipsec_test_flags flags;

	if (ipsec_check_ah(auth->algo, auth_keylen) != ODP_TEST_ACTIVE)
		return;

	printf("\n    %s (keylen %d) ", auth->name, auth_keylen);

	memset(&flags, 0, sizeof(flags));
	flags.ah = true;

	test_out_in_common(&flags, ODP_CIPHER_ALG_NULL, NULL,
			   auth->algo, auth->key,
			   NULL, auth->key_extra);
}

static void test_ah_out_in_all(void)
{
	uint32_t a;

	for (a = 0; a < ARRAY_SIZE(auths); a++)
		test_ah_out_in(&auths[a]);
	printf("\n  ");
}

static void test_out_ipv4_esp_udp_null_sha256(void)
{
	odp_ipsec_sa_param_t param;
	odp_ipsec_sa_t sa;

	ipsec_sa_param_fill(&param,
			    false, false, 123, NULL,
			    ODP_CIPHER_ALG_NULL, NULL,
			    ODP_AUTH_ALG_SHA256_HMAC, &key_5a_256,
			    NULL, NULL);
	param.opt.udp_encap = 1;

	sa = odp_ipsec_sa_create(&param);

	CU_ASSERT_NOT_EQUAL_FATAL(ODP_IPSEC_SA_INVALID, sa);

	ipsec_test_part test = {
		.pkt_in = &pkt_ipv4_icmp_0,
		.num_pkt = 1,
		.out = {
			{ .status.warn.all = 0,
			  .status.error.all = 0,
			  .pkt_res = &pkt_ipv4_icmp_0_esp_udp_null_sha256_1 },
		},
	};

	ipsec_check_out_one(&test, sa);

	ipsec_sa_destroy(sa);
}

static void test_out_ipv4_ah_aes_gmac_128(void)
{
	ipsec_test_flags flags;

	memset(&flags, 0, sizeof(flags));
	flags.ah = true;

	test_out_in_common(&flags, ODP_CIPHER_ALG_NULL, NULL,
			   ODP_AUTH_ALG_AES_GMAC, &key_a5_128,
			   NULL, &key_mcgrew_gcm_salt_2);
}

static void test_out_ipv4_ah_aes_gmac_192(void)
{
	ipsec_test_flags flags;

	memset(&flags, 0, sizeof(flags));
	flags.ah = true;

	test_out_in_common(&flags, ODP_CIPHER_ALG_NULL, NULL,
			   ODP_AUTH_ALG_AES_GMAC, &key_a5_192,
			   NULL, &key_mcgrew_gcm_salt_2);
}

static void test_out_ipv4_ah_aes_gmac_256(void)
{
	ipsec_test_flags flags;

	memset(&flags, 0, sizeof(flags));
	flags.ah = true;

	test_out_in_common(&flags, ODP_CIPHER_ALG_NULL, NULL,
			   ODP_AUTH_ALG_AES_GMAC, &key_a5_256,
			   NULL, &key_mcgrew_gcm_salt_2);
}

static void test_out_ipv4_ah_sha256_frag_check(void)
{
	odp_ipsec_sa_param_t param;
	odp_ipsec_sa_t sa;
	ipsec_test_part test;
	ipsec_test_part test2;

	memset(&test, 0, sizeof(ipsec_test_part));
	memset(&test2, 0, sizeof(ipsec_test_part));

	ipsec_sa_param_fill(&param,
			    false, true, 123, NULL,
			    ODP_CIPHER_ALG_NULL, NULL,
			    ODP_AUTH_ALG_SHA256_HMAC, &key_5a_256,
			    NULL, NULL);
	param.outbound.frag_mode = ODP_IPSEC_FRAG_CHECK;
	param.outbound.mtu = 100;

	sa = odp_ipsec_sa_create(&param);

	CU_ASSERT_NOT_EQUAL_FATAL(ODP_IPSEC_SA_INVALID, sa);

	test.pkt_in = &pkt_ipv4_icmp_0;
	test.num_pkt = 1;
	test.out[0].status.error.mtu = 1;

	test2.pkt_in = &pkt_ipv4_icmp_0;
	test2.num_opt = 1;
	test2.opt.flag.frag_mode = 1;
	test2.opt.frag_mode = ODP_IPSEC_FRAG_DISABLED;
	test2.num_pkt = 1;
	test2.out[0].pkt_res = &pkt_ipv4_icmp_0_ah_sha256_1;

	ipsec_check_out_one(&test, sa);

	ipsec_check_out_one(&test2, sa);

	ipsec_sa_destroy(sa);
}

static void test_out_ipv4_ah_sha256_frag_check_2(void)
{
	odp_ipsec_sa_param_t param;
	odp_ipsec_sa_t sa;
	ipsec_test_part test;

	memset(&test, 0, sizeof(ipsec_test_part));

	ipsec_sa_param_fill(&param,
			    false, true, 123, NULL,
			    ODP_CIPHER_ALG_NULL, NULL,
			    ODP_AUTH_ALG_SHA256_HMAC, &key_5a_256,
			    NULL, NULL);
	param.outbound.frag_mode = ODP_IPSEC_FRAG_CHECK;
	param.outbound.mtu = 100;

	sa = odp_ipsec_sa_create(&param);

	CU_ASSERT_NOT_EQUAL_FATAL(ODP_IPSEC_SA_INVALID, sa);

	test.pkt_in = &pkt_ipv4_icmp_0;
	test.num_pkt = 1;
	test.out[0].status.error.mtu = 1;

	ipsec_test_part test2 = {
		.pkt_in = &pkt_ipv4_icmp_0,
		.num_pkt = 1,
		.out = {
			{ .status.warn.all = 0,
			  .status.error.all = 0,
			  .pkt_res = &pkt_ipv4_icmp_0_ah_sha256_1 },
		},
	};

	ipsec_check_out_one(&test, sa);

	odp_ipsec_sa_mtu_update(sa, 256);

	ipsec_check_out_one(&test2, sa);

	ipsec_sa_destroy(sa);
}

static void test_out_ipv4_esp_null_sha256_frag_check(void)
{
	odp_ipsec_sa_param_t param;
	odp_ipsec_sa_t sa;
	ipsec_test_part test;
	ipsec_test_part test2;

	memset(&test, 0, sizeof(ipsec_test_part));
	memset(&test2, 0, sizeof(ipsec_test_part));

	ipsec_sa_param_fill(&param,
			    false, false, 123, NULL,
			    ODP_CIPHER_ALG_NULL, NULL,
			    ODP_AUTH_ALG_SHA256_HMAC, &key_5a_256,
			    NULL, NULL);

	param.outbound.frag_mode = ODP_IPSEC_FRAG_CHECK;
	param.outbound.mtu = 100;

	sa = odp_ipsec_sa_create(&param);

	CU_ASSERT_NOT_EQUAL_FATAL(ODP_IPSEC_SA_INVALID, sa);

	test.pkt_in = &pkt_ipv4_icmp_0;
	test.num_pkt = 1;
	test.out[0].status.error.mtu = 1;

	test2.pkt_in = &pkt_ipv4_icmp_0;
	test2.num_opt = 1;
	test2.opt.flag.frag_mode = 1;
	test2.opt.frag_mode = ODP_IPSEC_FRAG_DISABLED;
	test2.num_pkt = 1;
	test2.out[0].pkt_res = &pkt_ipv4_icmp_0_esp_null_sha256_1;

	ipsec_check_out_one(&test, sa);

	ipsec_check_out_one(&test2, sa);

	ipsec_sa_destroy(sa);
}

static void test_out_ipv4_esp_null_sha256_frag_check_2(void)
{
	odp_ipsec_sa_param_t param;
	odp_ipsec_sa_t sa;
	ipsec_test_part test;

	memset(&test, 0, sizeof(ipsec_test_part));

	ipsec_sa_param_fill(&param,
			    false, false, 123, NULL,
			    ODP_CIPHER_ALG_NULL, NULL,
			    ODP_AUTH_ALG_SHA256_HMAC, &key_5a_256,
			    NULL, NULL);

	param.outbound.frag_mode = ODP_IPSEC_FRAG_CHECK;
	param.outbound.mtu = 100;

	sa = odp_ipsec_sa_create(&param);

	CU_ASSERT_NOT_EQUAL_FATAL(ODP_IPSEC_SA_INVALID, sa);

	test.pkt_in = &pkt_ipv4_icmp_0;
	test.num_pkt = 1;
	test.out[0].status.error.mtu = 1;

	ipsec_test_part test2 = {
		.pkt_in = &pkt_ipv4_icmp_0,
		.num_pkt = 1,
		.out = {
			{ .status.warn.all = 0,
			  .status.error.all = 0,
			  .pkt_res = &pkt_ipv4_icmp_0_esp_null_sha256_1 },
		},
	};

	ipsec_check_out_one(&test, sa);

	odp_ipsec_sa_mtu_update(sa, 256);

	ipsec_check_out_one(&test2, sa);

	ipsec_sa_destroy(sa);
}

static void test_out_ipv6_ah_sha256(void)
{
	odp_ipsec_sa_param_t param;
	odp_ipsec_sa_t sa;

	ipsec_sa_param_fill(&param,
			    false, true, 123, NULL,
			    ODP_CIPHER_ALG_NULL, NULL,
			    ODP_AUTH_ALG_SHA256_HMAC, &key_5a_256,
			    NULL, NULL);

	sa = odp_ipsec_sa_create(&param);

	CU_ASSERT_NOT_EQUAL_FATAL(ODP_IPSEC_SA_INVALID, sa);

	ipsec_test_part test = {
		.pkt_in = &pkt_ipv6_icmp_0,
		.num_pkt = 1,
		.out = {
			{ .status.warn.all = 0,
			  .status.error.all = 0,
			  .pkt_res = &pkt_ipv6_icmp_0_ah_sha256_1 },
		},
	};

	ipsec_check_out_one(&test, sa);

	ipsec_sa_destroy(sa);
}

static void test_out_ipv6_ah_sha256_tun_ipv4(void)
{
	odp_ipsec_tunnel_param_t tunnel;
	odp_ipsec_sa_param_t param;
	odp_ipsec_sa_t sa;
	uint32_t src = IPV4ADDR(10, 0, 111, 2);
	uint32_t dst = IPV4ADDR(10, 0, 222, 2);

	memset(&tunnel, 0, sizeof(odp_ipsec_tunnel_param_t));
	tunnel.type = ODP_IPSEC_TUNNEL_IPV4;
	tunnel.ipv4.src_addr = &src;
	tunnel.ipv4.dst_addr = &dst;
	tunnel.ipv4.ttl = 64;

	ipsec_sa_param_fill(&param,
			    false, true, 123, &tunnel,
			    ODP_CIPHER_ALG_NULL, NULL,
			    ODP_AUTH_ALG_SHA256_HMAC, &key_5a_256,
			    NULL, NULL);

	sa = odp_ipsec_sa_create(&param);

	CU_ASSERT_NOT_EQUAL_FATAL(ODP_IPSEC_SA_INVALID, sa);

	ipsec_test_part test = {
		.pkt_in = &pkt_ipv6_icmp_0,
		.num_pkt = 1,
		.out = {
			{ .status.warn.all = 0,
			  .status.error.all = 0,
			  .pkt_res = &pkt_ipv6_icmp_0_ah_tun_ipv4_sha256_1 },
		},
	};

	ipsec_check_out_one(&test, sa);

	ipsec_sa_destroy(sa);
}

static void test_out_ipv6_ah_sha256_tun_ipv6(void)
{
	odp_ipsec_tunnel_param_t tunnel;
	odp_ipsec_sa_param_t param;
	odp_ipsec_sa_t sa;
	uint8_t src[16] = {
		0x20, 0x01, 0x0d, 0xb8, 0x00, 0x00, 0x00, 0x00,
		0x02, 0x11, 0x43, 0xff, 0xfe, 0x4a, 0xd7, 0x0a,
	};
	uint8_t dst[16] = {
		0x20, 0x01, 0x0d, 0xb8, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x16,
	};

	memset(&tunnel, 0, sizeof(odp_ipsec_tunnel_param_t));
	tunnel.type = ODP_IPSEC_TUNNEL_IPV6;
	tunnel.ipv6.src_addr = src;
	tunnel.ipv6.dst_addr = dst;
	tunnel.ipv6.hlimit = 64;

	ipsec_sa_param_fill(&param,
			    false, true, 123, &tunnel,
			    ODP_CIPHER_ALG_NULL, NULL,
			    ODP_AUTH_ALG_SHA256_HMAC, &key_5a_256,
			    NULL, NULL);

	sa = odp_ipsec_sa_create(&param);

	CU_ASSERT_NOT_EQUAL_FATAL(ODP_IPSEC_SA_INVALID, sa);

	ipsec_test_part test = {
		.pkt_in = &pkt_ipv6_icmp_0,
		.num_pkt = 1,
		.out = {
			{ .status.warn.all = 0,
			  .status.error.all = 0,
			  .pkt_res = &pkt_ipv6_icmp_0_ah_tun_ipv6_sha256_1 },
		},
	};

	ipsec_check_out_one(&test, sa);

	ipsec_sa_destroy(sa);
}

static void test_out_ipv6_esp_null_sha256(void)
{
	odp_ipsec_sa_param_t param;
	odp_ipsec_sa_t sa;

	ipsec_sa_param_fill(&param,
			    false, false, 123, NULL,
			    ODP_CIPHER_ALG_NULL, NULL,
			    ODP_AUTH_ALG_SHA256_HMAC, &key_5a_256,
			    NULL, NULL);

	sa = odp_ipsec_sa_create(&param);

	CU_ASSERT_NOT_EQUAL_FATAL(ODP_IPSEC_SA_INVALID, sa);

	ipsec_test_part test = {
		.pkt_in = &pkt_ipv6_icmp_0,
		.num_pkt = 1,
		.out = {
			{ .status.warn.all = 0,
			  .status.error.all = 0,
			  .pkt_res = &pkt_ipv6_icmp_0_esp_null_sha256_1 },
		},
	};

	ipsec_check_out_one(&test, sa);

	ipsec_sa_destroy(sa);
}

static void test_out_ipv6_esp_null_sha256_tun_ipv4(void)
{
	odp_ipsec_tunnel_param_t tunnel;
	odp_ipsec_sa_param_t param;
	odp_ipsec_sa_t sa;
	uint32_t src = IPV4ADDR(10, 0, 111, 2);
	uint32_t dst = IPV4ADDR(10, 0, 222, 2);

	memset(&tunnel, 0, sizeof(odp_ipsec_tunnel_param_t));
	tunnel.type = ODP_IPSEC_TUNNEL_IPV4;
	tunnel.ipv4.src_addr = &src;
	tunnel.ipv4.dst_addr = &dst;
	tunnel.ipv4.ttl = 64;

	ipsec_sa_param_fill(&param,
			    false, false, 123, &tunnel,
			    ODP_CIPHER_ALG_NULL, NULL,
			    ODP_AUTH_ALG_SHA256_HMAC, &key_5a_256,
			    NULL, NULL);

	sa = odp_ipsec_sa_create(&param);

	CU_ASSERT_NOT_EQUAL_FATAL(ODP_IPSEC_SA_INVALID, sa);

	ipsec_test_part test = {
		.pkt_in = &pkt_ipv6_icmp_0,
		.num_pkt = 1,
		.out = {
			{ .status.warn.all = 0,
			  .status.error.all = 0,
			  .pkt_res =
				  &pkt_ipv6_icmp_0_esp_tun_ipv4_null_sha256_1 },
		},
	};

	ipsec_check_out_one(&test, sa);

	ipsec_sa_destroy(sa);
}

static void test_out_ipv6_esp_null_sha256_tun_ipv6(void)
{
	odp_ipsec_tunnel_param_t tunnel;
	odp_ipsec_sa_param_t param;
	odp_ipsec_sa_t sa;
	uint8_t src[16] = {
		0x20, 0x01, 0x0d, 0xb8, 0x00, 0x00, 0x00, 0x00,
		0x02, 0x11, 0x43, 0xff, 0xfe, 0x4a, 0xd7, 0x0a,
	};
	uint8_t dst[16] = {
		0x20, 0x01, 0x0d, 0xb8, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x16,
	};

	memset(&tunnel, 0, sizeof(odp_ipsec_tunnel_param_t));
	tunnel.type = ODP_IPSEC_TUNNEL_IPV6;
	tunnel.ipv6.src_addr = &src;
	tunnel.ipv6.dst_addr = &dst;
	tunnel.ipv6.hlimit = 64;

	ipsec_sa_param_fill(&param,
			    false, false, 123, &tunnel,
			    ODP_CIPHER_ALG_NULL, NULL,
			    ODP_AUTH_ALG_SHA256_HMAC, &key_5a_256,
			    NULL, NULL);

	sa = odp_ipsec_sa_create(&param);

	CU_ASSERT_NOT_EQUAL_FATAL(ODP_IPSEC_SA_INVALID, sa);

	ipsec_test_part test = {
		.pkt_in = &pkt_ipv6_icmp_0,
		.num_pkt = 1,
		.out = {
			{ .status.warn.all = 0,
			  .status.error.all = 0,
			  .pkt_res =
				  &pkt_ipv6_icmp_0_esp_tun_ipv6_null_sha256_1 },
		},
	};

	ipsec_check_out_one(&test, sa);

	ipsec_sa_destroy(sa);
}

static void test_out_ipv6_esp_udp_null_sha256(void)
{
	odp_ipsec_sa_param_t param;
	odp_ipsec_sa_t sa;

	ipsec_sa_param_fill(&param,
			    false, false, 123, NULL,
			    ODP_CIPHER_ALG_NULL, NULL,
			    ODP_AUTH_ALG_SHA256_HMAC, &key_5a_256,
			    NULL, NULL);
	param.opt.udp_encap = 1;

	sa = odp_ipsec_sa_create(&param);

	CU_ASSERT_NOT_EQUAL_FATAL(ODP_IPSEC_SA_INVALID, sa);

	ipsec_test_part test = {
		.pkt_in = &pkt_ipv6_icmp_0,
		.num_pkt = 1,
		.out = {
			{ .status.warn.all = 0,
			  .status.error.all = 0,
			  .pkt_res = &pkt_ipv6_icmp_0_esp_udp_null_sha256_1 },
		},
	};

	ipsec_check_out_one(&test, sa);

	ipsec_sa_destroy(sa);
}

static void test_out_dummy_esp_null_sha256_tun_ipv4(void)
{
	odp_ipsec_tunnel_param_t tunnel;
	odp_ipsec_sa_param_t param;
	odp_ipsec_sa_t sa;
	odp_ipsec_sa_t sa2;
	uint32_t src = IPV4ADDR(10, 0, 111, 2);
	uint32_t dst = IPV4ADDR(10, 0, 222, 2);
	ipsec_test_part test;
	ipsec_test_part test_empty;

	memset(&test, 0, sizeof(ipsec_test_part));
	memset(&test_empty, 0, sizeof(ipsec_test_part));

	memset(&tunnel, 0, sizeof(odp_ipsec_tunnel_param_t));
	tunnel.type = ODP_IPSEC_TUNNEL_IPV4;
	tunnel.ipv4.src_addr = &src;
	tunnel.ipv4.dst_addr = &dst;
	tunnel.ipv4.ttl = 64;

	/* This test will not work properly inbound inline mode.
	 * Packet might be dropped and we will not check for that. */
	if (suite_context.inbound_op_mode == ODP_IPSEC_OP_MODE_INLINE)
		return;

	ipsec_sa_param_fill(&param,
			    false, false, 123, &tunnel,
			    ODP_CIPHER_ALG_NULL, NULL,
			    ODP_AUTH_ALG_SHA256_HMAC, &key_5a_256,
			    NULL, NULL);

	sa = odp_ipsec_sa_create(&param);

	CU_ASSERT_NOT_EQUAL_FATAL(ODP_IPSEC_SA_INVALID, sa);

	ipsec_sa_param_fill(&param,
			    true, false, 123, &tunnel,
			    ODP_CIPHER_ALG_NULL, NULL,
			    ODP_AUTH_ALG_SHA256_HMAC, &key_5a_256,
			    NULL, NULL);

	sa2 = odp_ipsec_sa_create(&param);

	CU_ASSERT_NOT_EQUAL_FATAL(ODP_IPSEC_SA_INVALID, sa2);

	test.pkt_in = &pkt_test_nodata;
	test.num_opt = 1;
	test.opt .flag.tfc_dummy = 1;
	test.opt.tfc_pad_len = 16;
	test.num_pkt = 1;
	test.out[0].l3_type = ODP_PROTO_L3_TYPE_IPV4;
	test.out[0].l4_type = ODP_PROTO_L4_TYPE_NO_NEXT;

	test_empty.pkt_in = &pkt_test_empty;
	test_empty.num_opt = 1;
	test_empty.opt.flag.tfc_dummy = 1;
	test_empty.opt.tfc_pad_len = 16;
	test_empty.num_pkt = 1;
	test_empty.out[0].l3_type = ODP_PROTO_L3_TYPE_IPV4;
	test_empty.out[0].l4_type = ODP_PROTO_L4_TYPE_NO_NEXT;

	ipsec_check_out_in_one(&test, sa, sa2);
	ipsec_check_out_in_one(&test_empty, sa, sa2);

	ipsec_sa_destroy(sa2);
	ipsec_sa_destroy(sa);
}

static void test_out_dummy_esp_null_sha256_tun_ipv6(void)
{
	odp_ipsec_tunnel_param_t tunnel;
	odp_ipsec_sa_param_t param;
	odp_ipsec_sa_t sa;
	odp_ipsec_sa_t sa2;
	ipsec_test_part test;
	ipsec_test_part test_empty;
	uint8_t src[16] = {
		0x20, 0x01, 0x0d, 0xb8, 0x00, 0x00, 0x00, 0x00,
		0x02, 0x11, 0x43, 0xff, 0xfe, 0x4a, 0xd7, 0x0a,
	};
	uint8_t dst[16] = {
		0x20, 0x01, 0x0d, 0xb8, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x16,
	};

	memset(&test, 0, sizeof(ipsec_test_part));
	memset(&test_empty, 0, sizeof(ipsec_test_part));

	memset(&tunnel, 0, sizeof(odp_ipsec_tunnel_param_t));
	tunnel.type = ODP_IPSEC_TUNNEL_IPV6;
	tunnel.ipv6.src_addr = src;
	tunnel.ipv6.dst_addr = dst;
	tunnel.ipv6.hlimit = 64;

	/* This test will not work properly inbound inline mode.
	 * Packet might be dropped and we will not check for that. */
	if (suite_context.inbound_op_mode == ODP_IPSEC_OP_MODE_INLINE)
		return;

	ipsec_sa_param_fill(&param,
			    false, false, 123, &tunnel,
			    ODP_CIPHER_ALG_NULL, NULL,
			    ODP_AUTH_ALG_SHA256_HMAC, &key_5a_256,
			    NULL, NULL);

	sa = odp_ipsec_sa_create(&param);

	CU_ASSERT_NOT_EQUAL_FATAL(ODP_IPSEC_SA_INVALID, sa);

	ipsec_sa_param_fill(&param,
			    true, false, 123, &tunnel,
			    ODP_CIPHER_ALG_NULL, NULL,
			    ODP_AUTH_ALG_SHA256_HMAC, &key_5a_256,
			    NULL, NULL);

	sa2 = odp_ipsec_sa_create(&param);

	CU_ASSERT_NOT_EQUAL_FATAL(ODP_IPSEC_SA_INVALID, sa2);

	test.pkt_in = &pkt_test_nodata;
	test.num_opt = 1;
	test.opt .flag.tfc_dummy = 1;
	test.opt.tfc_pad_len = 16;
	test.num_pkt = 1;
	test.out[0].l3_type = ODP_PROTO_L3_TYPE_IPV4;
	test.out[0].l4_type = ODP_PROTO_L4_TYPE_NO_NEXT;

	test_empty.pkt_in = &pkt_test_empty;
	test_empty.num_opt = 1;
	test_empty.opt.flag.tfc_dummy = 1;
	test_empty.opt.tfc_pad_len = 16;
	test_empty.num_pkt = 1;
	test_empty.out[0].l3_type = ODP_PROTO_L3_TYPE_IPV4;
	test_empty.out[0].l4_type = ODP_PROTO_L4_TYPE_NO_NEXT;

	ipsec_check_out_in_one(&test, sa, sa2);
	ipsec_check_out_in_one(&test_empty, sa, sa2);

	ipsec_sa_destroy(sa2);
	ipsec_sa_destroy(sa);
}

static void test_out_ipv4_udp_esp_null_sha256(void)
{
	odp_ipsec_sa_param_t param;
	odp_ipsec_sa_t sa;

	ipsec_sa_param_fill(&param,
			    false, false, 123, NULL,
			    ODP_CIPHER_ALG_NULL, NULL,
			    ODP_AUTH_ALG_SHA256_HMAC, &key_5a_256,
			    NULL, NULL);

	sa = odp_ipsec_sa_create(&param);

	CU_ASSERT_NOT_EQUAL_FATAL(ODP_IPSEC_SA_INVALID, sa);

	ipsec_test_part test = {
		.pkt_in = &pkt_ipv4_udp,
		.num_pkt = 1,
		.out = {
			{ .status.warn.all = 0,
			  .status.error.all = 0,
			  .pkt_res = &pkt_ipv4_udp_esp_null_sha256 },
		},
	};

	ipsec_check_out_one(&test, sa);

	ipsec_sa_destroy(sa);
}

static void test_sa_info(void)
{
	uint32_t src = IPV4ADDR(10, 0, 111, 2);
	uint32_t dst = IPV4ADDR(10, 0, 222, 2);
	odp_ipsec_tunnel_param_t tunnel_out;
	odp_ipsec_tunnel_param_t tunnel_in;
	odp_ipsec_sa_param_t param_out;
	odp_ipsec_sa_param_t param_in;
	odp_ipsec_sa_info_t info_out;
	odp_ipsec_sa_info_t info_in;
	odp_ipsec_capability_t capa;
	odp_ipsec_sa_t sa_out;
	odp_ipsec_sa_t sa_in;

	CU_ASSERT_EQUAL(0, odp_ipsec_capability(&capa));

	memset(&tunnel_out, 0, sizeof(tunnel_out));
	memset(&tunnel_in, 0, sizeof(tunnel_in));

	tunnel_out.type = ODP_IPSEC_TUNNEL_IPV4;
	tunnel_out.ipv4.src_addr = &src;
	tunnel_out.ipv4.dst_addr = &dst;

	ipsec_sa_param_fill(&param_out,
			    false, false, 123, &tunnel_out,
			    ODP_CIPHER_ALG_AES_CBC, &key_a5_128,
			    ODP_AUTH_ALG_SHA1_HMAC, &key_5a_160,
			    NULL, NULL);

	sa_out = odp_ipsec_sa_create(&param_out);

	CU_ASSERT_NOT_EQUAL_FATAL(ODP_IPSEC_SA_INVALID, sa_out);

	ipsec_sa_param_fill(&param_in,
			    true, false, 123, &tunnel_in,
			    ODP_CIPHER_ALG_AES_CBC, &key_a5_128,
			    ODP_AUTH_ALG_SHA1_HMAC, &key_5a_160,
			    NULL, NULL);

	param_in.inbound.antireplay_ws = 32;
	sa_in = odp_ipsec_sa_create(&param_in);

	memset(&info_out, 0, sizeof(info_out));
	CU_ASSERT_EQUAL_FATAL(0, odp_ipsec_sa_info(sa_out, &info_out));

	CU_ASSERT_EQUAL(info_out.param.dir, param_out.dir);
	CU_ASSERT_EQUAL(info_out.param.proto, param_out.proto);
	CU_ASSERT_EQUAL(info_out.param.mode, param_out.mode);

	CU_ASSERT_EQUAL(info_out.param.crypto.cipher_alg,
			param_out.crypto.cipher_alg);
	CU_ASSERT_EQUAL(info_out.param.crypto.auth_alg,
			param_out.crypto.auth_alg);
	CU_ASSERT_EQUAL(info_out.param.opt.udp_encap, param_out.opt.udp_encap);
	CU_ASSERT_EQUAL(info_out.param.spi, param_out.spi);
	CU_ASSERT_EQUAL(info_out.param.opt.esn, param_out.opt.esn);
	CU_ASSERT_EQUAL(info_out.param.opt.udp_encap, param_out.opt.udp_encap);
	CU_ASSERT_EQUAL(info_out.param.opt.copy_dscp, param_out.opt.copy_dscp);
	CU_ASSERT_EQUAL(info_out.param.opt.copy_flabel, param_out.opt.copy_flabel);
	CU_ASSERT_EQUAL(info_out.param.opt.copy_df, param_out.opt.copy_df);

	CU_ASSERT_EQUAL(ODP_IPSEC_MODE_TUNNEL, info_out.param.mode);

	CU_ASSERT_EQUAL(info_out.param.outbound.tunnel.type,
			param_out.outbound.tunnel.type);
	CU_ASSERT_EQUAL(info_out.param.outbound.tunnel.ipv4.dscp,
			param_out.outbound.tunnel.ipv4.dscp);
	CU_ASSERT_EQUAL(info_out.param.outbound.tunnel.ipv4.df,
			param_out.outbound.tunnel.ipv4.df);
	CU_ASSERT_NOT_EQUAL_FATAL(NULL,
				  info_out.param.outbound.tunnel.ipv4.src_addr);
	CU_ASSERT_EQUAL(0, memcmp(info_out.param.outbound.tunnel.ipv4.src_addr,
				  param_out.outbound.tunnel.ipv4.src_addr,
				  ODP_IPV4_ADDR_SIZE));
	CU_ASSERT_NOT_EQUAL_FATAL(NULL,
				  info_out.param.outbound.tunnel.ipv4.dst_addr);
	CU_ASSERT_EQUAL(0, memcmp(info_out.param.outbound.tunnel.ipv4.dst_addr,
				  param_out.outbound.tunnel.ipv4.dst_addr,
				  ODP_IPV4_ADDR_SIZE));

	CU_ASSERT_EQUAL(info_out.param.lifetime.soft_limit.bytes,
			param_out.lifetime.soft_limit.bytes);
	CU_ASSERT_EQUAL(info_out.param.lifetime.hard_limit.bytes,
			param_out.lifetime.hard_limit.bytes);
	CU_ASSERT_EQUAL(info_out.param.lifetime.soft_limit.packets,
			param_out.lifetime.soft_limit.packets);
	CU_ASSERT_EQUAL(info_out.param.lifetime.hard_limit.packets,
			param_out.lifetime.hard_limit.packets);

	CU_ASSERT_EQUAL(0, info_out.outbound.seq_num);

	memset(&info_in, 0, sizeof(info_in));
	CU_ASSERT_EQUAL_FATAL(0, odp_ipsec_sa_info(sa_in, &info_in));
	CU_ASSERT_EQUAL(0, info_in.inbound.antireplay_window_top);

	ipsec_test_part test = {
		.pkt_in = &pkt_ipv4_icmp_0,
		.num_pkt = 1,
		.out = {
			{ .status.warn.all = 0,
			  .status.error.all = 0,
			  .l3_type = ODP_PROTO_L3_TYPE_IPV4,
			  .l4_type = ODP_PROTO_L4_TYPE_ICMPV4,
			  .pkt_res = &pkt_ipv4_icmp_0 },
		},
	};

	ipsec_check_out_in_one(&test, sa_out, sa_in);

	memset(&info_out, 0, sizeof(info_out));
	CU_ASSERT_EQUAL_FATAL(0, odp_ipsec_sa_info(sa_out, &info_out));
	CU_ASSERT_EQUAL(1, info_out.outbound.seq_num);

	memset(&info_in, 0, sizeof(info_in));
	CU_ASSERT_EQUAL_FATAL(0, odp_ipsec_sa_info(sa_in, &info_in));
	CU_ASSERT_EQUAL(1, info_in.inbound.antireplay_window_top);

	ipsec_sa_destroy(sa_out);
	ipsec_sa_destroy(sa_in);
}

static void ipsec_test_capability(void)
{
	odp_ipsec_capability_t capa;

	CU_ASSERT(odp_ipsec_capability(&capa) == 0);
}

static void test_ipsec_stats(void)
{
	ipsec_test_flags flags;

	memset(&flags, 0, sizeof(flags));

	printf("\n        Stats : success");
	flags.stats = IPSEC_TEST_STATS_SUCCESS;
	test_esp_out_in_all(&flags);

	printf("\n        Stats : proto err");
	flags.stats = IPSEC_TEST_STATS_PROTO_ERR;
	test_esp_out_in_all(&flags);

	printf("\n        Stats : auth err");
	flags.stats = IPSEC_TEST_STATS_AUTH_ERR;
	test_esp_out_in_all(&flags);

	printf("\n  ");
}

odp_testinfo_t ipsec_out_suite[] = {
	ODP_TEST_INFO(ipsec_test_capability),
	ODP_TEST_INFO_CONDITIONAL(test_out_ipv4_ah_sha256,
				  ipsec_check_ah_sha256),
	ODP_TEST_INFO_CONDITIONAL(test_out_ipv4_ah_sha256_tun_ipv4,
				  ipsec_check_ah_sha256),
	ODP_TEST_INFO_CONDITIONAL(test_out_ipv4_ah_sha256_tun_ipv6,
				  ipsec_check_ah_sha256),
	ODP_TEST_INFO_CONDITIONAL(test_out_ipv4_esp_null_sha256,
				  ipsec_check_esp_null_sha256),
	ODP_TEST_INFO_CONDITIONAL(test_out_ipv4_esp_null_sha256_tun_ipv4,
				  ipsec_check_esp_null_sha256),
	ODP_TEST_INFO_CONDITIONAL(test_out_ipv4_esp_null_sha256_tun_ipv6,
				  ipsec_check_esp_null_sha256),
	ODP_TEST_INFO_CONDITIONAL(test_out_ipv4_esp_udp_null_sha256,
				  ipsec_check_esp_null_sha256),
	ODP_TEST_INFO_CONDITIONAL(test_out_ipv4_ah_aes_gmac_128,
				  ipsec_check_ah_aes_gmac_128),
	ODP_TEST_INFO_CONDITIONAL(test_out_ipv4_ah_aes_gmac_192,
				  ipsec_check_ah_aes_gmac_192),
	ODP_TEST_INFO_CONDITIONAL(test_out_ipv4_ah_aes_gmac_256,
				  ipsec_check_ah_aes_gmac_256),
	ODP_TEST_INFO_CONDITIONAL(test_out_ipv4_ah_sha256_frag_check,
				  ipsec_check_ah_sha256),
	ODP_TEST_INFO_CONDITIONAL(test_out_ipv4_ah_sha256_frag_check_2,
				  ipsec_check_ah_sha256),
	ODP_TEST_INFO_CONDITIONAL(test_out_ipv4_esp_null_sha256_frag_check,
				  ipsec_check_esp_null_sha256),
	ODP_TEST_INFO_CONDITIONAL(test_out_ipv4_esp_null_sha256_frag_check_2,
				  ipsec_check_esp_null_sha256),
	ODP_TEST_INFO_CONDITIONAL(test_out_ipv6_ah_sha256,
				  ipsec_check_ah_sha256),
	ODP_TEST_INFO_CONDITIONAL(test_out_ipv6_ah_sha256_tun_ipv4,
				  ipsec_check_ah_sha256),
	ODP_TEST_INFO_CONDITIONAL(test_out_ipv6_ah_sha256_tun_ipv6,
				  ipsec_check_ah_sha256),
	ODP_TEST_INFO_CONDITIONAL(test_out_ipv6_esp_null_sha256,
				  ipsec_check_esp_null_sha256),
	ODP_TEST_INFO_CONDITIONAL(test_out_ipv6_esp_null_sha256_tun_ipv4,
				  ipsec_check_esp_null_sha256),
	ODP_TEST_INFO_CONDITIONAL(test_out_ipv6_esp_null_sha256_tun_ipv6,
				  ipsec_check_esp_null_sha256),
	ODP_TEST_INFO_CONDITIONAL(test_out_ipv6_esp_udp_null_sha256,
				  ipsec_check_esp_null_sha256),
	ODP_TEST_INFO_CONDITIONAL(test_out_dummy_esp_null_sha256_tun_ipv4,
				  ipsec_check_esp_null_sha256),
	ODP_TEST_INFO_CONDITIONAL(test_out_dummy_esp_null_sha256_tun_ipv6,
				  ipsec_check_esp_null_sha256),
	ODP_TEST_INFO_CONDITIONAL(test_out_ipv4_udp_esp_null_sha256,
				  ipsec_check_esp_null_sha256),
	ODP_TEST_INFO_CONDITIONAL(test_sa_info,
				  ipsec_check_esp_aes_cbc_128_sha1),
	ODP_TEST_INFO(test_esp_out_in_all_basic),
	ODP_TEST_INFO(test_ah_out_in_all),
	ODP_TEST_INFO(test_ipsec_stats),
	ODP_TEST_INFO_NULL,
};
