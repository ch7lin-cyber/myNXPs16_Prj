/*
 * Copyright 2017-2019 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/**
 * @file    board.c
 * @brief   Board initialization file.
 */

#include <stdint.h>
#include "board.h"

#if (PRODUCT_FC0_MODE == PRODUCT_FC0_MODE_DEBUG_CONSOLE)

#include "fsl_clock.h"
#include "fsl_debug_console.h"

#define BOARD_DEBUG_CONSOLE_INSTANCE (0U)
#define BOARD_DEBUG_CONSOLE_BAUDRATE (115200U)

/**
 * @brief Initialize the FLEXCOMM0 UART debug console.
 */
void BOARD_InitDebugConsole(void)
{
    uint32_t uartClockFreq;

    uartClockFreq = CLOCK_GetFlexCommClkFreq(BOARD_DEBUG_CONSOLE_INSTANCE);

    (void)DbgConsole_Init(BOARD_DEBUG_CONSOLE_INSTANCE,
                          BOARD_DEBUG_CONSOLE_BAUDRATE,
                          kSerialPort_Uart,
                          uartClockFreq);
}

#endif /* PRODUCT_FC0_MODE == PRODUCT_FC0_MODE_DEBUG_CONSOLE */
