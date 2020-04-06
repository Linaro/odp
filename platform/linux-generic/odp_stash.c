/* Copyright (c) 2020, Nokia
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */

#include <stdio.h>
#include <string.h>

#include <odp/api/ticketlock.h>
#include <odp/api/shared_memory.h>
#include <odp/api/stash.h>
#include <odp/api/plat/strong_types.h>

#include <odp_debug_internal.h>
#include <odp_init_internal.h>
#include <odp_ring_ptr_internal.h>
#include <odp_ring_u32_internal.h>

#define MAX_STASHES   32
#define MAX_RING_SIZE (1024 * 1024)
#define MIN_RING_SIZE 64

typedef struct stash_t {
	char      name[ODP_STASH_NAME_LEN];
	odp_shm_t shm;
	int       index;
	uint32_t  ring_mask;
	uint32_t  obj_size;

	/* Ring header followed by variable sized data (object handles) */
	union {
		struct ODP_ALIGNED_CACHE {
			ring_ptr_t hdr;
			uintptr_t  data[0];
		} ring_ptr;

		struct ODP_ALIGNED_CACHE {
			ring_u32_t hdr;
			uint32_t   data[0];
		} ring_u32;
	};

} stash_t;

typedef struct stash_global_t {
	odp_ticketlock_t  lock;
	odp_shm_t         shm;
	uint8_t           stash_reserved[MAX_STASHES];
	stash_t           *stash[MAX_STASHES];

} stash_global_t;

static stash_global_t *stash_global;

int _odp_stash_init_global(void)
{
	odp_shm_t shm;

	shm = odp_shm_reserve("_odp_stash_table", sizeof(stash_global_t),
			      ODP_CACHE_LINE_SIZE, 0);

	stash_global = odp_shm_addr(shm);

	if (stash_global == NULL) {
		ODP_ERR("SHM reserve of stash global data failed\n");
		return -1;
	}

	memset(stash_global, 0, sizeof(stash_global_t));
	stash_global->shm = shm;
	odp_ticketlock_init(&stash_global->lock);

	return 0;
}

int _odp_stash_term_global(void)
{
	if (stash_global == NULL)
		return 0;

	if (odp_shm_free(stash_global->shm)) {
		ODP_ERR("SHM free failed\n");
		return -1;
	}

	return 0;
}

int odp_stash_capability(odp_stash_capability_t *capa, odp_stash_type_t type)
{
	(void)type;
	memset(capa, 0, sizeof(odp_stash_capability_t));

	capa->max_stashes_any_type = MAX_STASHES;
	capa->max_stashes          = MAX_STASHES;
	capa->max_num_obj          = MAX_RING_SIZE;
	capa->max_obj_size         = sizeof(uintptr_t);

	return 0;
}

void odp_stash_param_init(odp_stash_param_t *param)
{
	memset(param, 0, sizeof(odp_stash_param_t));
	param->type     = ODP_STASH_TYPE_DEFAULT;
	param->put_mode = ODP_STASH_OP_MT;
	param->get_mode = ODP_STASH_OP_MT;
}

static int reserve_index(void)
{
	int i;
	int index = -1;

	odp_ticketlock_lock(&stash_global->lock);

	for (i = 0; i < MAX_STASHES; i++) {
		if (stash_global->stash_reserved[i] == 0) {
			index = i;
			stash_global->stash_reserved[i] = 1;
			break;
		}
	}

	odp_ticketlock_unlock(&stash_global->lock);

	return index;
}

static void free_index(int i)
{
	odp_ticketlock_lock(&stash_global->lock);

	stash_global->stash[i] = NULL;
	stash_global->stash_reserved[i] = 0;

	odp_ticketlock_unlock(&stash_global->lock);
}

odp_stash_t odp_stash_create(const char *name, const odp_stash_param_t *param)
{
	odp_shm_t shm;
	stash_t *stash;
	uint64_t i, ring_size, shm_size;
	int ring_ptr, index;
	char shm_name[ODP_STASH_NAME_LEN + 8];

	if (param->obj_size > sizeof(uintptr_t)) {
		ODP_ERR("Too large object handle.\n");
		return ODP_STASH_INVALID;
	}

	if (param->num_obj > MAX_RING_SIZE) {
		ODP_ERR("Too many objects.\n");
		return ODP_STASH_INVALID;
	}

	if (name && strlen(name) >= ODP_STASH_NAME_LEN) {
		ODP_ERR("Too long name.\n");
		return ODP_STASH_INVALID;
	}

	index = reserve_index();

	if (index < 0) {
		ODP_ERR("Maximum number of stashes created already.\n");
		return ODP_STASH_INVALID;
	}

	ring_ptr = 0;
	if (param->obj_size > sizeof(uint32_t))
		ring_ptr = 1;

	ring_size = param->num_obj;

	/* Ring size must be larger than the number of items stored */
	if (ring_size + 1 <= MIN_RING_SIZE)
		ring_size = MIN_RING_SIZE;
	else
		ring_size = ROUNDUP_POWER2_U32(ring_size + 1);

	memset(shm_name, 0, sizeof(shm_name));
	snprintf(shm_name, sizeof(shm_name) - 1, "_stash_%s", name);

	if (ring_ptr)
		shm_size = sizeof(stash_t) + (ring_size * sizeof(uintptr_t));
	else
		shm_size = sizeof(stash_t) + (ring_size * sizeof(uint32_t));

	shm = odp_shm_reserve(shm_name, shm_size, ODP_CACHE_LINE_SIZE, 0);

	if (shm == ODP_SHM_INVALID) {
		ODP_ERR("SHM reserve failed.\n");
		free_index(index);
		return ODP_STASH_INVALID;
	}

	stash = odp_shm_addr(shm);
	memset(stash, 0, sizeof(stash_t));

	if (ring_ptr) {
		ring_ptr_init(&stash->ring_ptr.hdr);

		for (i = 0; i < ring_size; i++)
			stash->ring_ptr.data[i] = 0;
	} else {
		ring_u32_init(&stash->ring_u32.hdr);

		for (i = 0; i < ring_size; i++)
			stash->ring_u32.data[i] = 0;
	}

	if (name)
		strcpy(stash->name, name);

	stash->index        = index;
	stash->shm          = shm;
	stash->obj_size     = param->obj_size;
	stash->ring_mask    = ring_size - 1;

	/* This makes stash visible to lookups */
	odp_ticketlock_lock(&stash_global->lock);
	stash_global->stash[index] = stash;
	odp_ticketlock_unlock(&stash_global->lock);

	return (odp_stash_t)stash;
}

int odp_stash_destroy(odp_stash_t st)
{
	stash_t *stash;
	int index;
	odp_shm_t shm;

	if (st == ODP_STASH_INVALID)
		return -1;

	stash = (stash_t *)(uintptr_t)st;
	index = stash->index;
	shm   = stash->shm;

	free_index(index);

	if (odp_shm_free(shm)) {
		ODP_ERR("SHM free failed.\n");
		return -1;
	}

	return 0;
}

uint64_t odp_stash_to_u64(odp_stash_t st)
{
	return _odp_pri(st);
}

odp_stash_t odp_stash_lookup(const char *name)
{
	int i;
	stash_t *stash;

	if (name == NULL)
		return ODP_STASH_INVALID;

	odp_ticketlock_lock(&stash_global->lock);

	for (i = 0; i < MAX_STASHES; i++) {
		stash = stash_global->stash[i];

		if (stash && strcmp(stash->name, name) == 0) {
			odp_ticketlock_unlock(&stash_global->lock);
			return (odp_stash_t)stash;
		}
	}

	odp_ticketlock_unlock(&stash_global->lock);

	return ODP_STASH_INVALID;
}

int32_t odp_stash_put(odp_stash_t st, const void *obj, int32_t num)
{
	stash_t *stash;
	uint32_t obj_size;
	int32_t i;

	stash = (stash_t *)(uintptr_t)st;

	if (odp_unlikely(st == ODP_STASH_INVALID))
		return -1;

	obj_size = stash->obj_size;

	if (obj_size == sizeof(uintptr_t)) {
		ring_ptr_t *ring_ptr = &stash->ring_ptr.hdr;

		ring_ptr_enq_multi(ring_ptr, stash->ring_mask,
				   (void *)(uintptr_t)obj, num);
		return num;
	}

	if (obj_size == sizeof(uint32_t)) {
		ring_u32_t *ring_u32 = &stash->ring_u32.hdr;

		ring_u32_enq_multi(ring_u32, stash->ring_mask,
				   (uint32_t *)(uintptr_t)obj, num);
		return num;
	}

	if (obj_size == sizeof(uint16_t)) {
		const uint16_t *u16_ptr = obj;
		ring_u32_t *ring_u32 = &stash->ring_u32.hdr;
		uint32_t u32[num];

		for (i = 0; i < num; i++)
			u32[i] = u16_ptr[i];

		ring_u32_enq_multi(ring_u32, stash->ring_mask, u32, num);
		return num;
	}

	if (obj_size == sizeof(uint8_t)) {
		const uint8_t *u8_ptr = obj;
		ring_u32_t *ring_u32 = &stash->ring_u32.hdr;
		uint32_t u32[num];

		for (i = 0; i < num; i++)
			u32[i] = u8_ptr[i];

		ring_u32_enq_multi(ring_u32, stash->ring_mask, u32, num);
		return num;
	}

	return -1;
}

int32_t odp_stash_get(odp_stash_t st, void *obj, int32_t num)
{
	stash_t *stash;
	uint32_t obj_size;
	uint32_t i, num_deq;

	stash = (stash_t *)(uintptr_t)st;

	if (odp_unlikely(st == ODP_STASH_INVALID))
		return -1;

	obj_size = stash->obj_size;

	if (obj_size == sizeof(uintptr_t)) {
		ring_ptr_t *ring_ptr = &stash->ring_ptr.hdr;

		return ring_ptr_deq_multi(ring_ptr, stash->ring_mask, obj, num);
	}

	if (obj_size == sizeof(uint32_t)) {
		ring_u32_t *ring_u32 = &stash->ring_u32.hdr;

		return ring_u32_deq_multi(ring_u32, stash->ring_mask, obj, num);
	}

	if (obj_size == sizeof(uint16_t)) {
		uint16_t *u16_ptr = obj;
		ring_u32_t *ring_u32 = &stash->ring_u32.hdr;
		uint32_t u32[num];

		num_deq = ring_u32_deq_multi(ring_u32, stash->ring_mask,
					     u32, num);

		for (i = 0; i < num_deq; i++)
			u16_ptr[i] = u32[i];

		return num_deq;
	}

	if (obj_size == sizeof(uint8_t)) {
		uint8_t *u8_ptr = obj;
		ring_u32_t *ring_u32 = &stash->ring_u32.hdr;
		uint32_t u32[num];

		num_deq = ring_u32_deq_multi(ring_u32, stash->ring_mask,
					     u32, num);

		for (i = 0; i < num_deq; i++)
			u8_ptr[i] = u32[i];

		return num_deq;
	}

	return -1;
}

int odp_stash_flush_cache(odp_stash_t st)
{
	if (odp_unlikely(st == ODP_STASH_INVALID))
		return -1;

	return 0;
}
