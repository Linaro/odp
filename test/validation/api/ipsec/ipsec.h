/* Copyright (c) 2017-2018, Linaro Limited
 * Copyright (c) 2020, Marvell
 * Copyright (c) 2020, Nokia
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */

#ifndef _ODP_TEST_IPSEC_H_
#define _ODP_TEST_IPSEC_H_

#include <odp_cunit_common.h>

/* test arrays: */
extern odp_testinfo_t ipsec_in_suite[];
extern odp_testinfo_t ipsec_out_suite[];

int ipsec_init(odp_instance_t *inst, odp_ipsec_op_mode_t mode);
int ipsec_term(odp_instance_t inst);
int ipsec_config(odp_instance_t inst);

int ipsec_in_inline_init(void);
int ipsec_out_inline_init(void);

int ipsec_suite_init(void);
int ipsec_in_term(void);
int ipsec_out_term(void);

struct suite_context_s {
	odp_ipsec_op_mode_t inbound_op_mode;
	odp_ipsec_op_mode_t outbound_op_mode;
	odp_pool_t pool;
	odp_queue_t queue;
	odp_pktio_t pktio;
};

extern struct suite_context_s suite_context;

typedef struct {
	uint32_t len;
	uint32_t l2_offset;
	uint32_t l3_offset;
	uint32_t l4_offset;
	uint8_t data[256];
} ipsec_test_packet;

#define _ODP_PROTO_L3_TYPE_UNDEF ((odp_proto_l3_type_t)-1)
#define _ODP_PROTO_L4_TYPE_UNDEF ((odp_proto_l4_type_t)-1)

enum ipsec_test_stats {
	IPSEC_TEST_STATS_NONE = 0,
	IPSEC_TEST_STATS_SUCCESS,
	IPSEC_TEST_STATS_PROTO_ERR,
	IPSEC_TEST_STATS_AUTH_ERR,
};

typedef struct {
	odp_bool_t display_algo;
	odp_bool_t lookup;
	odp_bool_t ah;
	enum ipsec_test_stats stats;
} ipsec_test_flags;

typedef struct {
	const ipsec_test_packet *pkt_in;
	ipsec_test_flags flags;
	int num_opt;
	odp_ipsec_out_opt_t opt;
	int num_pkt;
	struct {
		odp_ipsec_op_status_t status;
		const ipsec_test_packet *pkt_res;
		odp_proto_l3_type_t l3_type;
		odp_proto_l4_type_t l4_type;
	} out[1];
	struct {
		odp_ipsec_op_status_t status;
		const ipsec_test_packet *pkt_res;
		odp_proto_l3_type_t l3_type;
		odp_proto_l4_type_t l4_type;
	} in[1];
} ipsec_test_part;

void ipsec_sa_param_fill(odp_ipsec_sa_param_t *param,
			 odp_bool_t in,
			 odp_bool_t ah,
			 uint32_t spi,
			 odp_ipsec_tunnel_param_t *tun,
			 odp_cipher_alg_t cipher_alg,
			 const odp_crypto_key_t *cipher_key,
			 odp_auth_alg_t auth_alg,
			 const odp_crypto_key_t *auth_key,
			 const odp_crypto_key_t *cipher_key_extra,
			 const odp_crypto_key_t *auth_key_extra);

void ipsec_sa_destroy(odp_ipsec_sa_t sa);
odp_packet_t ipsec_packet(const ipsec_test_packet *itp);
void ipsec_check_in_one(const ipsec_test_part *part, odp_ipsec_sa_t sa);
void ipsec_check_out_one(const ipsec_test_part *part, odp_ipsec_sa_t sa);
void ipsec_check_out_in_one(const ipsec_test_part *part,
			    odp_ipsec_sa_t sa,
			    odp_ipsec_sa_t sa_in);

int ipsec_check(odp_bool_t ah,
		odp_cipher_alg_t cipher,
		uint32_t cipher_bits,
		odp_auth_alg_t auth,
		uint32_t auth_bits);
#define ipsec_check_ah(auth, auth_bits) \
	ipsec_check(true, ODP_CIPHER_ALG_NULL, 0, auth, auth_bits)
#define ipsec_check_esp(cipher, cipher_bits, auth, auth_bits) \
	ipsec_check(false, cipher, cipher_bits, auth, auth_bits)
int ipsec_check_ah_sha256(void);
int ipsec_check_esp_null_sha256(void);
int ipsec_check_esp_aes_cbc_128_null(void);
int ipsec_check_esp_aes_cbc_128_sha1(void);
int ipsec_check_esp_aes_cbc_128_sha256(void);
int ipsec_check_esp_aes_ctr_128_null(void);
int ipsec_check_esp_aes_gcm_128(void);
int ipsec_check_esp_aes_gcm_256(void);
int ipsec_check_ah_aes_gmac_128(void);
int ipsec_check_ah_aes_gmac_192(void);
int ipsec_check_ah_aes_gmac_256(void);
int ipsec_check_esp_null_aes_gmac_128(void);
int ipsec_check_esp_null_aes_gmac_192(void);
int ipsec_check_esp_null_aes_gmac_256(void);
int ipsec_check_esp_chacha20_poly1305(void);

#endif
