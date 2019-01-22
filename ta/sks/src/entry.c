// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2018-2019, Linaro Limited
 */

#include <compiler.h>
#include <sks_ta.h>
#include <tee_internal_api.h>
#include <tee_internal_api_extensions.h>

TEE_Result TA_CreateEntryPoint(void)
{
	return TEE_SUCCESS;
}

void TA_DestroyEntryPoint(void)
{
}

TEE_Result TA_OpenSessionEntryPoint(uint32_t __unused param_types,
				    TEE_Param __unused params[4],
				    void **session)
{
	*session = NULL;

	return TEE_SUCCESS;
}

void TA_CloseSessionEntryPoint(void *session __unused)
{
}

/*
 * Entry point for invocation command SKS_CMD_PING
 *
 * @ctrl - param memref[0] or NULL: expected NULL
 * @in - param memref[1] or NULL: expected NULL
 * @out - param memref[2] or NULL
 *
 * Return a TEE_Result value
 */
static TEE_Result entry_ping(TEE_Param *ctrl, TEE_Param *in, TEE_Param *out)
{
	const uint32_t ver[] = { SKS_VERSION_ID0, SKS_VERSION_ID1 };

	if (ctrl || in)
		return TEE_ERROR_BAD_PARAMETERS;

	if (!out)
		return TEE_SUCCESS;

	if (out->memref.size < sizeof(ver)) {
		out->memref.size = sizeof(ver);
		return TEE_ERROR_SHORT_BUFFER;
	}

	TEE_MemMove(out->memref.buffer, ver, sizeof(ver));

	return TEE_SUCCESS;
}

/*
 * Entry point for SKS TA commands
 */
TEE_Result TA_InvokeCommandEntryPoint(void *tee_session __unused, uint32_t cmd,
				      uint32_t ptypes,
				      TEE_Param params[TEE_NUM_PARAMS])
{
	TEE_Param *ctrl __maybe_unused = NULL;
	TEE_Param *p1_in __maybe_unused = NULL;
	TEE_Param *p1_out __maybe_unused = NULL;
	TEE_Param *p2_in __maybe_unused = NULL;
	TEE_Param *p2_out __maybe_unused = NULL;
	TEE_Result res = TEE_ERROR_GENERIC;

	/* Param#0: none or in-out buffer with serialized arguments */
	switch (TEE_PARAM_TYPE_GET(ptypes, 0)) {
	case TEE_PARAM_TYPE_NONE:
		break;
	case TEE_PARAM_TYPE_MEMREF_INOUT:
		ctrl = &params[0];
		break;
	default:
		return TEE_ERROR_BAD_PARAMETERS;
	}

	/* Param#1: none or input data buffer */
	switch (TEE_PARAM_TYPE_GET(ptypes, 1)) {
	case TEE_PARAM_TYPE_NONE:
		break;
	case TEE_PARAM_TYPE_MEMREF_INPUT:
		p1_in = &params[1];
		break;
	default:
		return TEE_ERROR_BAD_PARAMETERS;
	}

	/* Param#2: none or input data buffer */
	switch (TEE_PARAM_TYPE_GET(ptypes, 2)) {
	case TEE_PARAM_TYPE_NONE:
		break;
	case TEE_PARAM_TYPE_MEMREF_INPUT:
		p2_in = &params[2];
		break;
	case TEE_PARAM_TYPE_MEMREF_OUTPUT:
		p2_out = &params[2];
		break;
	default:
		return TEE_ERROR_BAD_PARAMETERS;
	}

	/* Param#3: currently unused */
	switch (TEE_PARAM_TYPE_GET(ptypes, 3)) {
	case TEE_PARAM_TYPE_NONE:
		break;
	default:
		return TEE_ERROR_BAD_PARAMETERS;
	}

	switch (cmd) {
	case SKS_CMD_PING:
		res = entry_ping(ctrl, p1_in, p2_out);
		break;

	default:
		EMSG("Command 0x%" PRIx32 " is not supported", cmd);
		return TEE_ERROR_NOT_SUPPORTED;
	}

	/* Currently no output data stored in output param#0 */
	if (TEE_PARAM_TYPE_GET(ptypes, 0) == TEE_PARAM_TYPE_MEMREF_INOUT)
		ctrl->memref.size = 0;

	return res;
}
