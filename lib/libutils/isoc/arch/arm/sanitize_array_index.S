/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (c) 2018, Linaro Limited
 */

#include <asm.S>

#if defined(CFG_UNWIND)
#define UNWIND(...)	__VA_ARGS__
#else
#define UNWIND(...)
#endif

#ifdef ARM32
#define SANITY_SHIFT 	31

	.macro return
		bx	lr
	.endm
#endif

#ifdef ARM64
#define SANITY_SHIFT	63

	.macro return
		ret
	.endm
#endif

	.macro __sanitize_array_signed_index_nospec x0, x1, x2
		asr	\x2, \x0, #SANITY_SHIFT	/* X2 = (idx < 0) ? ~0 :: 0 */
		sub	\x1, \x1, \x0		/* X1 is negative if handle > max */
		asr	\x1, \x1, #SANITY_SHIFT	/* X2 = (idx > max) ? ~0 :: 0 */
		bics	\x0, \x0, \x1		/* nullify idx if above max */
		bics	\x0, \x0, \x2		/* nullify idx if idx negative */
		return
	.endm

FUNC sanitize_array_signed_index_nospec , :
UNWIND(	.fnstart)
#if defined(ARM32)
	__sanitize_array_signed_index_nospec r0, r1, r2
#elif defined(ARM64)
	__sanitize_array_signed_index_nospec x0, x1, x2
#else
#error "Expect ARM32 or ARM64 as assembly directive"
#endif
UNWIND(	.fnend)
END_FUNC sanitize_array_signed_index_nospec
