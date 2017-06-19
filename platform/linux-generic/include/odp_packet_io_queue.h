/* Copyright (c) 2013, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */


/**
 * @file
 *
 * ODP packet IO - implementation internal
 */

#ifndef ODP_PACKET_IO_QUEUE_H_
#define ODP_PACKET_IO_QUEUE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <odp_buffer_internal.h>
#include <odp_config_internal.h>
#include <odp_queue_if.h>

/** Max nbr of pkts to receive in one burst (keep same as QUEUE_MULTI_MAX) */
#define ODP_PKTIN_QUEUE_MAX_BURST CONFIG_BURST_SIZE
/* pktin_deq_multi() depends on the condition: */
ODP_STATIC_ASSERT(ODP_PKTIN_QUEUE_MAX_BURST >= QUEUE_MULTI_MAX,
		  "ODP_PKTIN_DEQ_MULTI_MAX_ERROR");

int pktin_enqueue(odp_queue_t queue, odp_buffer_hdr_t *buf_hdr);
odp_buffer_hdr_t *pktin_dequeue(odp_queue_t queue);

int pktin_enq_multi(odp_queue_t queue, odp_buffer_hdr_t *buf_hdr[], int num);
int pktin_deq_multi(odp_queue_t queue, odp_buffer_hdr_t *buf_hdr[], int num);


int pktout_enqueue(odp_queue_t queue, odp_buffer_hdr_t *buf_hdr);
odp_buffer_hdr_t *pktout_dequeue(odp_queue_t queue);

int pktout_enq_multi(odp_queue_t queue, odp_buffer_hdr_t *buf_hdr[], int num);
int pktout_deq_multi(odp_queue_t queue, odp_buffer_hdr_t *buf_hdr[], int num);

#ifdef __cplusplus
}
#endif

#endif
