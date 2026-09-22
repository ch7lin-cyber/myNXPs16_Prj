/*
 * Copyright 2026
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef PRODUCT_RS485_DMA_H_
#define PRODUCT_RS485_DMA_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "fsl_common.h"
#include "product_rs485_direction.h"

#define PRODUCT_RS485_DMA_DEFAULT_FRAME_TIMEOUT_US (1000U)

status_t ProductRs485Dma_Init(void);
status_t ProductRs485Dma_Configure(
    product_rs485_channel_t channel,
    uint32_t baudRate,
    uint8_t dataBits,
    uint8_t parity,
    uint8_t stopBits);
status_t ProductRs485Dma_SendAsync(product_rs485_channel_t channel, const uint8_t *data, size_t length);
status_t ProductRs485Dma_AbortTransmit(product_rs485_channel_t channel);
status_t ProductRs485Dma_IsTransmitBusy(product_rs485_channel_t channel, bool *isBusy);

/*! @brief Configure one silence timeout for delimiter-based protocols. */
status_t ProductRs485Dma_SetReceiveTimeoutUs(product_rs485_channel_t channel, uint32_t timeoutUs);

/*!
 * @brief Configure Modbus RTU T1.5 and T3.5 receive timing.
 *
 * MRT0 first expires at T1.5, then at the remaining interval to T3.5. A byte
 * observed between those boundaries marks the frame as a timing error.
 */
status_t ProductRs485Dma_SetRtuReceiveTimingUs(
    product_rs485_channel_t channel,
    uint32_t t15Us,
    uint32_t t35Us);

status_t ProductRs485Dma_StartReceive(product_rs485_channel_t channel, uint8_t *buffer, size_t capacity);
status_t ProductRs485Dma_GetReceiveCount(product_rs485_channel_t channel, size_t *count);
status_t ProductRs485Dma_CompleteReceive(product_rs485_channel_t channel, size_t frameLength);
status_t ProductRs485Dma_TakeReceivedFrame(product_rs485_channel_t channel, size_t *length);
status_t ProductRs485Dma_GetLastReceiveTimingError(
    product_rs485_channel_t channel,
    bool *timingError);
status_t ProductRs485Dma_CancelReceive(product_rs485_channel_t channel);

/*! @brief Start the shared MRT0 channel 2 one-shot turnaround delay. */
status_t ProductRs485Dma_StartTurnaroundDelayUs(uint32_t delayUs);

/*! @brief Atomically take the MRT0 channel 2 elapsed event. */
status_t ProductRs485Dma_TakeTurnaroundDelayElapsed(bool *elapsed);

/*! @brief Perform deferred TX, RX and timer work outside interrupt context. */
void ProductRs485Dma_Process(void);

#endif /* PRODUCT_RS485_DMA_H_ */
