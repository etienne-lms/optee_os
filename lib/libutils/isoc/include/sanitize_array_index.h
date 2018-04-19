/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (c) Linaro Limited
 */
#ifndef SANITIZE_ARRAY_SIGNED_INDEX_H
#define SANITIZE_ARRAY_SIGNED_INDEX_H

#ifndef ASM
#include <assert.h>

/*
 * long int sanitize_array_signed_index_nospec(long int index, long int max)
 *
 * Return the value of signed index idx or 0 if idx is negative or if
 * is exceeds value of max. The sequence must execute without
 * speculatively returning any out of bounds index values.
 *
 * Max must be positive.
 */
long int sanitize_array_signed_index_nospec(long int index, long int max);
#endif

#endif /*SANITIZE_ARRAY_SIGNED_INDEX_H*/
