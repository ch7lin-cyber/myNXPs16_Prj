/*
 * Copyright 2026
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "product_rs485_direction.h"

#include "fsl_gpio.h"
#include "fsl_usart.h"
#include "peripherals.h"
#include "pin_mux.h"

#define PRODUCT_RS485_DIR_RECEIVE_LEVEL (0U)
#define PRODUCT_RS485_DIR_TRANSMIT_LEVEL (1U)
#define PRODUCT_RS485_DEFAULT_PRE_TX_DELAY_US  (2U)
#define PRODUCT_RS485_DEFAULT_POST_TX_DELAY_US (2U)

typedef struct _product_rs485_channel_hardware
{
    GPIO_Type *gpio;
    uint32_t gpioPort;
    uint32_t gpioPin;
    USART_Type *usart;
} product_rs485_channel_hardware_t;

static const product_rs485_channel_hardware_t s_rs485Hardware[kProductRs485ChannelCount] = {
    {
        BOARD_INITDEBUG_UARTPINS_FC0_DIR_GPIO,
        BOARD_INITDEBUG_UARTPINS_FC0_DIR_PORT,
        BOARD_INITDEBUG_UARTPINS_FC0_DIR_PIN,
        UART0_FC0_PERIPHERAL,
    },
    {
        BOARD_INITUART2PINS_FC1_DIR_GPIO,
        BOARD_INITUART2PINS_FC1_DIR_PORT,
        BOARD_INITUART2PINS_FC1_DIR_PIN,
        UART1_FC1_PERIPHERAL,
    },
};

static product_rs485_timing_t s_rs485Timing[kProductRs485ChannelCount] = {
    {PRODUCT_RS485_DEFAULT_PRE_TX_DELAY_US, PRODUCT_RS485_DEFAULT_POST_TX_DELAY_US},
    {PRODUCT_RS485_DEFAULT_PRE_TX_DELAY_US, PRODUCT_RS485_DEFAULT_POST_TX_DELAY_US},
};

static bool ProductRs485Direction_IsAvailable(product_rs485_channel_t channel)
{
    if ((uint32_t)channel >= (uint32_t)kProductRs485ChannelCount)
    {
        return false;
    }

    return true;
}

static void ProductRs485Direction_Write(product_rs485_channel_t channel, uint8_t level)
{
    const product_rs485_channel_hardware_t *hardware = &s_rs485Hardware[(uint32_t)channel];

    GPIO_PinWrite(hardware->gpio, hardware->gpioPort, hardware->gpioPin, level);
}

static void ProductRs485Direction_Delay(uint32_t delayUs)
{
    if (delayUs != 0U)
    {
        SDK_DelayAtLeastUs(delayUs, SystemCoreClock);
    }
}

status_t ProductRs485Direction_Init(void)
{
    ProductRs485Direction_Write(kProductRs485Channel0, PRODUCT_RS485_DIR_RECEIVE_LEVEL);
    ProductRs485Direction_Write(kProductRs485Channel1, PRODUCT_RS485_DIR_RECEIVE_LEVEL);

    return kStatus_Success;
}

status_t ProductRs485Direction_SetTiming(product_rs485_channel_t channel, const product_rs485_timing_t *timing)
{
    if ((timing == NULL) || !ProductRs485Direction_IsAvailable(channel))
    {
        return kStatus_InvalidArgument;
    }

    s_rs485Timing[(uint32_t)channel] = *timing;
    return kStatus_Success;
}

status_t ProductRs485Direction_BeginTransmit(product_rs485_channel_t channel)
{
    if (!ProductRs485Direction_IsAvailable(channel))
    {
        return kStatus_InvalidArgument;
    }

    ProductRs485Direction_Write(channel, PRODUCT_RS485_DIR_TRANSMIT_LEVEL);
    ProductRs485Direction_Delay(s_rs485Timing[(uint32_t)channel].preTxDelayUs);

    return kStatus_Success;
}

status_t ProductRs485Direction_IsTransmitComplete(product_rs485_channel_t channel, bool *isComplete)
{
    if ((isComplete == NULL) || !ProductRs485Direction_IsAvailable(channel))
    {
        return kStatus_InvalidArgument;
    }

    *isComplete =
        ((s_rs485Hardware[(uint32_t)channel].usart->STAT & USART_STAT_TXIDLE_MASK) != 0U);

    return kStatus_Success;
}

status_t ProductRs485Direction_EndTransmitBlocking(product_rs485_channel_t channel, uint32_t timeoutUs)
{
    bool isComplete;

    if (!ProductRs485Direction_IsAvailable(channel))
    {
        return kStatus_InvalidArgument;
    }

    while (timeoutUs != 0U)
    {
        (void)ProductRs485Direction_IsTransmitComplete(channel, &isComplete);
        if (isComplete)
        {
            ProductRs485Direction_Delay(s_rs485Timing[(uint32_t)channel].postTxDelayUs);
            ProductRs485Direction_Write(channel, PRODUCT_RS485_DIR_RECEIVE_LEVEL);
            return kStatus_Success;
        }

        SDK_DelayAtLeastUs(1U, SystemCoreClock);
        timeoutUs--;
    }

    return kStatus_Timeout;
}

status_t ProductRs485Direction_ForceReceive(product_rs485_channel_t channel)
{
    if (!ProductRs485Direction_IsAvailable(channel))
    {
        return kStatus_InvalidArgument;
    }

    ProductRs485Direction_Write(channel, PRODUCT_RS485_DIR_RECEIVE_LEVEL);
    return kStatus_Success;
}
