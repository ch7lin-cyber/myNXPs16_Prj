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

#define L02_RS485_DMA_DEFAULT_FRAME_TIMEOUT_US (1000U)

status_t L02_Rs485Dma_Init(void);
status_t L02_Rs485Dma_SendAsync(l02_rs485_channel_t channel, const uint8_t *data, size_t length);
status_t L02_Rs485Dma_IsTransmitBusy(l02_rs485_channel_t channel, bool *isBusy);

/*! @brief Configure one silence timeout for delimiter-based protocols. */
status_t L02_Rs485Dma_SetReceiveTimeoutUs(l02_rs485_channel_t channel, uint32_t timeoutUs);

/*!
 * @brief Configure Modbus RTU T1.5 and T3.5 receive timing.
 *
 * MRT0 first expires at T1.5, then at the remaining interval to T3.5. A byte
 * observed between those boundaries marks the frame as a timing error.
 */
status_t L02_Rs485Dma_SetRtuReceiveTimingUs(
    l02_rs485_channel_t channel,
    uint32_t t15Us,
    uint32_t t35Us);

status_t L02_Rs485Dma_StartReceive(l02_rs485_channel_t channel, uint8_t *buffer, size_t capacity);
status_t L02_Rs485Dma_GetReceiveCount(l02_rs485_channel_t channel, size_t *count);
status_t L02_Rs485Dma_CompleteReceive(l02_rs485_channel_t channel, size_t frameLength);
status_t L02_Rs485Dma_TakeReceivedFrame(l02_rs485_channel_t channel, size_t *length);
status_t L02_Rs485Dma_GetLastReceiveTimingError(
    l02_rs485_channel_t channel,
    bool *timingError);
status_t L02_Rs485Dma_CancelReceive(l02_rs485_channel_t channel);

/*! @brief Start the shared MRT0 channel 2 one-shot turnaround delay. */
status_t L02_Rs485Dma_StartTurnaroundDelayUs(uint32_t delayUs);

/*! @brief Atomically take the MRT0 channel 2 elapsed event. */
status_t L02_Rs485Dma_TakeTurnaroundDelayElapsed(bool *elapsed);

/*! @brief Perform deferred TX, RX and timer work outside interrupt context. */
void L02_Rs485Dma_Process(void);

#endif /* L02_RS485_DMA_H_ */
