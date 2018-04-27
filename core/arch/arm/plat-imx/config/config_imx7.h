/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright 2017 NXP
 *
 * Peng Fan <peng.fan@nxp.com>
 */

#ifndef __CONFIG_IMX7_H
#define __CONFIG_IMX7_H

#include <imx-regs.h>

#ifdef CFG_UART_BASE
#define CONSOLE_UART_BASE	CFG_UART_BASE
#else
#define CONSOLE_UART_BASE	UART1_BASE
#endif

#ifndef CFG_DDR_SIZE
#error "CFG_DDR_SIZE not defined"
#endif

#define DRAM0_BASE		0x80000000
#define DRAM0_SIZE		CFG_DDR_SIZE

#define TZDRAM_SIZE		(32 * 1024 * 1024 - TEE_SHMEM_SIZE)
#define TZDRAM_BASE		(DRAM0_BASE + (CFG_DDR_SIZE - \
					TZDRAM_SIZE - TEE_SHMEM_SIZE))

#define TEE_SHMEM_START		(TZDRAM_BASE + TZDRAM_SIZE)
#define TEE_SHMEM_SIZE		CFG_SHMEM_SIZE

#define TEE_RAM_VA_SIZE		(1024 * 1024)

/*
 * Everything is in TZDRAM.
 * +------------------+
 * |        | TEE_RAM |
 * + TZDRAM +---------+
 * |        | TA_RAM  |
 * +--------+---------+
 */
#define TEE_RAM_PH_SIZE		TEE_RAM_VA_SIZE
#define TEE_RAM_START		TZDRAM_BASE
#define TA_RAM_START		ROUNDUP((TZDRAM_BASE + TEE_RAM_VA_SIZE), \
					CORE_MMU_DEVICE_SIZE)
#define TA_RAM_SIZE		ROUNDDOWN((TZDRAM_SIZE - TEE_RAM_VA_SIZE), \
					  CORE_MMU_DEVICE_SIZE)

#ifdef CFG_TEE_LOAD_ADDR
#define TEE_LOAD_ADDR		CFG_TEE_LOAD_ADDR
#else
#define TEE_LOAD_ADDR		TEE_RAM_START
#endif

#endif /*__CONFIG_IMX7_H*/
