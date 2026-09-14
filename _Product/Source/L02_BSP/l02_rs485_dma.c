/*
 * Copyright 2026
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "l02_rs485_dma.h"

#include "ProductFeatureConfig.h"
#include "fsl_usart_dma.h"
#include "peripherals.h"

typedef enum _l02_rs485_dma_tx_state
{
    kL02_Rs485DmaTxIdle = 0U,
    kL02_Rs485DmaTxActive,
    kL02_Rs485DmaTxDirectionReleasePending
} l02_rs485_dma_tx_state_t;

typedef struct _l02_rs485_dma_hardware
{
    USART_Type *usart;
    usart_dma_handle_t *usartDmaHandle;
    dma_handle_t *txDmaHandle;
    dma_handle_t *rxDmaHandle;
} l02_rs485_dma_hardware_t;

typedef struct _l02_rs485_dma_context
{
    l02_rs485_channel_t channel;
    volatile l02_rs485_dma_tx_state_t txState;
    bool initialized;
} l02_rs485_dma_context_t;

static const l02_rs485_dma_hardware_t s_rs485DmaHardware[kL02_Rs485ChannelCount] = {
    {
        UART0_FC0_PERIPHERAL,
        &UART0_FC0_USART_DMA_Handle,
        &UART0_FC0_TX_Handle,
        &UART0_FC0_RX_Handle,
    },
    {
        UART1_FC1_PERIPHERAL,
        &UART1_FC1_USART_DMA_Handle,
        &UART1_FC1_TX_Handle,
        &UART1_FC1_RX_Handle,
    },
};

static l02_rs485_dma_context_t s_rs485DmaContext[kL02_Rs485ChannelCount] = {
    {kL02_Rs485Channel0, kL02_Rs485DmaTxIdle, false},
    {kL02_Rs485Channel1, kL02_Rs485DmaTxIdle, false},
};

static bool L02_Rs485Dma_IsChannelIndexValid(l02_rs485_channel_t channel)
{
    return ((uint32_t)channel < (uint32_t)kL02_Rs485ChannelCount);
}

static void L02_Rs485Dma_Callback(USART_Type *base,
                                  usart_dma_handle_t *handle,
                                  status_t status,
                                  void *userData)
{
    l02_rs485_dma_context_t *context = (l02_rs485_dma_context_t *)userData;

    (void)base;
    (void)handle;

    if ((context != NULL) && (status == kStatus_USART_TxIdle))
    {
        /*
         * ISR rule: only publish the event. Post-TX delay and GPIO access are
         * intentionally deferred to L02_Rs485Dma_Process().
         */
        context->txState = kL02_Rs485DmaTxDirectionReleasePending;
    }
}

static status_t L02_Rs485Dma_InitChannel(l02_rs485_channel_t channel)
{
    const l02_rs485_dma_hardware_t *hardware = &s_rs485DmaHardware[(uint32_t)channel];
    l02_rs485_dma_context_t *context = &s_rs485DmaContext[(uint32_t)channel];
    status_t status;

    context->txState = kL02_Rs485DmaTxIdle;
    context->initialized = false;

    status = USART_TransferCreateHandleDMA(hardware->usart,
                                           hardware->usartDmaHandle,
                                           L02_Rs485Dma_Callback,
                                           context,
                                           hardware->txDmaHandle,
                                           hardware->rxDmaHandle);
    if (status == kStatus_Success)
    {
        context->initialized = true;
    }

    return status;
}

status_t L02_Rs485Dma_Init(void)
{
    status_t status;

#if (PRODUCT_FC0_MODE == PRODUCT_FC0_MODE_RS485)
    status = L02_Rs485Dma_InitChannel(kL02_Rs485Channel0);
    if (status != kStatus_Success)
    {
        return status;
    }
#endif

    status = L02_Rs485Dma_InitChannel(kL02_Rs485Channel1);
    return status;
}

status_t L02_Rs485Dma_SendAsync(l02_rs485_channel_t channel, const uint8_t *data, size_t length)
{
    l02_rs485_dma_context_t *context;
    const l02_rs485_dma_hardware_t *hardware;
    usart_transfer_t transfer;
    status_t status;

    if ((data == NULL) || (length == 0U) || !L02_Rs485Dma_IsChannelIndexValid(channel))
    {
        return kStatus_InvalidArgument;
    }

    context = &s_rs485DmaContext[(uint32_t)channel];
    hardware = &s_rs485DmaHardware[(uint32_t)channel];

    if (!context->initialized)
    {
        return kStatus_InvalidArgument;
    }

    if (context->txState != kL02_Rs485DmaTxIdle)
    {
        return kStatus_Busy;
    }

    status = L02_Rs485Direction_BeginTransmit(channel);
    if (status != kStatus_Success)
    {
        return status;
    }

    transfer.txData = (uint8_t *)data;
    transfer.dataSize = length;

    /*
     * Set active before enabling DMA. Otherwise a very short transfer could
     * complete in an interrupt before the state is updated here.
     */
    context->txState = kL02_Rs485DmaTxActive;
    status = USART_TransferSendDMA(hardware->usart, hardware->usartDmaHandle, &transfer);
    if (status != kStatus_Success)
    {
        context->txState = kL02_Rs485DmaTxIdle;
        (void)L02_Rs485Direction_ForceReceive(channel);
    }

    return status;
}

void L02_Rs485Dma_Process(void)
{
    uint32_t index;

    for (index = 0U; index < (uint32_t)kL02_Rs485ChannelCount; index++)
    {
        l02_rs485_dma_context_t *context = &s_rs485DmaContext[index];

        if (context->initialized &&
            (context->txState == kL02_Rs485DmaTxDirectionReleasePending))
        {
            /*
             * The SDK callback is raised by the USART TXIDLE IRQ, so one
             * microsecond of timeout is only a defensive register re-check.
             */
            if (L02_Rs485Direction_EndTransmitBlocking(context->channel, 1U) == kStatus_Success)
            {
                context->txState = kL02_Rs485DmaTxIdle;
            }
        }
    }
}

status_t L02_Rs485Dma_IsTransmitBusy(l02_rs485_channel_t channel, bool *isBusy)
{
    l02_rs485_dma_context_t *context;

    if ((isBusy == NULL) || !L02_Rs485Dma_IsChannelIndexValid(channel))
    {
        return kStatus_InvalidArgument;
    }

    context = &s_rs485DmaContext[(uint32_t)channel];
    if (!context->initialized)
    {
        return kStatus_InvalidArgument;
    }

    *isBusy = (context->txState != kL02_Rs485DmaTxIdle);
    return kStatus_Success;
}
