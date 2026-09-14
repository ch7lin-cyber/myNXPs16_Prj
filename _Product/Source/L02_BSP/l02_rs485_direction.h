/*
 * Copyright 2026
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef L02_RS485_DIRECTION_H_
#define L02_RS485_DIRECTION_H_

#include <stdbool.h>
#include <stdint.h>

#include "fsl_common.h"

/*!
 * @brief Logical RS-485 channels. The physical pin mapping remains in pin_mux.h.
 */
typedef enum _l02_rs485_channel
{
    kL02_Rs485Channel0 = 0U, /*!< FLEXCOMM0, FC0_DIR on PIO1_0. */
    kL02_Rs485Channel1 = 1U, /*!< FLEXCOMM1, FC1_DIR on PIO0_14. */
    kL02_Rs485ChannelCount
} l02_rs485_channel_t;

/*! @brief Direction timing around one RS-485 transmission. */
typedef struct _l02_rs485_timing
{
    uint32_t preTxDelayUs;  /*!< Delay after DIR=TX and before loading USART/DMA. */
    uint32_t postTxDelayUs; /*!< Delay after USART TXIDLE and before DIR=RX. */
} l02_rs485_timing_t;

/*!
 * @brief Initialize direction state and put every available channel in receive mode.
 *
 * FLEXCOMM0 is unavailable when PRODUCT_FC0_MODE selects the debug console.
 */
status_t L02_Rs485Direction_Init(void);

/*! @brief Change the direction timing for one available channel. */
status_t L02_Rs485Direction_SetTiming(l02_rs485_channel_t channel, const l02_rs485_timing_t *timing);

/*!
 * @brief Set DIR to transmit and apply the configured pre-transmit delay.
 *
 * Call this before starting a USART or DMA transfer. This function is blocking and
 * must not be called from an interrupt handler.
 */
status_t L02_Rs485Direction_BeginTransmit(l02_rs485_channel_t channel);

/*!
 * @brief Report whether the USART shift register has sent the final stop bit.
 */
status_t L02_Rs485Direction_IsTransmitComplete(l02_rs485_channel_t channel, bool *isComplete);

/*!
 * @brief Wait for USART TXIDLE, apply the post-transmit delay, then set DIR to receive.
 *
 * DMA completion alone is not sufficient because bytes can remain in the USART FIFO
 * or shift register. On timeout, DIR intentionally remains in transmit mode so the
 * last byte is not truncated. This function is blocking and must not be called from
 * an interrupt handler.
 */
status_t L02_Rs485Direction_EndTransmitBlocking(l02_rs485_channel_t channel, uint32_t timeoutUs);

/*!
 * @brief Force an available channel into receive mode without waiting for USART.
 *
 * Use this for initialization or explicit transfer abort only.
 */
status_t L02_Rs485Direction_ForceReceive(l02_rs485_channel_t channel);

#endif /* L02_RS485_DIRECTION_H_ */
