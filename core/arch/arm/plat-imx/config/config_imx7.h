/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright 2017 NXP
 *
 * Peng Fan <peng.fan@nxp.com>
 */

#ifndef __CONFIG_IMX7_H
#define __CONFIG_IMX7_H

#include <imx-regs.h>
#include <mm/generic_ram_layout.h>

#ifdef CFG_UART_BASE
#define CONSOLE_UART_BASE	CFG_UART_BASE
#else
#define CONSOLE_UART_BASE	UART1_BASE
#endif

#ifndef CFG_DDR_SIZE
#error "CFG_DDR_SIZE not defined"
#endif

#define DRAM0_BASE		0x80000000

#ifndef TZDRAM_BASE
/*
 * If configuration did not set memory locations, default to the legacy that
 * uses the last 32MB of the DDR which end with the static SHM (mandated
 * CFG_SHMEM_SIZE).
 */
#define TZDRAM_BASE		(DRAM0_BASE - 32 * 1024 * 1024 + CFG_DDR_SIZE)
#define TZDRAM_SIZE		(32 * 1024 * 1024 - CFG_SHMEM_SIZE)
#define TEE_RAM_VA_SIZE		(1024 * 1024)
#define TEE_RAM_PH_SIZE		TEE_RAM_VA_SIZE
#define TEE_RAM_START		TZDRAM_BASE
#define TA_RAM_START		(TZDRAM_BASE + TEE_RAM_VA_SIZE)
#define TA_RAM_SIZE		(TZDRAM_SIZE - TEE_RAM_VA_SIZE)
#define TEE_SHMEM_START		(TZDRAM_BASE + TZDRAM_SIZE)
#define TEE_SHMEM_SIZE		CFG_SHMEM_SIZE
#endif

#endif /*__CONFIG_IMX7_H*/
