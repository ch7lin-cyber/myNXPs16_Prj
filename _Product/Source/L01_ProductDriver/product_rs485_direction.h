/*
 * Copyright 2026
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef PRODUCT_RS485_DIRECTION_H_
#define PRODUCT_RS485_DIRECTION_H_

#include <stdbool.h>
#include <stdint.h>

#include "fsl_common.h"

/*!
 * @brief Logical RS-485 channels. The physical pin mapping remains in pin_mux.h.
 */
typedef enum _product_rs485_channel
{
    kProductRs485Channel0 = 0U, /*!< FLEXCOMM0, FC0_DIR on PIO1_0. */
    kProductRs485Channel1 = 1U, /*!< FLEXCOMM1, FC1_DIR on PIO0_14. */
    kProductRs485ChannelCount
} product_rs485_channel_t;

/*! @brief Direction timing around one RS-485 transmission. */
typedef struct _product_rs485_timing
{
    uint32_t preTxDelayUs;  /*!< Delay after DIR=TX and before loading USART/DMA. */
    uint32_t postTxDelayUs; /*!< Delay after USART TXIDLE and before DIR=RX. */
} product_rs485_timing_t;

/*!
 * @brief Initialize direction state and put every available channel in receive mode.
 *
 * FLEXCOMM0 is shared with the boot/maintenance debug console.  Once this
 * driver is initialized, both channels are owned by RS-485 communication.
 */
status_t ProductRs485Direction_Init(void);

/*! @brief Change the direction timing for one available channel. */
status_t ProductRs485Direction_SetTiming(product_rs485_channel_t channel, const product_rs485_timing_t *timing);

/*!
 * @brief Set DIR to transmit and apply the configured pre-transmit delay.
 *
 * Call this before starting a USART or DMA transfer. This function is blocking and
 * must not be called from an interrupt handler.
 */
status_t ProductRs485Direction_BeginTransmit(product_rs485_channel_t channel);

/*!
 * @brief Report whether the USART shift register has sent the final stop bit.
 */
status_t ProductRs485Direction_IsTransmitComplete(product_rs485_channel_t channel, bool *isComplete);

/*!
 * @brief Wait for USART TXIDLE, apply the post-transmit delay, then set DIR to receive.
 *
 * DMA completion alone is not sufficient because bytes can remain in the USART FIFO
 * or shift register. On timeout, DIR intentionally remains in transmit mode so the
 * last byte is not truncated. This function is blocking and must not be called from
 * an interrupt handler.
 */
status_t ProductRs485Direction_EndTransmitBlocking(product_rs485_channel_t channel, uint32_t timeoutUs);

/*!
 * @brief Force an available channel into receive mode without waiting for USART.
 *
 * Use this for initialization or explicit transfer abort only.
 */
status_t ProductRs485Direction_ForceReceive(product_rs485_channel_t channel);

#endif /* PRODUCT_RS485_DIRECTION_H_ */
