/*
 * Copyright 2026
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "product_rs485_dma.h"

#include "HalSerial.h"
#include "fsl_clock.h"
#include "fsl_ctimer.h"
#include "fsl_mrt.h"
#include "fsl_usart.h"
#include "fsl_usart_dma.h"
#include "peripherals.h"

#define PRODUCT_RS485_MRT_IRQ_PRIORITY       (5U)
#define PRODUCT_RS485_MRT_TURNAROUND_CHANNEL kMRT_Channel_2

typedef enum _product_rs485_dma_tx_state
{
    kProductRs485DmaTxIdle = 0U,
    kProductRs485DmaTxActive,
    kProductRs485DmaTxDirectionReleasePending
} product_rs485_dma_tx_state_t;

typedef enum _product_rs485_dma_rx_state
{
    kProductRs485DmaRxIdle = 0U,
    kProductRs485DmaRxActive,
    kProductRs485DmaRxFrameReady
} product_rs485_dma_rx_state_t;

typedef enum _product_rs485_dma_rx_gap_state
{
    kProductRs485DmaRxGapIdle = 0U,
    kProductRs485DmaRxGapWaitT15,
    kProductRs485DmaRxGapWaitT35
} product_rs485_dma_rx_gap_state_t;

typedef struct _product_rs485_dma_hardware
{
    USART_Type *usart;
    usart_dma_handle_t *usartDmaHandle;
    dma_handle_t *txDmaHandle;
    dma_handle_t *rxDmaHandle;
} product_rs485_dma_hardware_t;

typedef struct _product_rs485_dma_context
{
    product_rs485_channel_t channel;
    volatile product_rs485_dma_tx_state_t txState;
    volatile bool rxDmaFullPending;
    product_rs485_dma_rx_state_t rxState;
    product_rs485_dma_rx_gap_state_t rxGapState;
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
} product_rs485_dma_context_t;

static const product_rs485_dma_hardware_t s_rs485DmaHardware[kProductRs485ChannelCount] = {
    { UART0_FC0_PERIPHERAL, &UART0_FC0_USART_DMA_Handle, &UART0_FC0_TX_Handle, &UART0_FC0_RX_Handle },
    { UART1_FC1_PERIPHERAL, &UART1_FC1_USART_DMA_Handle, &UART1_FC1_TX_Handle, &UART1_FC1_RX_Handle },
};

static product_rs485_dma_context_t s_rs485DmaContext[kProductRs485ChannelCount] = {
    { .channel = kProductRs485Channel0, .txState = kProductRs485DmaTxIdle,
      .rxTimeoutUs = PRODUCT_RS485_DMA_DEFAULT_FRAME_TIMEOUT_US },
    { .channel = kProductRs485Channel1, .txState = kProductRs485DmaTxIdle,
      .rxTimeoutUs = PRODUCT_RS485_DMA_DEFAULT_FRAME_TIMEOUT_US },
};

static volatile uint32_t s_rs485MrtEvents;
static uint32_t s_rs485MrtClockHz;
static bool s_rs485MrtInitialized;

static bool ProductRs485Dma_IsChannelIndexValid(product_rs485_channel_t channel)
{
    return ((uint32_t)channel < (uint32_t)kProductRs485ChannelCount);
}

static bool ProductRs485Dma_IsBaudRateSupported(uint32_t baudRate)
{
    return (baudRate == 4800UL) ||
           (baudRate == 9600UL) ||
           (baudRate == 19200UL) ||
           (baudRate == 38400UL) ||
           (baudRate == 57600UL) ||
           (baudRate == 115200UL) ||
           (baudRate == 230400UL);
}

static mrt_chnl_t ProductRs485Dma_GetReceiveTimerChannel(product_rs485_channel_t channel)
{
    return (channel == kProductRs485Channel0) ? kMRT_Channel_0 : kMRT_Channel_1;
}

static status_t ProductRs485Dma_StartMrtTimer(mrt_chnl_t channel, uint32_t timeoutUs)
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

static void ProductRs485Dma_StopMrtTimer(mrt_chnl_t channel)
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

static bool ProductRs485Dma_TakeMrtEvent(mrt_chnl_t channel)
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

static status_t ProductRs485Dma_InitMrt(void)
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
    NVIC_SetPriority(MRT0_IRQn, PRODUCT_RS485_MRT_IRQ_PRIORITY);
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

static void ProductRs485Dma_Callback(USART_Type *base,
                                  usart_dma_handle_t *handle,
                                  status_t status,
                                  void *userData)
{
    product_rs485_dma_context_t *context = (product_rs485_dma_context_t *)userData;

    (void)base;
    (void)handle;

    if (context != NULL)
    {
        if (status == kStatus_USART_TxIdle)
        {
            context->txState = kProductRs485DmaTxDirectionReleasePending;
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

static status_t ProductRs485Dma_InitChannel(product_rs485_channel_t channel)
{
    const product_rs485_dma_hardware_t *hardware = &s_rs485DmaHardware[(uint32_t)channel];
    product_rs485_dma_context_t *context = &s_rs485DmaContext[(uint32_t)channel];
    status_t status;

    context->txState = kProductRs485DmaTxIdle;
    context->rxState = kProductRs485DmaRxIdle;
    context->rxGapState = kProductRs485DmaRxGapIdle;
    context->rxDmaFullPending = false;
    context->rxTimeoutUs = PRODUCT_RS485_DMA_DEFAULT_FRAME_TIMEOUT_US;
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
                                           ProductRs485Dma_Callback,
                                           context,
                                           hardware->txDmaHandle,
                                           hardware->rxDmaHandle);
    if (status == kStatus_Success)
    {
        context->initialized = true;
    }

    return status;
}

static void ProductRs485Dma_FinalizeReceive(product_rs485_dma_context_t *context, size_t length)
{
    ProductRs485Dma_StopMrtTimer(ProductRs485Dma_GetReceiveTimerChannel(context->channel));
    context->rxFrameLength = length;
    context->rxFrameTimingError = context->rxTimingError;
    context->rxState = (length != 0U) ? kProductRs485DmaRxFrameReady : kProductRs485DmaRxIdle;
    context->rxGapState = kProductRs485DmaRxGapIdle;
    context->rxDmaFullPending = false;
    context->rxHasData = false;
}

static void ProductRs485Dma_StopReceiveAndFinalize(
    product_rs485_dma_context_t *context,
    uint32_t receivedCount)
{
    const product_rs485_dma_hardware_t *hardware =
        &s_rs485DmaHardware[(uint32_t)context->channel];

    USART_EnableRxDMA(hardware->usart, false);
    (void)USART_TransferGetReceiveCountDMA(hardware->usart, hardware->usartDmaHandle, &receivedCount);
    USART_TransferAbortReceiveDMA(hardware->usart, hardware->usartDmaHandle);
    ProductRs485Dma_FinalizeReceive(context, (size_t)receivedCount);
}

static void ProductRs485Dma_RestartReceiveGapTimer(product_rs485_dma_context_t *context)
{
    uint32_t timeoutUs;

    if (context->rxUsesRtuTiming)
    {
        context->rxGapState = kProductRs485DmaRxGapWaitT15;
        timeoutUs = context->rxT15Us;
    }
    else
    {
        context->rxGapState = kProductRs485DmaRxGapIdle;
        timeoutUs = context->rxTimeoutUs;
    }

    if (ProductRs485Dma_StartMrtTimer(
            ProductRs485Dma_GetReceiveTimerChannel(context->channel),
            timeoutUs) != kStatus_Success)
    {
        context->rxTimingError = true;
    }
}

static void ProductRs485Dma_ProcessReceive(product_rs485_dma_context_t *context)
{
    const product_rs485_dma_hardware_t *hardware;
    mrt_chnl_t timerChannel;
    uint32_t receivedCount;
    status_t countStatus;
    bool timerElapsed;

    if (context->rxState != kProductRs485DmaRxActive)
    {
        return;
    }

    hardware = &s_rs485DmaHardware[(uint32_t)context->channel];
    timerChannel = ProductRs485Dma_GetReceiveTimerChannel(context->channel);

    if (context->rxDmaFullPending)
    {
        ProductRs485Dma_FinalizeReceive(context, context->rxCapacity);
        return;
    }

    timerElapsed = ProductRs485Dma_TakeMrtEvent(timerChannel);
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
                ProductRs485Dma_StopReceiveAndFinalize(context, receivedCount);
                return;
            }
        }
        else if (context->rxGapState == kProductRs485DmaRxGapWaitT15)
        {
            if (receivedCount == context->rxLastCount)
            {
                context->rxT15Count = receivedCount;
                context->rxGapState = kProductRs485DmaRxGapWaitT35;
                if (ProductRs485Dma_StartMrtTimer(
                        timerChannel,
                        context->rxT35Us - context->rxT15Us) != kStatus_Success)
                {
                    context->rxTimingError = true;
                }
                return;
            }
        }
        else if (context->rxGapState == kProductRs485DmaRxGapWaitT35)
        {
            if (receivedCount == context->rxT15Count)
            {
                ProductRs485Dma_StopReceiveAndFinalize(context, receivedCount);
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
            (context->rxGapState == kProductRs485DmaRxGapWaitT35))
        {
            context->rxTimingError = true;
        }

        context->rxLastCount = receivedCount;
        context->rxHasData = (receivedCount != 0U);
        ProductRs485Dma_RestartReceiveGapTimer(context);
    }
}

status_t ProductRs485Dma_Init(void)
{
    status_t status = ProductRs485Dma_InitMrt();

    if (status != kStatus_Success)
    {
        return status;
    }

    status = ProductRs485Dma_InitChannel(kProductRs485Channel0);
    if (status != kStatus_Success)
    {
        return status;
    }

    return ProductRs485Dma_InitChannel(kProductRs485Channel1);
}

status_t ProductRs485Dma_Configure(
    product_rs485_channel_t channel,
    uint32_t baudRate,
    uint8_t dataBits,
    uint8_t parity,
    uint8_t stopBits)
{
    product_rs485_dma_context_t *context;
    const product_rs485_dma_hardware_t *hardware;
    usart_config_t configuration;
    status_t status;

    if (!ProductRs485Dma_IsChannelIndexValid(channel) ||
        !ProductRs485Dma_IsBaudRateSupported(baudRate) ||
        ((dataBits != 7U) && (dataBits != 8U)) ||
        (parity > 2U) || ((stopBits != 1U) && (stopBits != 2U)))
    {
        return kStatus_InvalidArgument;
    }

    context = &s_rs485DmaContext[(uint32_t)channel];
    hardware = &s_rs485DmaHardware[(uint32_t)channel];
    if (!context->initialized)
    {
        return kStatus_Fail;
    }
    if (context->txState != kProductRs485DmaTxIdle)
    {
        return kStatus_Busy;
    }

    (void)ProductRs485Dma_CancelReceive(channel);
    USART_GetDefaultConfig(&configuration);
    configuration.baudRate_Bps = baudRate;
    configuration.enableTx = true;
    configuration.enableRx = true;
    configuration.bitCountPerChar =
        (dataBits == 7U) ? kUSART_7BitsPerChar : kUSART_8BitsPerChar;
    configuration.parityMode = (parity == 0U) ? kUSART_ParityDisabled :
        ((parity == 1U) ? kUSART_ParityEven : kUSART_ParityOdd);
    configuration.stopBitCount =
        (stopBits == 2U) ? kUSART_TwoStopBit : kUSART_OneStopBit;

    USART_Deinit(hardware->usart);
    status = USART_Init(
        hardware->usart,
        &configuration,
        (channel == kProductRs485Channel0) ?
            UART0_FC0_CLOCK_SOURCE : UART1_FC1_CLOCK_SOURCE);
    if (status != kStatus_Success)
    {
        return status;
    }

    status = USART_TransferCreateHandleDMA(
        hardware->usart,
        hardware->usartDmaHandle,
        ProductRs485Dma_Callback,
        context,
        hardware->txDmaHandle,
        hardware->rxDmaHandle);
    if (status == kStatus_Success)
    {
        (void)ProductRs485Direction_ForceReceive(channel);
    }
    return status;
}

status_t ProductRs485Dma_SendAsync(product_rs485_channel_t channel, const uint8_t *data, size_t length)
{
    product_rs485_dma_context_t *context;
    const product_rs485_dma_hardware_t *hardware;
    usart_transfer_t transfer;
    status_t status;

    if ((data == NULL) || (length == 0U) || !ProductRs485Dma_IsChannelIndexValid(channel))
    {
        return kStatus_InvalidArgument;
    }

    context = &s_rs485DmaContext[(uint32_t)channel];
    hardware = &s_rs485DmaHardware[(uint32_t)channel];
    if (!context->initialized)
    {
        return kStatus_InvalidArgument;
    }
    if ((context->txState != kProductRs485DmaTxIdle) ||
        (context->rxState != kProductRs485DmaRxIdle))
    {
        return kStatus_Busy;
    }

    status = ProductRs485Direction_BeginTransmit(channel);
    if (status != kStatus_Success)
    {
        return status;
    }

    transfer.txData = (uint8_t *)data;
    transfer.dataSize = length;
    context->txState = kProductRs485DmaTxActive;
    status = USART_TransferSendDMA(hardware->usart, hardware->usartDmaHandle, &transfer);
    if (status != kStatus_Success)
    {
        context->txState = kProductRs485DmaTxIdle;
        (void)ProductRs485Direction_ForceReceive(channel);
    }

    return status;
}

status_t ProductRs485Dma_AbortTransmit(product_rs485_channel_t channel)
{
    product_rs485_dma_context_t *context;
    const product_rs485_dma_hardware_t *hardware;

    if (!ProductRs485Dma_IsChannelIndexValid(channel))
    {
        return kStatus_InvalidArgument;
    }
    context = &s_rs485DmaContext[(uint32_t)channel];
    hardware = &s_rs485DmaHardware[(uint32_t)channel];
    if (!context->initialized)
    {
        return kStatus_Fail;
    }

    USART_TransferAbortSendDMA(hardware->usart, hardware->usartDmaHandle);
    context->txState = kProductRs485DmaTxIdle;
    return ProductRs485Direction_ForceReceive(channel);
}

status_t ProductRs485Dma_IsTransmitBusy(product_rs485_channel_t channel, bool *isBusy)
{
    product_rs485_dma_context_t *context;

    if ((isBusy == NULL) || !ProductRs485Dma_IsChannelIndexValid(channel))
    {
        return kStatus_InvalidArgument;
    }

    context = &s_rs485DmaContext[(uint32_t)channel];
    if (!context->initialized)
    {
        return kStatus_InvalidArgument;
    }

    *isBusy = (context->txState != kProductRs485DmaTxIdle);
    return kStatus_Success;
}

status_t ProductRs485Dma_SetReceiveTimeoutUs(product_rs485_channel_t channel, uint32_t timeoutUs)
{
    product_rs485_dma_context_t *context;
    uint64_t ticks;

    if ((timeoutUs == 0U) || !ProductRs485Dma_IsChannelIndexValid(channel))
    {
        return kStatus_InvalidArgument;
    }

    context = &s_rs485DmaContext[(uint32_t)channel];
    if (!context->initialized)
    {
        return kStatus_InvalidArgument;
    }
    if (context->rxState == kProductRs485DmaRxActive)
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

status_t ProductRs485Dma_SetRtuReceiveTimingUs(
    product_rs485_channel_t channel,
    uint32_t t15Us,
    uint32_t t35Us)
{
    product_rs485_dma_context_t *context;
    uint64_t maxTicks;

    if (!ProductRs485Dma_IsChannelIndexValid(channel) ||
        (t15Us == 0U) || (t35Us <= t15Us))
    {
        return kStatus_InvalidArgument;
    }

    context = &s_rs485DmaContext[(uint32_t)channel];
    if (!context->initialized)
    {
        return kStatus_InvalidArgument;
    }
    if (context->rxState == kProductRs485DmaRxActive)
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

status_t ProductRs485Dma_StartReceive(product_rs485_channel_t channel, uint8_t *buffer, size_t capacity)
{
    product_rs485_dma_context_t *context;
    const product_rs485_dma_hardware_t *hardware;
    usart_transfer_t transfer;
    status_t status;

    if ((buffer == NULL) || (capacity == 0U) || !ProductRs485Dma_IsChannelIndexValid(channel))
    {
        return kStatus_InvalidArgument;
    }

    context = &s_rs485DmaContext[(uint32_t)channel];
    hardware = &s_rs485DmaHardware[(uint32_t)channel];
    if (!context->initialized)
    {
        return kStatus_InvalidArgument;
    }
    if ((context->txState != kProductRs485DmaTxIdle) ||
        (context->rxState != kProductRs485DmaRxIdle))
    {
        return kStatus_Busy;
    }

    transfer.data = buffer;
    transfer.dataSize = capacity;

    ProductRs485Dma_StopMrtTimer(ProductRs485Dma_GetReceiveTimerChannel(channel));
    context->rxState = kProductRs485DmaRxActive;
    context->rxGapState = kProductRs485DmaRxGapIdle;
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
        context->rxState = kProductRs485DmaRxIdle;
        context->rxCapacity = 0U;
    }

    return status;
}

status_t ProductRs485Dma_GetReceiveCount(product_rs485_channel_t channel, size_t *count)
{
    product_rs485_dma_context_t *context;
    const product_rs485_dma_hardware_t *hardware;
    uint32_t receivedCount;
    status_t status;

    if ((count == NULL) || !ProductRs485Dma_IsChannelIndexValid(channel))
    {
        return kStatus_InvalidArgument;
    }

    context = &s_rs485DmaContext[(uint32_t)channel];
    hardware = &s_rs485DmaHardware[(uint32_t)channel];
    if (!context->initialized)
    {
        return kStatus_InvalidArgument;
    }
    if (context->rxState != kProductRs485DmaRxActive)
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

status_t ProductRs485Dma_CompleteReceive(product_rs485_channel_t channel, size_t frameLength)
{
    product_rs485_dma_context_t *context;
    const product_rs485_dma_hardware_t *hardware;
    size_t receivedCount;
    status_t status;

    if ((frameLength == 0U) || !ProductRs485Dma_IsChannelIndexValid(channel))
    {
        return kStatus_InvalidArgument;
    }

    context = &s_rs485DmaContext[(uint32_t)channel];
    hardware = &s_rs485DmaHardware[(uint32_t)channel];
    if (!context->initialized)
    {
        return kStatus_InvalidArgument;
    }
    if (context->rxState != kProductRs485DmaRxActive)
    {
        return kStatus_NoTransferInProgress;
    }

    status = ProductRs485Dma_GetReceiveCount(channel, &receivedCount);
    if (status != kStatus_Success)
    {
        if ((status == kStatus_NoTransferInProgress) &&
            context->rxDmaFullPending &&
            (frameLength <= context->rxCapacity))
        {
            ProductRs485Dma_FinalizeReceive(context, frameLength);
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
    ProductRs485Dma_FinalizeReceive(context, frameLength);
    return kStatus_Success;
}

status_t ProductRs485Dma_TakeReceivedFrame(product_rs485_channel_t channel, size_t *length)
{
    product_rs485_dma_context_t *context;

    if ((length == NULL) || !ProductRs485Dma_IsChannelIndexValid(channel))
    {
        return kStatus_InvalidArgument;
    }

    context = &s_rs485DmaContext[(uint32_t)channel];
    if (!context->initialized)
    {
        return kStatus_InvalidArgument;
    }
    if (context->rxState != kProductRs485DmaRxFrameReady)
    {
        return kStatus_NoData;
    }

    *length = context->rxFrameLength;
    context->rxFrameLength = 0U;
    context->rxCapacity = 0U;
    context->rxState = kProductRs485DmaRxIdle;
    return kStatus_Success;
}

status_t ProductRs485Dma_GetLastReceiveTimingError(
    product_rs485_channel_t channel,
    bool *timingError)
{
    product_rs485_dma_context_t *context;

    if ((timingError == NULL) || !ProductRs485Dma_IsChannelIndexValid(channel))
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

status_t ProductRs485Dma_CancelReceive(product_rs485_channel_t channel)
{
    product_rs485_dma_context_t *context;
    const product_rs485_dma_hardware_t *hardware;

    if (!ProductRs485Dma_IsChannelIndexValid(channel))
    {
        return kStatus_InvalidArgument;
    }

    context = &s_rs485DmaContext[(uint32_t)channel];
    hardware = &s_rs485DmaHardware[(uint32_t)channel];
    if (!context->initialized)
    {
        return kStatus_InvalidArgument;
    }

    ProductRs485Dma_StopMrtTimer(ProductRs485Dma_GetReceiveTimerChannel(channel));
    if (context->rxState == kProductRs485DmaRxActive)
    {
        USART_EnableRxDMA(hardware->usart, false);
        USART_TransferAbortReceiveDMA(hardware->usart, hardware->usartDmaHandle);
    }

    context->rxState = kProductRs485DmaRxIdle;
    context->rxGapState = kProductRs485DmaRxGapIdle;
    context->rxDmaFullPending = false;
    context->rxLastCount = 0U;
    context->rxFrameLength = 0U;
    context->rxCapacity = 0U;
    context->rxHasData = false;
    context->rxTimingError = false;
    context->rxFrameTimingError = false;
    return kStatus_Success;
}

status_t ProductRs485Dma_StartTurnaroundDelayUs(uint32_t delayUs)
{
    return ProductRs485Dma_StartMrtTimer(PRODUCT_RS485_MRT_TURNAROUND_CHANNEL, delayUs);
}

status_t ProductRs485Dma_TakeTurnaroundDelayElapsed(bool *elapsed)
{
    if ((elapsed == NULL) || !s_rs485MrtInitialized)
    {
        return kStatus_InvalidArgument;
    }

    *elapsed = ProductRs485Dma_TakeMrtEvent(PRODUCT_RS485_MRT_TURNAROUND_CHANNEL);
    return kStatus_Success;
}

void ProductRs485Dma_Process(void)
{
    uint32_t index;

    for (index = 0U; index < (uint32_t)kProductRs485ChannelCount; index++)
    {
        product_rs485_dma_context_t *context = &s_rs485DmaContext[index];

        if (!context->initialized)
        {
            continue;
        }

        if (context->txState == kProductRs485DmaTxDirectionReleasePending)
        {
            if (ProductRs485Direction_EndTransmitBlocking(context->channel, 1U) == kStatus_Success)
            {
                context->txState = kProductRs485DmaTxIdle;
                HalSerial_NotifyTransmitCompleteFromIsr(
                    (context->channel == kProductRs485Channel0) ?
                        HAL_SERIAL_PORT_0 : HAL_SERIAL_PORT_1);
            }
        }

        ProductRs485Dma_ProcessReceive(context);
    }
}
