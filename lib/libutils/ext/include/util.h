/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (c) 2014, STMicroelectronics International N.V.
 */
#ifndef UTIL_H
#define UTIL_H

#include <compiler.h>
#include <inttypes.h>

#ifndef __ASSEMBLER__
#include <assert.h>
#include <stddef.h>
#endif

#define SIZE_4K	UINTPTR_C(0x1000)
#define SIZE_1M	UINTPTR_C(0x100000)
#define SIZE_2M	UINTPTR_C(0x200000)
#define SIZE_4M	UINTPTR_C(0x400000)
#define SIZE_8M	UINTPTR_C(0x800000)
#define SIZE_2G	UINTPTR_C(0x80000000)

#ifndef MAX
#ifndef __ASSEMBLER__
#define MAX(a, b) \
	(__extension__({ __typeof__(a) _a = (a); \
	   __typeof__(b) _b = (b); \
	 _a > _b ? _a : _b; }))

#define MIN(a, b) \
	(__extension__({ __typeof__(a) _a = (a); \
	   __typeof__(b) _b = (b); \
	 _a < _b ? _a : _b; }))
#else
#define MAX(a, b)	(((a) > (b)) ? (a) : (b))
#define MIN(a, b)	(((a) < (b)) ? (a) : (b))
#endif
#endif

/*
 * In some particular conditions MAX and MIN macros fail to
 * build from C source file implmentation. In such case one
 * need to use MAX_UNSAFE/MIN_UNSAFE instead.
 */
#define MAX_UNSAFE(a, b)	(((a) > (b)) ? (a) : (b))
#define MIN_UNSAFE(a, b)	(((a) < (b)) ? (a) : (b))

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#endif

#ifndef __ASSEMBLER__
/*
 * __ROUNDUP(v, size)
 * ROUNDUP(v, size)
 * ROUNDUP_VAR(v, size)
 * Round up value @v to the even multiple of @size that shall be a power of 2.
 *
 * __ROUNDOWN() does not verify @size is a power of 2.
 *
 * ROUNDUP() requires a constant value as @size argument.
 * If @size is not a power of 2, the macro evaluates as unexpected void.
 *
 * ROUNDUP_VAR() supports a variable reference as @size argument.
 * The macro asserts (in debug mode) that @size is a power of 2.
 */
#define __ROUNDUP(v, size) \
	(((v) + ((__typeof__(v))(size) - 1)) & ~((__typeof__(v))(size) - 1))

#define ROUNDUP_VAR(v, size) \
	(__extension__({ \
		assert(IS_POWER_OF_TWO(size)); \
		__ROUNDUP((v), (size)); \
	}))

#define ROUNDUP(v, size) \
	(__builtin_choose_expr(IS_POWER_OF_TWO((size)), \
			       __ROUNDUP((v), (size)), \
			       (void)0))

/*
 * ROUNDUP2(v, size)
 *
 * Round up value @v to the even multiple of @size.
 * @size can have any value.
 */
#define ROUNDUP2(v, size)	((((v) + (size) - 1) / (size)) * (size))

/*
 * __ROUNDUP_OVERFLOW(v, size)
 * ROUNDUP_OVERFLOW(v, size)
 * ROUNDUP_OVERFLOW_VAR(v, size)
 *
 * Round up value @v to the even multiple of @size and return if result
 * overflows the output value range pointed by @res. The rounded value is
 * stored in the memory address pointed by @res.
 * @size must be a power of 2.
 *
 * __ROUNDUP_OVERFLOW() does not verify @size is a power of 2.
 *
 * ROUNDUP_OVERFLOW() requires a constant value as @size argument.
 * If @size is not a power of 2, the macro evaluates as unexpected void.
 *
 * ROUNDUP_OVERFLOW_VAR() supports a variable reference as @size argument.
 * The macro asserts (in debug mode) that @size is a power of 2.
 */
 #define __ROUNDUP_OVERFLOW(v, size, res) \
	(__extension__({ \
		typeof(*(res)) __roundup_tmp = 0; \
		typeof(v) __roundup_mask = (typeof(v))(size) - 1; \
		\
		ADD_OVERFLOW((v), __roundup_mask, &__roundup_tmp) ? 1 : \
			((void)(*(res) = __roundup_tmp & ~__roundup_mask), 0); \
	}))

#define ROUNDUP_OVERFLOW(v, size, res) \
	(__builtin_choose_expr(IS_POWER_OF_TWO((size)), \
			       __ROUNDUP_OVERFLOW((v), (size), (res)), \
			       (void)0))

#define ROUNDUP_OVERFLOW_VAR(v, size, res) \
	(__extension__({ \
		assert(IS_POWER_OF_TWO(size)); \
		__ROUNDUP_OVERFLOW((v), (size), (res)); \
	}))

/*
 * ROUNDUP_OVERFLOW2(v, size, res)
 *
 * Round up value @v to the even multiple of @size and return if result
 * overflows the output value range pointed by @res. The rounded value is
 * stored in the memory address pointed by @res.
 * @size can have any value.
 */
#define ROUNDUP_OVERFLOW2(v, size, res) \
	(__extension__({ \
		typeof(*(res)) __roundup_tmp = 0; \
		typeof(v) __roundup_mod = 0; \
		typeof(v) __roundup_add = 0; \
		\
		__roundup_mod = (v) % (typeof(v))(size); \
		if (__roundup_mod) \
			__roundup_add = (typeof(v))(size) - __roundup_mod; \
		ADD_OVERFLOW((v), __roundup_add, &__roundup_tmp) ? 1 : \
			((void)(*(res) = __roundup_tmp), 0); \
	}))

/*
 * Rounds up to the nearest multiple of y and then divides by y. Safe
 * against overflow, y has to be a power of 2.
 *
 * This macro is intended to be used to convert from "number of bytes" to
 * "number of pages" or similar units. Example:
 * num_pages = ROUNDUP_DIV(num_bytes, SMALL_PAGE_SIZE);
 */
#define ROUNDUP_DIV(x, y) (__extension__({ \
	typeof(x) __roundup_x = (x); \
	typeof(y) __roundup_mask = (typeof(x))(y) - 1; \
	\
	assert(IS_POWER_OF_TWO(y)); \
	(__roundup_x / (y)) + (__roundup_x & __roundup_mask ? 1 : 0); \
}))

/*
 * __ROUNDOWN(v, size)
 * ROUNDDOWN(v, size)
 * ROUNDDOWN_VAR(v, size)
 * Round down value @v to the even multiple of @size that shall be a power of 2.
 *
 * __ROUNDOWN() does not verify @size is a power of 2.
 *
 * ROUNDDOWN() requires a constant value as @size argument.
 * If @size is not a power of 2, the macro evaluates as unexpected void.
 *
 * ROUNDDOWN_VAR() supports a variable reference as @size argument.
 * The macro asserts (in debug mode) that @size is a power of 2.
 */
#define __ROUNDDOWN(v, size)	((v) & ~((__typeof__(v))(size) - 1)) \

#define ROUNDDOWN(v, size) \
	(__builtin_choose_expr(IS_POWER_OF_TWO((size)), \
			       __ROUNDDOWN((v), (size)), \
			       (void)0))

#define ROUNDDOWN_VAR(v, size)	\
	(__extension__({ \
		assert(IS_POWER_OF_TWO(size)); \
		__ROUNDDOWN((v), (size)); \
	}))

/*
 * ROUNDDOWN2(v, size)

 * Round down value @v to the even multiple of @size.
 * @sizee can have any value.
 */
#define ROUNDDOWN2(v, size)	(((v) / (size)) * (size))

/*
 * Round up the result of x / y to the nearest upper integer if result is not
 * already an integer.
 */
#define DIV_ROUND_UP(x, y) (((x) + (y) - 1) / (y))

/* Unsigned integer division with nearest rounding variant */
#define UDIV_ROUND_NEAREST(x, y) \
	(__extension__ ({ __typeof__(x) _x = (x); \
	  __typeof__(y) _y = (y); \
	  (_x + (_y / 2)) / _y; }))
#else
#define ROUNDUP(x, y)			((((x) + (y) - 1) / (y)) * (y))
#define ROUNDDOWN(x, y)		(((x) / (y)) * (y))
#define UDIV_ROUND_NEAREST(x, y)	(((x) + ((y) / 2)) / (y))
#endif

/* x has to be of an unsigned type */
#define IS_POWER_OF_TWO(x) (((x) != 0) && (((x) & (~(x) + 1)) == (x)))

#define IS_ALIGNED(x, a)		(((x) & ((a) - 1)) == 0)
#define IS_ALIGNED_WITH_TYPE(x, type) \
        (__extension__({ \
                type __is_aligned_y; \
                IS_ALIGNED((uintptr_t)(x), __alignof__(__is_aligned_y)); \
        }))

#define TO_STR(x) _TO_STR(x)
#define _TO_STR(x) #x

#define CONCAT(x, y) _CONCAT(x, y)
#define _CONCAT(x, y) x##y

#define container_of(ptr, type, member) \
	(__extension__({ \
		const typeof(((type *)0)->member) *__ptr = (ptr); \
		(type *)((unsigned long)(__ptr) - offsetof(type, member)); \
	}))

#define MEMBER_SIZE(type, member) sizeof(((type *)0)->member)

#ifdef __ASSEMBLER__
#define BIT32(nr)		(1 << (nr))
#define BIT64(nr)		(1 << (nr))
#define SHIFT_U32(v, shift)	((v) << (shift))
#define SHIFT_U64(v, shift)	((v) << (shift))
#else
#define BIT32(nr)		(UINT32_C(1) << (nr))
#define BIT64(nr)		(UINT64_C(1) << (nr))
#define SHIFT_U32(v, shift)	((uint32_t)(v) << (shift))
#define SHIFT_U64(v, shift)	((uint64_t)(v) << (shift))
#endif
#define BIT(nr)			BIT32(nr)

/*
 * Create a contiguous bitmask starting at bit position @l and ending at
 * position @h. For example
 * GENMASK_64(39, 21) gives us the 64bit vector 0x000000ffffe00000.
 */
#define GENMASK_32(h, l) \
	((UINT32_C(0xffffffff) << (l)) & \
	 (UINT32_C(0xffffffff) >> (32 - 1 - (h))))

#define GENMASK_64(h, l) \
	(((~UINT64_C(0)) << (l)) & (~UINT64_C(0) >> (64 - 1 - (h))))

/*
 * Checking overflow for addition, subtraction and multiplication. Result
 * of operation is stored in res which is a pointer to some kind of
 * integer.
 *
 * The macros return true if an overflow occurred and *res is undefined.
 */
#define ADD_OVERFLOW(a, b, res) __compiler_add_overflow((a), (b), (res))
#define SUB_OVERFLOW(a, b, res) __compiler_sub_overflow((a), (b), (res))
#define MUL_OVERFLOW(a, b, res) __compiler_mul_overflow((a), (b), (res))

/* Return a signed +1, 0 or -1 value based on data comparison */
#define CMP_TRILEAN(a, b) \
	(__extension__({ \
		__typeof__(a) _a = (a); \
		__typeof__(b) _b = (b); \
		\
		_a > _b ? 1 : _a < _b ? -1 : 0; \
	}))

#ifndef __ASSEMBLER__
static inline uint64_t reg_pair_to_64(uint32_t reg0, uint32_t reg1)
{
	return (uint64_t)reg0 << 32 | reg1;
}

static inline uint32_t high32_from_64(uint64_t val)
{
	return val >> 32;
}

static inline uint32_t low32_from_64(uint64_t val)
{
	return val;
}

static inline void reg_pair_from_64(uint64_t val, uint32_t *reg0,
				    uint32_t *reg1)
{
	*reg0 = high32_from_64(val);
	*reg1 = low32_from_64(val);
}

/* Get and set bit fields  */
static inline uint32_t get_field_u32(uint32_t reg, uint32_t mask)
{
	return (reg & mask) / (mask & ~(mask - 1));
}

static inline uint32_t set_field_u32(uint32_t reg, uint32_t mask, uint32_t val)
{
	return (reg & ~mask) | (val * (mask & ~(mask - 1)));
}

static inline uint64_t get_field_u64(uint64_t reg, uint64_t mask)
{
	return (reg & mask) / (mask & ~(mask - 1));
}

static inline uint64_t set_field_u64(uint64_t reg, uint64_t mask, uint64_t val)
{
	return (reg & ~mask) | (val * (mask & ~(mask - 1)));
}

/* Helper function for qsort with standard types */
void qsort_int(int *aa, size_t n);
void qsort_uint(unsigned int *aa, size_t n);
void qsort_long(long int *aa, size_t n);
void qsort_ul(unsigned long int *aa, size_t n);
void qsort_ll(long long int *aa, size_t n);
void qsort_ull(unsigned long long int *aa, size_t n);
void qsort_s8(int8_t *aa, size_t n);
void qsort_u8(uint8_t *aa, size_t n);
void qsort_s16(int16_t *aa, size_t n);
void qsort_u16(uint16_t *aa, size_t n);
void qsort_s32(int32_t *aa, size_t n);
void qsort_u32(uint32_t *aa, size_t n);
void qsort_s64(int64_t *aa, size_t n);
void qsort_u64(uint64_t *aa, size_t n);
#endif

#endif /*UTIL_H*/
