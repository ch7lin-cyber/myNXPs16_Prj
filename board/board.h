/*
 * Copyright 2017-2019 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/**
 * @file    board.h
 * @brief   Board initialization header file.
 */

#ifndef _BOARD_H_
#define _BOARD_H_

#include "ProductFeatureConfig.h"

#define BOARD_NAME "board"

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

#if (PRODUCT_FC0_MODE == PRODUCT_FC0_MODE_DEBUG_CONSOLE)
/**
 * @brief Initialize FLEXCOMM0 for the NXP debug console.
 */
void BOARD_InitDebugConsole(void);
#endif

#if defined(__cplusplus)
}
#endif /* __cplusplus */

#endif /* _BOARD_H_ */
