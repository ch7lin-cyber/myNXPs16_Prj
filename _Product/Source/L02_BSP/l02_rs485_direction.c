/*
 * Copyright 2026
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "l02_rs485_direction.h"

#include "ProductFeatureConfig.h"
#include "fsl_gpio.h"
#include "fsl_usart.h"
#include "peripherals.h"
#include "pin_mux.h"

#define L02_RS485_DIR_RECEIVE_LEVEL (0U)
#define L02_RS485_DIR_TRANSMIT_LEVEL (1U)
#define L02_RS485_DEFAULT_PRE_TX_DELAY_US  (2U)
#define L02_RS485_DEFAULT_POST_TX_DELAY_US (2U)

typedef struct _l02_rs485_channel_hardware
{
    GPIO_Type *gpio;
    uint32_t gpioPort;
    uint32_t gpioPin;
    USART_Type *usart;
} l02_rs485_channel_hardware_t;

static const l02_rs485_channel_hardware_t s_rs485Hardware[kL02_Rs485ChannelCount] = {
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

static l02_rs485_timing_t s_rs485Timing[kL02_Rs485ChannelCount] = {
    {L02_RS485_DEFAULT_PRE_TX_DELAY_US, L02_RS485_DEFAULT_POST_TX_DELAY_US},
    {L02_RS485_DEFAULT_PRE_TX_DELAY_US, L02_RS485_DEFAULT_POST_TX_DELAY_US},
};

static bool L02_Rs485Direction_IsAvailable(l02_rs485_channel_t channel)
{
    if ((uint32_t)channel >= (uint32_t)kL02_Rs485ChannelCount)
    {
        return false;
    }

#if (PRODUCT_FC0_MODE == PRODUCT_FC0_MODE_DEBUG_CONSOLE)
    if (channel == kL02_Rs485Channel0)
    {
        return false;
    }
#endif

    return true;
}

static void L02_Rs485Direction_Write(l02_rs485_channel_t channel, uint8_t level)
{
    const l02_rs485_channel_hardware_t *hardware = &s_rs485Hardware[(uint32_t)channel];

    GPIO_PinWrite(hardware->gpio, hardware->gpioPort, hardware->gpioPin, level);
}

static void L02_Rs485Direction_Delay(uint32_t delayUs)
{
    if (delayUs != 0U)
    {
        SDK_DelayAtLeastUs(delayUs, SystemCoreClock);
    }
}

status_t L02_Rs485Direction_Init(void)
{
#if (PRODUCT_FC0_MODE == PRODUCT_FC0_MODE_RS485)
    L02_Rs485Direction_Write(kL02_Rs485Channel0, L02_RS485_DIR_RECEIVE_LEVEL);
#endif
    L02_Rs485Direction_Write(kL02_Rs485Channel1, L02_RS485_DIR_RECEIVE_LEVEL);

    return kStatus_Success;
}

status_t L02_Rs485Direction_SetTiming(l02_rs485_channel_t channel, const l02_rs485_timing_t *timing)
{
    if ((timing == NULL) || !L02_Rs485Direction_IsAvailable(channel))
    {
        return kStatus_InvalidArgument;
    }

    s_rs485Timing[(uint32_t)channel] = *timing;
    return kStatus_Success;
}

status_t L02_Rs485Direction_BeginTransmit(l02_rs485_channel_t channel)
{
    if (!L02_Rs485Direction_IsAvailable(channel))
    {
        return kStatus_InvalidArgument;
    }

    L02_Rs485Direction_Write(channel, L02_RS485_DIR_TRANSMIT_LEVEL);
    L02_Rs485Direction_Delay(s_rs485Timing[(uint32_t)channel].preTxDelayUs);

    return kStatus_Success;
}

status_t L02_Rs485Direction_IsTransmitComplete(l02_rs485_channel_t channel, bool *isComplete)
{
    if ((isComplete == NULL) || !L02_Rs485Direction_IsAvailable(channel))
    {
        return kStatus_InvalidArgument;
    }

    *isComplete =
        ((s_rs485Hardware[(uint32_t)channel].usart->STAT & USART_STAT_TXIDLE_MASK) != 0U);

    return kStatus_Success;
}

status_t L02_Rs485Direction_EndTransmitBlocking(l02_rs485_channel_t channel, uint32_t timeoutUs)
{
    bool isComplete;

    if (!L02_Rs485Direction_IsAvailable(channel))
    {
        return kStatus_InvalidArgument;
    }

    while (timeoutUs != 0U)
    {
        (void)L02_Rs485Direction_IsTransmitComplete(channel, &isComplete);
        if (isComplete)
        {
            L02_Rs485Direction_Delay(s_rs485Timing[(uint32_t)channel].postTxDelayUs);
            L02_Rs485Direction_Write(channel, L02_RS485_DIR_RECEIVE_LEVEL);
            return kStatus_Success;
        }

        SDK_DelayAtLeastUs(1U, SystemCoreClock);
        timeoutUs--;
    }

    return kStatus_Timeout;
}

status_t L02_Rs485Direction_ForceReceive(l02_rs485_channel_t channel)
{
    if (!L02_Rs485Direction_IsAvailable(channel))
    {
        return kStatus_InvalidArgument;
    }

    L02_Rs485Direction_Write(channel, L02_RS485_DIR_RECEIVE_LEVEL);
    return kStatus_Success;
}
