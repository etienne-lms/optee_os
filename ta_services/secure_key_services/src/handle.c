/*
 * Copyright (c) 2014-2018, Linaro Limited
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <assert.h>
#include <stdlib.h>
#include <tee_internal_api.h>
#include <tee_internal_api_extensions.h>

#include "handle.h"

/*
 * Define the initial capacity of the database. It should be a low number
 * multiple of 2 since some databases a likely to only use a few handles.
 * Since the algorithm is to doubles up when growing it shouldn't cause a
 * noticable overhead on large databases.
 */
#define HANDLE_DB_INITIAL_MAX_PTRS	4

void handle_db_destroy(struct handle_db *db)
{
	if (db) {
		TEE_Free(db->ptrs);
		db->ptrs = NULL;
		db->max_ptrs = 0;
	}
}

uint32_t handle_get(struct handle_db *db, void *ptr)
{
	uint32_t n;
	void *p;
	uint32_t new_max_ptrs;

	if (!db || !ptr)
		return 0;

	/* Try to find an empty location (index 0 is reserved as invalid) */
	for (n = 1; n < db->max_ptrs; n++) {
		if (!db->ptrs[n]) {
			db->ptrs[n] = ptr;
			return n;
		}
	}

	/* No location available, grow the ptrs array */
	if (db->max_ptrs)
		new_max_ptrs = db->max_ptrs * 2;
	else
		new_max_ptrs = HANDLE_DB_INITIAL_MAX_PTRS;

	if (new_max_ptrs > (UINT32_MAX / 2))
		return 0;

	p = TEE_Realloc(db->ptrs, new_max_ptrs * sizeof(void *));
	if (!p)
		return 0;
	db->ptrs = p;
	TEE_MemFill(db->ptrs + db->max_ptrs, 0,
		    (new_max_ptrs - db->max_ptrs) * sizeof(void *));
	db->max_ptrs = new_max_ptrs;

	/* Since n stopped at db->max_ptrs there is an empty location there */
	db->ptrs[n] = ptr;
	return n;
}

/*
 * Protect from speculative access based on out bound handle values:
 * Generate a validation mask from signed difference from max valid handle
 * value to argument handle value:
 * - out bound handle values generate a null mask.
 * - handle values that can be seen as negative index generate a null mask.
 * This scheme expects value 0 is an invalid handle value and should
 * always return an invalid reference: here always a NULL reference.
 */
static size_t guard_handle(struct handle_db *db, uint32_t handle)
{
	uint32_t mask_neg = ((int32_t)handle) >> (sizeof(handle) * 8 - 1);
	uint32_t udiff = db->max_ptrs - handle;
	int32_t diff = ((int32_t)udiff) >> (sizeof(udiff) * 8 - 1);
	uint32_t mask_ovf = diff;

	return handle & ~mask_ovf & ~mask_neg;
}

void *handle_put(struct handle_db *db, uint32_t handle)
{
	uint32_t index;
	void *p;

	if (!db)
		return NULL;

	index = guard_handle(db, handle);
	p = db->ptrs[index];
	db->ptrs[index] = NULL;
	return p;
}

void *handle_lookup(struct handle_db *db, uint32_t handle)
{
	if (!db)
		return NULL;

	return db->ptrs[guard_handle(db, handle)];
}
