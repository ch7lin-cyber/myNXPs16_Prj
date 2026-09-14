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

/*! @brief Default silent interval used to delimit an RX frame. */
#define L02_RS485_DMA_DEFAULT_FRAME_TIMEOUT_US (1000U)

/*!
 * @brief Bind generated USART/DMA handles and start the shared 100 us timer tick.
 *
 * Call once after BOARD_InitBootPeripherals(). FLEXCOMM0 is skipped when
 * PRODUCT_FC0_MODE selects the debug console.
 */
status_t L02_Rs485Dma_Init(void);

/*!
 * @brief Start one non-blocking RS-485 DMA transmission.
 *
 * The channel remains busy until L02_Rs485Dma_Process() applies the post-TX
 * delay and changes DIR back to RX. Call from task/main context only.
 */
status_t L02_Rs485Dma_SendAsync(l02_rs485_channel_t channel, const uint8_t *data, size_t length);

/*! @brief Query whether a channel has an active or pending transmission. */
status_t L02_Rs485Dma_IsTransmitBusy(l02_rs485_channel_t channel, bool *isBusy);

/*!
 * @brief Configure the silent interval that completes a variable-length RX frame.
 *
 * CTIMER4 provides 100 us resolution; the requested value is rounded up.
 * For Modbus RTU, L03 can set this from baud rate and character format.
 */
status_t L02_Rs485Dma_SetReceiveTimeoutUs(l02_rs485_channel_t channel, uint32_t timeoutUs);

/*!
 * @brief Start a variable-length DMA receive.
 *
 * Reception completes when the buffer is full or no new byte arrives during
 * the configured timeout. The buffer must remain valid until the frame is
 * taken or reception is cancelled.
 */
status_t L02_Rs485Dma_StartReceive(l02_rs485_channel_t channel, uint8_t *buffer, size_t capacity);

/*!
 * @brief Read the current DMA byte count without stopping reception.
 *
 * Returns kStatus_NoTransferInProgress when RX is not active.
 */
status_t L02_Rs485Dma_GetReceiveCount(l02_rs485_channel_t channel, size_t *count);

/*!
 * @brief Stop active DMA and publish the requested leading bytes as one frame.
 *
 * This supports delimiter-based protocols such as Modbus ASCII. Any bytes
 * already received after frameLength are discarded; a Modbus master must wait
 * for the slave response before sending its next request.
 */
status_t L02_Rs485Dma_CompleteReceive(l02_rs485_channel_t channel, size_t frameLength);

/*!
 * @brief Take the completed frame length and release the channel for a new RX.
 *
 * The received bytes are already in the buffer supplied to StartReceive().
 * Returns kStatus_NoData until a complete frame is available.
 */
status_t L02_Rs485Dma_TakeReceivedFrame(l02_rs485_channel_t channel, size_t *length);

/*! @brief Cancel an active receive and discard its partial frame. */
status_t L02_Rs485Dma_CancelReceive(l02_rs485_channel_t channel);

/*!
 * @brief Perform deferred TX and RX completion work outside interrupt context.
 *
 * Call continuously from the main loop or an L03 periodic task.
 */
void L02_Rs485Dma_Process(void);

/*!
 * @brief CTIMER4 callback entry; publishes only the 100 us time base.
 *
 * Call this function from COMM_TMOut_callback().
 */
void L02_Rs485Dma_TimerCallback(uint32_t flags);

#endif /* L02_RS485_DMA_H_ */
