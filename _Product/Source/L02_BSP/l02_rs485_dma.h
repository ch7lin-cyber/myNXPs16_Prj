/*
 * Copyright 2026
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef L02_RS485_DMA_H_
#define L02_RS485_DMA_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "fsl_common.h"
#include "l02_rs485_direction.h"

/*!
 * @brief Bind the generated USART/DMA handles to the L02 RS-485 callback.
 *
 * Call this once after BOARD_InitBootPeripherals(). FLEXCOMM0 is skipped when
 * PRODUCT_FC0_MODE selects the debug console.
 */
status_t L02_Rs485Dma_Init(void);

/*!
 * @brief Start one non-blocking RS-485 DMA transmission.
 *
 * The function changes DIR to TX, applies the configured pre-TX delay, and
 * starts USART DMA. The channel remains busy until L02_Rs485Dma_Process()
 * completes the post-TX delay and changes DIR back to RX.
 *
 * This function must be called from task/main context, not from an ISR.
 */
status_t L02_Rs485Dma_SendAsync(l02_rs485_channel_t channel, const uint8_t *data, size_t length);

/*!
 * @brief Perform deferred TX-completion work outside the interrupt handler.
 *
 * Call this function continuously from the main loop or an L03 periodic task.
 */
void L02_Rs485Dma_Process(void);

/*! @brief Query whether a channel has an active or pending transmission. */
status_t L02_Rs485Dma_IsTransmitBusy(l02_rs485_channel_t channel, bool *isBusy);

#endif /* L02_RS485_DMA_H_ */
