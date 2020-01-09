/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (c) 2018-2019, Linaro Limited
 */

#ifndef __SKS_TA_H__
#define __SKS_TA_H__

#include <sys/types.h>
#include <stdint.h>
#include <util.h>

#define TA_SKS_UUID { 0xfd02c9da, 0x306c, 0x48c7, \
			{ 0xa4, 0x9c, 0xbb, 0xd8, 0x27, 0xae, 0x86, 0xee } }

/* SKS trusted application version information */
#define SKS_VERSION_MAJOR		0
#define SKS_VERSION_MINOR		0

/* Attribute specific values */
#define SKS_CK_UNAVAILABLE_INFORMATION		UINT32_C(0xFFFFFFFF)
#define SKS_UNDEFINED_ID			SKS_CK_UNAVAILABLE_INFORMATION
#define SKS_FALSE				0
#define SKS_TRUE				1

/*
 * Note on SKS TA commands API
 *
 * For evolution of the TA API and to not mess with the GPD TEE 4 parameters
 * constraint, all the SKS TA invocation commands use a subset of the available
 * GPD TEE invocation parameter types.
 *
 * Param#0 is used for the so-called control arguments of the invoked command
 * and for providing a PKCS#11 compliant status code for the request command.
 * Param#0 is an in/out memory reference (aka memref[0]). The input buffer
 * stores the command arguments serialized inside. The output buffer will
 * store the 32bit SKS return code for the command. Client shall get this
 * return code and override the GPD TEE Client API legacy TEE_Result value.
 *
 * Param#1 is used for input data arguments of the invoked command.
 * It is unused or is an input memory reference, aka memref[1].
 * Evolution of the API may use memref[1] for output data as well.
 *
 * Param#2 is mostly used for output data arguments of the invoked command
 * and for output handles generated from invoked commands.
 * Few commands uses it for a secondary input data buffer argument.
 * It is unused or is an input/output/in-out memory reference, aka memref[2].
 *
 * Param#3 is currently unused and reserved for evolution of the API.
 */

/*
 * SKS_CMD_PING		Acknowledge TA presence and return TA version info
 *
 * Optional invocation parameter (if none, command simply returns with success)
 * [out]        memref[2] = [
 *                      32bit version major value,
 *                      32bit version minor value
 *              ]
 */
#define SKS_CMD_PING			0
#endif /*__SKS_TA_H__*/
