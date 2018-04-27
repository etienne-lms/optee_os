/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (C) 2018 Linaro Limited
 */

#ifndef __CONFIG_IMX6UL_H
#define __CONFIG_IMX6UL_H

/* For i.MX 6UltraLite and 6ULL EVK board */

#include <imx-regs.h>
#include <mm/generic_ram_layout.h>

#ifdef CFG_WITH_PAGER
#error "Pager not supported for platform mx6ulevk"
#endif
#ifdef CFG_WITH_LPAE
#error "LPAE not supported for now"
#endif

#define CONSOLE_UART_BASE	UART1_BASE

#endif /*__CONFIG_IMX6UL_H*/

