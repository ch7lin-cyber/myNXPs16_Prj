/*
 * Copyright 2026
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "l02_rs485_dma.h"

#include "ProductFeatureConfig.h"
#include "fsl_clock.h"
#include "fsl_ctimer.h"
#include "fsl_mrt.h"
#include "fsl_usart.h"
#include "fsl_usart_dma.h"
#include "peripherals.h"

#define L02_RS485_MRT_IRQ_PRIORITY       (5U)
#define L02_RS485_MRT_TURNAROUND_CHANNEL kMRT_Channel_2

typedef enum _l02_rs485_dma_tx_state
{
    kL02_Rs485DmaTxIdle = 0U,
    kL02_Rs485DmaTxActive,
    kL02_Rs485DmaTxDirectionReleasePending
} l02_rs485_dma_tx_state_t;

typedef enum _l02_rs485_dma_rx_state
{
    kL02_Rs485DmaRxIdle = 0U,
    kL02_Rs485DmaRxActive,
    kL02_Rs485DmaRxFrameReady
} l02_rs485_dma_rx_state_t;

typedef enum _l02_rs485_dma_rx_gap_state
{
    kL02_Rs485DmaRxGapIdle = 0U,
    kL02_Rs485DmaRxGapWaitT15,
    kL02_Rs485DmaRxGapWaitT35
} l02_rs485_dma_rx_gap_state_t;

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
    volatile bool rxDmaFullPending;
    l02_rs485_dma_rx_state_t rxState;
    l02_rs485_dma_rx_gap_state_t rxGapState;
    uint32_t rxTimeoutUs;
    uint32_t rxT15Us;
    uint32_t rxT35Us;
    uint32_t rxLastCount;
    uint32_t rxT15Count;
    size_t rxFrameLength;
    size_t rxCapacity;
    bool rxHasData;
    bool rxUsesRtuTiming;
    bool rxTimingError;
    bool rxFrameTimingError;
    bool initialized;
} l02_rs485_dma_context_t;

static const l02_rs485_dma_hardware_t s_rs485DmaHardware[kL02_Rs485ChannelCount] = {
    { UART0_FC0_PERIPHERAL, &UART0_FC0_USART_DMA_Handle, &UART0_FC0_TX_Handle, &UART0_FC0_RX_Handle },
    { UART1_FC1_PERIPHERAL, &UART1_FC1_USART_DMA_Handle, &UART1_FC1_TX_Handle, &UART1_FC1_RX_Handle },
};

static l02_rs485_dma_context_t s_rs485DmaContext[kL02_Rs485ChannelCount] = {
    { .channel = kL02_Rs485Channel0, .txState = kL02_Rs485DmaTxIdle,
      .rxTimeoutUs = L02_RS485_DMA_DEFAULT_FRAME_TIMEOUT_US },
    { .channel = kL02_Rs485Channel1, .txState = kL02_Rs485DmaTxIdle,
      .rxTimeoutUs = L02_RS485_DMA_DEFAULT_FRAME_TIMEOUT_US },
};

static volatile uint32_t s_rs485MrtEvents;
static uint32_t s_rs485MrtClockHz;
static bool s_rs485MrtInitialized;

static bool L02_Rs485Dma_IsChannelIndexValid(l02_rs485_channel_t channel)
{
    return ((uint32_t)channel < (uint32_t)kL02_Rs485ChannelCount);
}

static mrt_chnl_t L02_Rs485Dma_GetReceiveTimerChannel(l02_rs485_channel_t channel)
{
    return (channel == kL02_Rs485Channel0) ? kMRT_Channel_0 : kMRT_Channel_1;
}

static status_t L02_Rs485Dma_StartMrtTimer(mrt_chnl_t channel, uint32_t timeoutUs)
{
    uint64_t ticks;
    uint32_t interruptMask;
    uint32_t eventMask;

    if (!s_rs485MrtInitialized || (timeoutUs == 0U))
    {
        return kStatus_InvalidArgument;
    }

    ticks = (((uint64_t)timeoutUs * (uint64_t)s_rs485MrtClockHz) + 999999ULL) / 1000000ULL;
    if ((ticks == 0ULL) || (ticks > (uint64_t)MRT_CHANNEL_INTVAL_IVALUE_MASK))
    {
        return kStatus_OutOfRange;
    }

    eventMask = 1UL << (uint32_t)channel;
    interruptMask = DisableGlobalIRQ();
    MRT_StopTimer(MRT0_PERIPHERAL, channel);
    MRT_ClearStatusFlags(MRT0_PERIPHERAL, channel, (uint32_t)kMRT_TimerInterruptFlag);
    s_rs485MrtEvents &= ~eventMask;
    MRT_StartTimer(MRT0_PERIPHERAL, channel, (uint32_t)ticks);
    EnableGlobalIRQ(interruptMask);

    return kStatus_Success;
}

static void L02_Rs485Dma_StopMrtTimer(mrt_chnl_t channel)
{
    uint32_t interruptMask;
    uint32_t eventMask = 1UL << (uint32_t)channel;

    if (!s_rs485MrtInitialized)
    {
        return;
    }

    interruptMask = DisableGlobalIRQ();
    MRT_StopTimer(MRT0_PERIPHERAL, channel);
    MRT_ClearStatusFlags(MRT0_PERIPHERAL, channel, (uint32_t)kMRT_TimerInterruptFlag);
    s_rs485MrtEvents &= ~eventMask;
    EnableGlobalIRQ(interruptMask);
}

static bool L02_Rs485Dma_TakeMrtEvent(mrt_chnl_t channel)
{
    uint32_t interruptMask;
    uint32_t eventMask = 1UL << (uint32_t)channel;
    bool elapsed;

    interruptMask = DisableGlobalIRQ();
    elapsed = ((s_rs485MrtEvents & eventMask) != 0U);
    s_rs485MrtEvents &= ~eventMask;
    EnableGlobalIRQ(interruptMask);

    return elapsed;
}

static status_t L02_Rs485Dma_InitMrt(void)
{
    uint32_t channel;

    s_rs485MrtClockHz = CLOCK_GetFreq(kCLOCK_BusClk);
    if (s_rs485MrtClockHz == 0U)
    {
        return kStatus_Fail;
    }

    s_rs485MrtEvents = 0U;
    for (channel = 0U; channel < (uint32_t)FSL_FEATURE_MRT_NUMBER_OF_CHANNELS; channel++)
    {
        MRT_StopTimer(MRT0_PERIPHERAL, (mrt_chnl_t)channel);
        MRT_SetupChannelMode(MRT0_PERIPHERAL, (mrt_chnl_t)channel, kMRT_OneShotMode);
        MRT_ClearStatusFlags(MRT0_PERIPHERAL, (mrt_chnl_t)channel, (uint32_t)kMRT_TimerInterruptFlag);
        MRT_EnableInterrupts(MRT0_PERIPHERAL, (mrt_chnl_t)channel, (uint32_t)kMRT_TimerInterruptEnable);
    }

    NVIC_ClearPendingIRQ(MRT0_IRQn);
    NVIC_SetPriority(MRT0_IRQn, L02_RS485_MRT_IRQ_PRIORITY);
    s_rs485MrtInitialized = true;
    EnableIRQ(MRT0_IRQn);

    /* Retire the former dedicated 100 us communication tick. */
    CTIMER_StopTimer(COMM_CTIMER4_PERIPHERAL);
    CTIMER_DisableInterrupts(COMM_CTIMER4_PERIPHERAL, (uint32_t)kCTIMER_Match1InterruptEnable);
    CTIMER_ClearStatusFlags(COMM_CTIMER4_PERIPHERAL, (uint32_t)kCTIMER_Match1Flag);
    DisableIRQ(COMM_CTIMER4_TIMER_IRQN);
    NVIC_ClearPendingIRQ(COMM_CTIMER4_TIMER_IRQN);

    return kStatus_Success;
}

void MRT0_DriverIRQHandler(void)
{
    uint32_t channel;

    for (channel = 0U; channel < (uint32_t)FSL_FEATURE_MRT_NUMBER_OF_CHANNELS; channel++)
    {
        mrt_chnl_t mrtChannel = (mrt_chnl_t)channel;

        if ((MRT_GetStatusFlags(MRT0_PERIPHERAL, mrtChannel) &
             (uint32_t)kMRT_TimerInterruptFlag) != 0U)
        {
            MRT_ClearStatusFlags(MRT0_PERIPHERAL, mrtChannel, (uint32_t)kMRT_TimerInterruptFlag);
            s_rs485MrtEvents |= 1UL << channel;
        }
    }

    SDK_ISR_EXIT_BARRIER;
}

static void L02_Rs485Dma_Callback(USART_Type *base,
                                  usart_dma_handle_t *handle,
                                  status_t status,
                                  void *userData)
{
    l02_rs485_dma_context_t *context = (l02_rs485_dma_context_t *)userData;

    (void)base;
    (void)handle;

    if (context != NULL)
    {
        if (status == kStatus_USART_TxIdle)
        {
            context->txState = kL02_Rs485DmaTxDirectionReleasePending;
        }
        else if (status == kStatus_USART_RxIdle)
        {
            context->rxDmaFullPending = true;
        }
        else
        {
            /* No action. */
        }
    }
}

static status_t L02_Rs485Dma_InitChannel(l02_rs485_channel_t channel)
{
    const l02_rs485_dma_hardware_t *hardware = &s_rs485DmaHardware[(uint32_t)channel];
    l02_rs485_dma_context_t *context = &s_rs485DmaContext[(uint32_t)channel];
    status_t status;

    context->txState = kL02_Rs485DmaTxIdle;
    context->rxState = kL02_Rs485DmaRxIdle;
    context->rxGapState = kL02_Rs485DmaRxGapIdle;
    context->rxDmaFullPending = false;
    context->rxTimeoutUs = L02_RS485_DMA_DEFAULT_FRAME_TIMEOUT_US;
    context->rxT15Us = 0U;
    context->rxT35Us = 0U;
    context->rxLastCount = 0U;
    context->rxT15Count = 0U;
    context->rxFrameLength = 0U;
    context->rxCapacity = 0U;
    context->rxHasData = false;
    context->rxUsesRtuTiming = false;
    context->rxTimingError = false;
    context->rxFrameTimingError = false;
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

static void L02_Rs485Dma_FinalizeReceive(l02_rs485_dma_context_t *context, size_t length)
{
    L02_Rs485Dma_StopMrtTimer(L02_Rs485Dma_GetReceiveTimerChannel(context->channel));
    context->rxFrameLength = length;
    context->rxFrameTimingError = context->rxTimingError;
    context->rxState = (length != 0U) ? kL02_Rs485DmaRxFrameReady : kL02_Rs485DmaRxIdle;
    context->rxGapState = kL02_Rs485DmaRxGapIdle;
    context->rxDmaFullPending = false;
    context->rxHasData = false;
}

static void L02_Rs485Dma_StopReceiveAndFinalize(
    l02_rs485_dma_context_t *context,
    uint32_t receivedCount)
{
    const l02_rs485_dma_hardware_t *hardware =
        &s_rs485DmaHardware[(uint32_t)context->channel];

    USART_EnableRxDMA(hardware->usart, false);
    (void)USART_TransferGetReceiveCountDMA(hardware->usart, hardware->usartDmaHandle, &receivedCount);
    USART_TransferAbortReceiveDMA(hardware->usart, hardware->usartDmaHandle);
    L02_Rs485Dma_FinalizeReceive(context, (size_t)receivedCount);
}

static void L02_Rs485Dma_RestartReceiveGapTimer(l02_rs485_dma_context_t *context)
{
    uint32_t timeoutUs;

    if (context->rxUsesRtuTiming)
    {
        context->rxGapState = kL02_Rs485DmaRxGapWaitT15;
        timeoutUs = context->rxT15Us;
    }
    else
    {
        context->rxGapState = kL02_Rs485DmaRxGapIdle;
        timeoutUs = context->rxTimeoutUs;
    }

    if (L02_Rs485Dma_StartMrtTimer(
            L02_Rs485Dma_GetReceiveTimerChannel(context->channel),
            timeoutUs) != kStatus_Success)
    {
        context->rxTimingError = true;
    }
}

static void L02_Rs485Dma_ProcessReceive(l02_rs485_dma_context_t *context)
{
    const l02_rs485_dma_hardware_t *hardware;
    mrt_chnl_t timerChannel;
    uint32_t receivedCount;
    status_t countStatus;
    bool timerElapsed;

    if (context->rxState != kL02_Rs485DmaRxActive)
    {
        return;
    }

    hardware = &s_rs485DmaHardware[(uint32_t)context->channel];
    timerChannel = L02_Rs485Dma_GetReceiveTimerChannel(context->channel);

    if (context->rxDmaFullPending)
    {
        L02_Rs485Dma_FinalizeReceive(context, context->rxCapacity);
        return;
    }

    timerElapsed = L02_Rs485Dma_TakeMrtEvent(timerChannel);
    countStatus = USART_TransferGetReceiveCountDMA(
        hardware->usart, hardware->usartDmaHandle, &receivedCount);
    if (countStatus != kStatus_Success)
    {
        return;
    }

    if (timerElapsed && context->rxHasData)
    {
        if (!context->rxUsesRtuTiming)
        {
            if (receivedCount == context->rxLastCount)
            {
                L02_Rs485Dma_StopReceiveAndFinalize(context, receivedCount);
                return;
            }
        }
        else if (context->rxGapState == kL02_Rs485DmaRxGapWaitT15)
        {
            if (receivedCount == context->rxLastCount)
            {
                context->rxT15Count = receivedCount;
                context->rxGapState = kL02_Rs485DmaRxGapWaitT35;
                if (L02_Rs485Dma_StartMrtTimer(
                        timerChannel,
                        context->rxT35Us - context->rxT15Us) != kStatus_Success)
                {
                    context->rxTimingError = true;
                }
                return;
            }
        }
        else if (context->rxGapState == kL02_Rs485DmaRxGapWaitT35)
        {
            if (receivedCount == context->rxT15Count)
            {
                L02_Rs485Dma_StopReceiveAndFinalize(context, receivedCount);
                return;
            }
            context->rxTimingError = true;
        }
        else
        {
            context->rxTimingError = true;
        }
    }

    if (receivedCount != context->rxLastCount)
    {
        if (context->rxUsesRtuTiming &&
            (context->rxGapState == kL02_Rs485DmaRxGapWaitT35))
        {
            context->rxTimingError = true;
        }

        context->rxLastCount = receivedCount;
        context->rxHasData = (receivedCount != 0U);
        L02_Rs485Dma_RestartReceiveGapTimer(context);
    }
}

status_t L02_Rs485Dma_Init(void)
{
    status_t status = L02_Rs485Dma_InitMrt();

    if (status != kStatus_Success)
    {
        return status;
    }

#if (PRODUCT_FC0_MODE == PRODUCT_FC0_MODE_RS485)
    status = L02_Rs485Dma_InitChannel(kL02_Rs485Channel0);
    if (status != kStatus_Success)
    {
        return status;
    }
#endif

    return L02_Rs485Dma_InitChannel(kL02_Rs485Channel1);
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
    if ((context->txState != kL02_Rs485DmaTxIdle) ||
        (context->rxState != kL02_Rs485DmaRxIdle))
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
    context->txState = kL02_Rs485DmaTxActive;
    status = USART_TransferSendDMA(hardware->usart, hardware->usartDmaHandle, &transfer);
    if (status != kStatus_Success)
    {
        context->txState = kL02_Rs485DmaTxIdle;
        (void)L02_Rs485Direction_ForceReceive(channel);
    }

    return status;
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

status_t L02_Rs485Dma_SetReceiveTimeoutUs(l02_rs485_channel_t channel, uint32_t timeoutUs)
{
    l02_rs485_dma_context_t *context;
    uint64_t ticks;

    if ((timeoutUs == 0U) || !L02_Rs485Dma_IsChannelIndexValid(channel))
    {
        return kStatus_InvalidArgument;
    }

    context = &s_rs485DmaContext[(uint32_t)channel];
    if (!context->initialized)
    {
        return kStatus_InvalidArgument;
    }
    if (context->rxState == kL02_Rs485DmaRxActive)
    {
        return kStatus_Busy;
    }

    ticks = (((uint64_t)timeoutUs * (uint64_t)s_rs485MrtClockHz) + 999999ULL) / 1000000ULL;
    if ((ticks == 0ULL) || (ticks > (uint64_t)MRT_CHANNEL_INTVAL_IVALUE_MASK))
    {
        return kStatus_OutOfRange;
    }

    context->rxTimeoutUs = timeoutUs;
    context->rxUsesRtuTiming = false;
    return kStatus_Success;
}

status_t L02_Rs485Dma_SetRtuReceiveTimingUs(
    l02_rs485_channel_t channel,
    uint32_t t15Us,
    uint32_t t35Us)
{
    l02_rs485_dma_context_t *context;
    uint64_t maxTicks;

    if (!L02_Rs485Dma_IsChannelIndexValid(channel) ||
        (t15Us == 0U) || (t35Us <= t15Us))
    {
        return kStatus_InvalidArgument;
    }

    context = &s_rs485DmaContext[(uint32_t)channel];
    if (!context->initialized)
    {
        return kStatus_InvalidArgument;
    }
    if (context->rxState == kL02_Rs485DmaRxActive)
    {
        return kStatus_Busy;
    }

    maxTicks = (((uint64_t)t35Us * (uint64_t)s_rs485MrtClockHz) + 999999ULL) / 1000000ULL;
    if ((maxTicks == 0ULL) || (maxTicks > (uint64_t)MRT_CHANNEL_INTVAL_IVALUE_MASK))
    {
        return kStatus_OutOfRange;
    }

    context->rxT15Us = t15Us;
    context->rxT35Us = t35Us;
    context->rxUsesRtuTiming = true;
    return kStatus_Success;
}

status_t L02_Rs485Dma_StartReceive(l02_rs485_channel_t channel, uint8_t *buffer, size_t capacity)
{
    l02_rs485_dma_context_t *context;
    const l02_rs485_dma_hardware_t *hardware;
    usart_transfer_t transfer;
    status_t status;

    if ((buffer == NULL) || (capacity == 0U) || !L02_Rs485Dma_IsChannelIndexValid(channel))
    {
        return kStatus_InvalidArgument;
    }

    context = &s_rs485DmaContext[(uint32_t)channel];
    hardware = &s_rs485DmaHardware[(uint32_t)channel];
    if (!context->initialized)
    {
        return kStatus_InvalidArgument;
    }
    if ((context->txState != kL02_Rs485DmaTxIdle) ||
        (context->rxState != kL02_Rs485DmaRxIdle))
    {
        return kStatus_Busy;
    }

    transfer.data = buffer;
    transfer.dataSize = capacity;

    L02_Rs485Dma_StopMrtTimer(L02_Rs485Dma_GetReceiveTimerChannel(channel));
    context->rxState = kL02_Rs485DmaRxActive;
    context->rxGapState = kL02_Rs485DmaRxGapIdle;
    context->rxDmaFullPending = false;
    context->rxLastCount = 0U;
    context->rxT15Count = 0U;
    context->rxFrameLength = 0U;
    context->rxCapacity = capacity;
    context->rxHasData = false;
    context->rxTimingError = false;
    context->rxFrameTimingError = false;

    status = USART_TransferReceiveDMA(hardware->usart, hardware->usartDmaHandle, &transfer);
    if (status != kStatus_Success)
    {
        context->rxState = kL02_Rs485DmaRxIdle;
        context->rxCapacity = 0U;
    }

    return status;
}

status_t L02_Rs485Dma_GetReceiveCount(l02_rs485_channel_t channel, size_t *count)
{
    l02_rs485_dma_context_t *context;
    const l02_rs485_dma_hardware_t *hardware;
    uint32_t receivedCount;
    status_t status;

    if ((count == NULL) || !L02_Rs485Dma_IsChannelIndexValid(channel))
    {
        return kStatus_InvalidArgument;
    }

    context = &s_rs485DmaContext[(uint32_t)channel];
    hardware = &s_rs485DmaHardware[(uint32_t)channel];
    if (!context->initialized)
    {
        return kStatus_InvalidArgument;
    }
    if (context->rxState != kL02_Rs485DmaRxActive)
    {
        return kStatus_NoTransferInProgress;
    }

    status = USART_TransferGetReceiveCountDMA(hardware->usart, hardware->usartDmaHandle, &receivedCount);
    if (status == kStatus_Success)
    {
        *count = (size_t)receivedCount;
    }

    return status;
}

status_t L02_Rs485Dma_CompleteReceive(l02_rs485_channel_t channel, size_t frameLength)
{
    l02_rs485_dma_context_t *context;
    const l02_rs485_dma_hardware_t *hardware;
    size_t receivedCount;
    status_t status;

    if ((frameLength == 0U) || !L02_Rs485Dma_IsChannelIndexValid(channel))
    {
        return kStatus_InvalidArgument;
    }

    context = &s_rs485DmaContext[(uint32_t)channel];
    hardware = &s_rs485DmaHardware[(uint32_t)channel];
    if (!context->initialized)
    {
        return kStatus_InvalidArgument;
    }
    if (context->rxState != kL02_Rs485DmaRxActive)
    {
        return kStatus_NoTransferInProgress;
    }

    status = L02_Rs485Dma_GetReceiveCount(channel, &receivedCount);
    if (status != kStatus_Success)
    {
        if ((status == kStatus_NoTransferInProgress) &&
            context->rxDmaFullPending &&
            (frameLength <= context->rxCapacity))
        {
            L02_Rs485Dma_FinalizeReceive(context, frameLength);
            return kStatus_Success;
        }
        return status;
    }
    if (frameLength > receivedCount)
    {
        return kStatus_OutOfRange;
    }

    USART_EnableRxDMA(hardware->usart, false);
    USART_TransferAbortReceiveDMA(hardware->usart, hardware->usartDmaHandle);
    L02_Rs485Dma_FinalizeReceive(context, frameLength);
    return kStatus_Success;
}

status_t L02_Rs485Dma_TakeReceivedFrame(l02_rs485_channel_t channel, size_t *length)
{
    l02_rs485_dma_context_t *context;

    if ((length == NULL) || !L02_Rs485Dma_IsChannelIndexValid(channel))
    {
        return kStatus_InvalidArgument;
    }

    context = &s_rs485DmaContext[(uint32_t)channel];
    if (!context->initialized)
    {
        return kStatus_InvalidArgument;
    }
    if (context->rxState != kL02_Rs485DmaRxFrameReady)
    {
        return kStatus_NoData;
    }

    *length = context->rxFrameLength;
    context->rxFrameLength = 0U;
    context->rxCapacity = 0U;
    context->rxState = kL02_Rs485DmaRxIdle;
    return kStatus_Success;
}

status_t L02_Rs485Dma_GetLastReceiveTimingError(
    l02_rs485_channel_t channel,
    bool *timingError)
{
    l02_rs485_dma_context_t *context;

    if ((timingError == NULL) || !L02_Rs485Dma_IsChannelIndexValid(channel))
    {
        return kStatus_InvalidArgument;
    }

    context = &s_rs485DmaContext[(uint32_t)channel];
    if (!context->initialized)
    {
        return kStatus_InvalidArgument;
    }

    *timingError = context->rxFrameTimingError;
    return kStatus_Success;
}

status_t L02_Rs485Dma_CancelReceive(l02_rs485_channel_t channel)
{
    l02_rs485_dma_context_t *context;
    const l02_rs485_dma_hardware_t *hardware;

    if (!L02_Rs485Dma_IsChannelIndexValid(channel))
    {
        return kStatus_InvalidArgument;
    }

    context = &s_rs485DmaContext[(uint32_t)channel];
    hardware = &s_rs485DmaHardware[(uint32_t)channel];
    if (!context->initialized)
    {
        return kStatus_InvalidArgument;
    }

    L02_Rs485Dma_StopMrtTimer(L02_Rs485Dma_GetReceiveTimerChannel(channel));
    if (context->rxState == kL02_Rs485DmaRxActive)
    {
        USART_EnableRxDMA(hardware->usart, false);
        USART_TransferAbortReceiveDMA(hardware->usart, hardware->usartDmaHandle);
    }

    context->rxState = kL02_Rs485DmaRxIdle;
    context->rxGapState = kL02_Rs485DmaRxGapIdle;
    context->rxDmaFullPending = false;
    context->rxLastCount = 0U;
    context->rxFrameLength = 0U;
    context->rxCapacity = 0U;
    context->rxHasData = false;
    context->rxTimingError = false;
    context->rxFrameTimingError = false;
    return kStatus_Success;
}

status_t L02_Rs485Dma_StartTurnaroundDelayUs(uint32_t delayUs)
{
    return L02_Rs485Dma_StartMrtTimer(L02_RS485_MRT_TURNAROUND_CHANNEL, delayUs);
}

status_t L02_Rs485Dma_TakeTurnaroundDelayElapsed(bool *elapsed)
{
    if ((elapsed == NULL) || !s_rs485MrtInitialized)
    {
        return kStatus_InvalidArgument;
    }

    *elapsed = L02_Rs485Dma_TakeMrtEvent(L02_RS485_MRT_TURNAROUND_CHANNEL);
    return kStatus_Success;
}

void L02_Rs485Dma_Process(void)
{
    uint32_t index;

    for (index = 0U; index < (uint32_t)kL02_Rs485ChannelCount; index++)
    {
        l02_rs485_dma_context_t *context = &s_rs485DmaContext[index];

        if (!context->initialized)
        {
            continue;
        }

        if (context->txState == kL02_Rs485DmaTxDirectionReleasePending)
        {
            if (L02_Rs485Direction_EndTransmitBlocking(context->channel, 1U) == kStatus_Success)
            {
                context->txState = kL02_Rs485DmaTxIdle;
            }
        }

        L02_Rs485Dma_ProcessReceive(context);
    }
}
