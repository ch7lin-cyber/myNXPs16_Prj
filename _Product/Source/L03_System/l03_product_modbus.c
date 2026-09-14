/*
 * Copyright 2026
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "l03_product_modbus.h"

#include <stddef.h>
#include <string.h>

#include "ModbusAsciiFramer.h"
#include "ModbusRtuFramer.h"
#include "ModbusSlave.h"
#include "ProductConfig.h"
#include "SerialConfiguration.h"
#include "l02_rs485_dma.h"
#include "peripherals.h"
#include "product_modbus_register_adapter.h"

typedef enum _l03_product_modbus_state
{
    kL03_ProductModbusStopped = 0U,
    kL03_ProductModbusReceiving,
    kL03_ProductModbusWaitingForTurnaround,
    kL03_ProductModbusWaitingForTx
} l03_product_modbus_state_t;

typedef struct _l03_product_modbus_context
{
    ModbusSlaveConfig_t slave;
    SerialProtocol_t protocol;
    l03_product_modbus_state_t state;
    l03_product_modbus_statistics_t statistics;
    uint8_t rxBuffer[MODBUS_ASCII_MAX_ADU_LENGTH];
    uint8_t txBuffer[MODBUS_ASCII_MAX_ADU_LENGTH];
    size_t rxScannedLength;
    size_t txLength;
    bool initialized;
} l03_product_modbus_context_t;

static l03_product_modbus_context_t s_productModbus;

static size_t L03_ProductModbus_GetRxCapacity(void)
{
    return (s_productModbus.protocol == SERIAL_PROTOCOL_MODBUS_ASCII) ?
           (size_t)MODBUS_ASCII_MAX_ADU_LENGTH :
           (size_t)MODBUS_RTU_MAX_ADU_LENGTH;
}

static status_t L03_ProductModbus_StartReceive(void)
{
    status_t status = L02_Rs485Dma_StartReceive(
        kL02_Rs485Channel1,
        s_productModbus.rxBuffer,
        L03_ProductModbus_GetRxCapacity());

    if (status == kStatus_Success)
    {
        s_productModbus.rxScannedLength = 0U;
        s_productModbus.state = kL03_ProductModbusReceiving;
    }
    else
    {
        s_productModbus.state = kL03_ProductModbusStopped;
        s_productModbus.statistics.transportErrors++;
    }

    return status;
}

static void L03_ProductModbus_GetAutomaticRtuTiming(
    uint32_t *t15Us,
    uint32_t *t35Us)
{
    uint32_t bitsPerCharacter;
    uint32_t baudRate = UART1_FC1_config.baudRate_Bps;

    bitsPerCharacter = 1U;
    bitsPerCharacter +=
        (UART1_FC1_config.bitCountPerChar == kUSART_7BitsPerChar) ? 7U : 8U;
    bitsPerCharacter +=
        (UART1_FC1_config.parityMode == kUSART_ParityDisabled) ? 0U : 1U;
    bitsPerCharacter +=
        (UART1_FC1_config.stopBitCount == kUSART_TwoStopBit) ? 2U : 1U;

    if (baudRate > 19200U)
    {
        *t15Us = 750U;
        *t35Us = 1750U;
    }
    else
    {
        *t15Us = (uint32_t)(
            (((uint64_t)bitsPerCharacter * 1500000ULL) +
             (uint64_t)baudRate - 1ULL) /
            (uint64_t)baudRate);
        *t35Us = (uint32_t)(
            (((uint64_t)bitsPerCharacter * 3500000ULL) +
             (uint64_t)baudRate - 1ULL) /
            (uint64_t)baudRate);
    }
}

static status_t L03_ProductModbus_ConfigureReceiveTiming(void)
{
    uint32_t t15Us;
    uint32_t t35Us;

    if (s_productModbus.protocol == SERIAL_PROTOCOL_MODBUS_ASCII)
    {
        return L02_Rs485Dma_SetReceiveTimeoutUs(
            kL02_Rs485Channel1,
            PRODUCT_MODBUS_FC1_FRAME_TIMEOUT_US);
    }

#if ((PRODUCT_MODBUS_FC1_RTU_T15_US != 0U) && \
     (PRODUCT_MODBUS_FC1_RTU_T35_US != 0U))
    t15Us = PRODUCT_MODBUS_FC1_RTU_T15_US;
    t35Us = PRODUCT_MODBUS_FC1_RTU_T35_US;
#else
    L03_ProductModbus_GetAutomaticRtuTiming(&t15Us, &t35Us);
#endif

    return L02_Rs485Dma_SetRtuReceiveTimingUs(
        kL02_Rs485Channel1,
        t15Us,
        t35Us);
}

static status_t L03_ProductModbus_DetectAsciiEnd(void)
{
    size_t receivedCount;
    size_t index;
    status_t status;

    if (s_productModbus.protocol != SERIAL_PROTOCOL_MODBUS_ASCII)
    {
        return kStatus_Success;
    }

    status = L02_Rs485Dma_GetReceiveCount(kL02_Rs485Channel1, &receivedCount);
    if (status == kStatus_NoTransferInProgress)
    {
        return kStatus_Success;
    }
    if (status != kStatus_Success)
    {
        return status;
    }

    for (index = s_productModbus.rxScannedLength; index < receivedCount; index++)
    {
        if ((index != 0U) &&
            (s_productModbus.rxBuffer[index - 1U] == MODBUS_ASCII_CR_CHARACTER) &&
            (s_productModbus.rxBuffer[index] == MODBUS_ASCII_LF_CHARACTER))
        {
            s_productModbus.rxScannedLength = index + 1U;
            return L02_Rs485Dma_CompleteReceive(kL02_Rs485Channel1, index + 1U);
        }
    }

    s_productModbus.rxScannedLength = receivedCount;
    return kStatus_Success;
}

static ModbusSlaveResult_t L03_ProductModbus_ProcessFrame(size_t rxLength, size_t *txLength)
{
    if (s_productModbus.protocol == SERIAL_PROTOCOL_MODBUS_ASCII)
    {
        return ModbusSlave_ProcessAsciiRequest(
            &s_productModbus.slave,
            s_productModbus.rxBuffer,
            rxLength,
            s_productModbus.txBuffer,
            sizeof(s_productModbus.txBuffer),
            txLength);
    }

    return ModbusSlave_ProcessRtuRequest(
        &s_productModbus.slave,
        s_productModbus.rxBuffer,
        rxLength,
        s_productModbus.txBuffer,
        sizeof(s_productModbus.txBuffer),
        txLength);
}

static void L03_ProductModbus_RecordResult(ModbusSlaveResult_t result)
{
    switch (result)
    {
        case MODBUS_SLAVE_RESULT_CRC_ERROR:
            s_productModbus.statistics.crcErrors++;
            break;
        case MODBUS_SLAVE_RESULT_LRC_ERROR:
            s_productModbus.statistics.lrcErrors++;
            break;
        case MODBUS_SLAVE_RESULT_IGNORED:
            s_productModbus.statistics.ignoredFrames++;
            break;
        case MODBUS_SLAVE_RESULT_INVALID_ARGUMENT:
        case MODBUS_SLAVE_RESULT_RESPONSE_TOO_SMALL:
            s_productModbus.statistics.transportErrors++;
            break;
        case MODBUS_SLAVE_RESULT_RESPONSE_READY:
        case MODBUS_SLAVE_RESULT_PROCESSED_NO_RESPONSE:
        default:
            break;
    }
}

static status_t L03_ProductModbus_SendPreparedResponse(void)
{
    status_t status = L02_Rs485Dma_SendAsync(
        kL02_Rs485Channel1,
        s_productModbus.txBuffer,
        s_productModbus.txLength);

    if (status == kStatus_Success)
    {
        s_productModbus.state = kL03_ProductModbusWaitingForTx;
        s_productModbus.statistics.transmittedResponses++;
    }

    return status;
}

static status_t L03_ProductModbus_StartResponse(void)
{
#if (PRODUCT_MODBUS_FC1_TURNAROUND_DELAY_US == 0U)
    return L03_ProductModbus_SendPreparedResponse();
#else
    status_t status = L02_Rs485Dma_StartTurnaroundDelayUs(
        PRODUCT_MODBUS_FC1_TURNAROUND_DELAY_US);

    if (status == kStatus_Success)
    {
        s_productModbus.state = kL03_ProductModbusWaitingForTurnaround;
    }

    return status;
#endif
}

status_t L03_ProductModbus_Init(void)
{
#if (PRODUCT_MODBUS_FC1_ENABLE == 0U)
    return kStatus_Success;
#else
    status_t status;

    (void)memset(&s_productModbus, 0, sizeof(s_productModbus));
    s_productModbus.slave.unit_address = (uint8_t)PRODUCT_MODBUS_FC1_UNIT_ADDRESS;
    ProductModbusRegisterAdapter_GetInterface(&s_productModbus.slave.registers);

#if (PRODUCT_MODBUS_FC1_PROTOCOL == PRODUCT_MODBUS_PROTOCOL_ASCII)
    s_productModbus.protocol = SERIAL_PROTOCOL_MODBUS_ASCII;
#else
    s_productModbus.protocol = SERIAL_PROTOCOL_MODBUS_RTU;
#endif

    status = L03_ProductModbus_ConfigureReceiveTiming();
    if (status != kStatus_Success)
    {
        return status;
    }

    s_productModbus.initialized = true;
    status = L03_ProductModbus_StartReceive();
    if (status != kStatus_Success)
    {
        s_productModbus.initialized = false;
    }

    return status;
#endif
}

void L03_ProductModbus_Process(void)
{
#if (PRODUCT_MODBUS_FC1_ENABLE != 0U)
    size_t rxLength;
    size_t txLength = 0U;
    bool txBusy;
    bool elapsed;
    bool timingError;
    status_t status;
    ModbusSlaveResult_t result;

    if (!s_productModbus.initialized)
    {
        return;
    }

    if (s_productModbus.state == kL03_ProductModbusReceiving)
    {
        status = L03_ProductModbus_DetectAsciiEnd();
        if (status != kStatus_Success)
        {
            s_productModbus.statistics.transportErrors++;
            (void)L02_Rs485Dma_CancelReceive(kL02_Rs485Channel1);
            s_productModbus.state = kL03_ProductModbusStopped;
            return;
        }

        status = L02_Rs485Dma_TakeReceivedFrame(kL02_Rs485Channel1, &rxLength);
        if (status == kStatus_NoData)
        {
            return;
        }
        if (status != kStatus_Success)
        {
            s_productModbus.statistics.transportErrors++;
            s_productModbus.state = kL03_ProductModbusStopped;
            return;
        }

        status = L02_Rs485Dma_GetLastReceiveTimingError(
            kL02_Rs485Channel1, &timingError);
        if (status != kStatus_Success)
        {
            s_productModbus.statistics.transportErrors++;
            s_productModbus.state = kL03_ProductModbusStopped;
            return;
        }
        if ((s_productModbus.protocol == SERIAL_PROTOCOL_MODBUS_RTU) && timingError)
        {
            s_productModbus.statistics.rtuInterCharacterErrors++;
            (void)L03_ProductModbus_StartReceive();
            return;
        }

        s_productModbus.statistics.receivedFrames++;
        result = L03_ProductModbus_ProcessFrame(rxLength, &txLength);
        L03_ProductModbus_RecordResult(result);

        if (result == MODBUS_SLAVE_RESULT_RESPONSE_READY)
        {
            s_productModbus.txLength = txLength;
            status = L03_ProductModbus_StartResponse();
            if (status == kStatus_Success)
            {
                return;
            }
            s_productModbus.statistics.transportErrors++;
        }

        (void)L03_ProductModbus_StartReceive();
    }
    else if (s_productModbus.state == kL03_ProductModbusWaitingForTurnaround)
    {
        status = L02_Rs485Dma_TakeTurnaroundDelayElapsed(&elapsed);
        if (status != kStatus_Success)
        {
            s_productModbus.statistics.transportErrors++;
            s_productModbus.state = kL03_ProductModbusStopped;
        }
        else if (elapsed)
        {
            if (L03_ProductModbus_SendPreparedResponse() != kStatus_Success)
            {
                s_productModbus.statistics.transportErrors++;
                s_productModbus.state = kL03_ProductModbusStopped;
            }
        }
        else
        {
            /* MRT0 channel 2 is still counting. */
        }
    }
    else if (s_productModbus.state == kL03_ProductModbusWaitingForTx)
    {
        status = L02_Rs485Dma_IsTransmitBusy(kL02_Rs485Channel1, &txBusy);
        if (status != kStatus_Success)
        {
            s_productModbus.statistics.transportErrors++;
            s_productModbus.state = kL03_ProductModbusStopped;
        }
        else if (!txBusy)
        {
            (void)L03_ProductModbus_StartReceive();
        }
        else
        {
            /* DMA or the final direction release is still active. */
        }
    }
    else
    {
        (void)L03_ProductModbus_StartReceive();
    }
#endif
}

status_t L03_ProductModbus_GetStatistics(
    l03_product_modbus_statistics_t *statistics)
{
    if (statistics == NULL)
    {
        return kStatus_InvalidArgument;
    }

#if (PRODUCT_MODBUS_FC1_ENABLE == 0U)
    (void)memset(statistics, 0, sizeof(*statistics));
    return kStatus_Success;
#else
    if (!s_productModbus.initialized)
    {
        return kStatus_Fail;
    }

    *statistics = s_productModbus.statistics;
    return kStatus_Success;
#endif
}

bool L03_ProductModbus_IsInitialized(void)
{
#if (PRODUCT_MODBUS_FC1_ENABLE == 0U)
    return false;
#else
    return s_productModbus.initialized;
#endif
}
