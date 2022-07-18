// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2022, Linaro Limited
 *
 * Example of implementation to retrieve device HUK from BSEC OPT words.
 * HUK is expected to be a 16 bytes value, stored in 4 contiguous BSEC words.
 * This code relies on 3 configuration switches
 *
 * CFG_STM32MP1_HUK_BSEC_SHADOW_TESTKEY is a boolean switch (y|n). When
 *      enable, HUK BSEC shadow register are overridden with an all 0's
 *      test key.
 */

#include <assert.h>
#include <config.h>
#include <drivers/stm32_bsec.h>
#include <kernel/panic.h>
#include <kernel/tee_common_otp.h>
#include <string.h>

#if !defined(CFG_STM32MP1_HUK_BSEC_BASE) || \
    !defined(CFG_STM32MP1_HUK_BSEC_COUNT)
#error "Missing CFG_STM32MP1_HUK_BSEC_BASE and/or CFG_STM32MP1_HUK_BSEC_COUNT"
#endif

TEE_Result tee_otp_get_hw_unique_key(struct tee_hw_unique_key *huk)
{
	static bool initialized = false;

	const uint32_t bsec_base_id = CFG_STM32MP1_HUK_BSEC_BASE;
-	const size_t bsec_huk_count = CFG_STM32MP1_HUK_BSEC_COUNT;
	TEE_Result res = TEE_ERROR_GENERIC;
	uint32_t bsec_word = 0;
	bool is_huk_zero = true;
	size_t n = 0;

	static_assert(CFG_STM32MP1_HUK_BSEC_COUNT * sizeof(uint32_t) ==
		      sizeof(huk->data));

	if (!initialized) {
		/* Load BSEC HUK words in shadow memory once for all */
		for (n = 0; n < bsec_huk_count; n++) {
			res = stm32_bsec_shadow_register(bsec_base_id + n);
			if (res)
				return res;
		}

		if (IS_ENABLED(CFG_STM32MP1_HUK_BSEC_SHADOW_TESTKEY)) {
			/* Write testkey (all 0's) in BSEC shadow registers */
			bsec_word = 0;

			for (n = 0; n < bsec_huk_count; n++) {
				res = stm32_bsec_write_otp(bsec_word,
							   bsec_base_id + n);
				if (res) {
					IMSG("Can't shadow HUK test key: %#"PRIx32,
					     res);
					return res;
				}
			}

			IMSG("BSEC OTPs for HUK shadowed with test key");
		}

		initialized = true;
	}

	/* Read HUK from BSEC shadow memory */
	for (n = 0; n < bsec_huk_count; n++) {
		res = stm32_bsec_read_otp(&bsec_word, bsec_base_id + n);
		if (res)
			return res;

		memcpy(huk->data + n * sizeof(uint32_t), &bsec_word,
		       sizeof(uint32_t));
	}

	return TEE_SUCCESS;
}
