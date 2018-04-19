/*
 * Copyright (c) 2014-2018, Linaro Limited
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <stdlib.h>
#include <tee_internal_api.h>
#include <tee_internal_api_extensions.h>
#include <sanitize_array_index.h>

#include "handle.h"

/*
 * Define the initial capacity of the database. It should be a low number
 * multiple of 2 since some databases a likely to only use a few handles.
 * Since the algorithm is to doubles up when growing it shouldn't cause a
 * noticable overhead on large databases.
 */
#define HANDLE_DB_INITIAL_MAX_PTRS	4

/*
 * Handles are uint32_t value. 0 denotes an invalid handle. Max return value
 * is below UINT32_MAX / 2 which relates to INT32_MAX. This limitation is due
 * the internal handlings of handle as signed value which are not allowed to
 * be negative design.
 */
#define HANDLE_MAX		(UINT32_MAX / 2)

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
	ssize_t n;
	void *p;
	ssize_t new_max_ptrs;

	if (!db || !ptr)
		return 0;

	/* Try to find an empty location (index 0 is reserved as invalid) */
	for (n = 1; n < db->max_ptrs; n++) {
		if (!db->ptrs[n]) {
			db->ptrs[n] = ptr;
			return (uint32_t)n;
		}
	}

	/* No location available, grow the ptrs array */
	if (db->max_ptrs)
		new_max_ptrs = db->max_ptrs * 2;
	else
		new_max_ptrs = HANDLE_DB_INITIAL_MAX_PTRS;

	if (new_max_ptrs > HANDLE_MAX)
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
	return (uint32_t)n;
}

void *handle_put(struct handle_db *db, uint32_t handle)
{
	ssize_t idx = (long int)handle;
	void *p;

	if (!db)
		return NULL;

	idx = sanitize_array_signed_index_nospec(idx, db->max_ptrs);
	p = db->ptrs[idx];
	db->ptrs[idx] = NULL;
	return p;
}

void *handle_lookup(struct handle_db *db, uint32_t handle)
{
	ssize_t idx = (long int)handle;

	if (!db)
		return NULL;

	return db->ptrs[sanitize_array_signed_index_nospec(idx, db->max_ptrs)];
}
