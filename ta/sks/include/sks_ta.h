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

/*
 * Note on SKS TA commands ABI
 *
 * For evolution of the TA API and to not mess with the GPD TEE 4 parameters
 * constraint, all the SKS TA invocation commands use a subset of available the
 * GPD TEE invocation parameter types.
 *
 * Param#0 is used for the command control arguments.
 * It is unused or is a input or in-out memory reference, aka memref[0].
 * The input buffer stores the command arguments serialized inside.
 * When in-out memref, the output buffer will store the SKS retrun code
 * for the command. Client shall get the PKCS#11 return code from it,
 *  the TEE_Result return code of the TA being forced the TEE_SUCCESS.
 *
 * Param#1 is used for input data parameters for the command.
 * It is unused or is a input memory reference, aka memref[1].
 * Evolution of the API may use memref[1] for output data as well.
 *
 * Param#2 is mostly used for output data parameters for the command.
 * Few commands uses it for a secondary input data parameter.
 * It is unused or is a input/output/in-out memory reference, aka memref[2].
 *
 * Param#3 is currently unused and reserved for evolution of the API.
 */

#endif /*__SKS_TA_H__*/
