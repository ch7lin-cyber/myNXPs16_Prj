#include "l03_product_modbus.h"

#include <stddef.h>
#include <string.h>

#include "HalSerial.h"
#include "ModbusAsciiFramer.h"
#include "ModbusRegisterAdapter.h"
#include "ModbusRtuFramer.h"
#include "ModbusSlave.h"
#include "ProductConfig.h"
#include "SerialConfiguration.h"
#include "SerialConfigurationApplyService.h"
#include "bsp_rs485.h"
#include "product_modbus_register_adapter.h"

typedef enum
{
    PRODUCT_MODBUS_STOPPED = 0,
    PRODUCT_MODBUS_RECEIVING,
    PRODUCT_MODBUS_WAITING_RESPONSE_DELAY,
    PRODUCT_MODBUS_WAITING_TX
} ProductModbusState_t;

typedef struct
{
    ModbusSlaveConfig_t slave;
    SerialProtocol_t protocol;
    ProductModbusState_t state;
    l03_product_modbus_statistics_t statistics;
    uint8_t rxBuffer[MODBUS_ASCII_MAX_ADU_LENGTH];
    uint8_t txBuffer[MODBUS_ASCII_MAX_ADU_LENGTH];
    size_t rxScannedLength;
    size_t txLength;
    uint16_t configurationRevision;
    bool initialized;
} ProductModbusContext_t;

static ProductModbusContext_t s_modbus;

static void SetInitialSerialConfiguration(
    ModbusSerialPortConfiguration_t *configuration,
    SerialRole_t role,
    uint16_t unit_id)
{
    configuration->serial.line.baud_rate = PRODUCT_MODBUS_BAUD_RATE;
    configuration->serial.line.data_bits =
        (HalSerialDataBits_t)PRODUCT_MODBUS_DATA_BITS;
    configuration->serial.line.parity =
        (HalSerialParity_t)PRODUCT_MODBUS_PARITY;
    configuration->serial.line.stop_bits =
        (HalSerialStopBits_t)PRODUCT_MODBUS_STOP_BITS;
    configuration->serial.protocol =
        (PRODUCT_MODBUS_PROTOCOL == PRODUCT_MODBUS_PROTOCOL_ASCII) ?
            SERIAL_PROTOCOL_MODBUS_ASCII : SERIAL_PROTOCOL_MODBUS_RTU;
    configuration->serial.role = role;
    configuration->serial.response_timeout_ms =
        PRODUCT_MODBUS_RESPONSE_TIMEOUT_MS;
    configuration->unit_id = unit_id;
}

static void GetRtuTiming(
    const SerialConfiguration_t *configuration,
    uint32_t *t15_us,
    uint32_t *t35_us)
{
#if ((PRODUCT_MODBUS_RTU_T15_US != 0UL) && \
     (PRODUCT_MODBUS_RTU_T35_US != 0UL))
    (void)configuration;
    *t15_us = PRODUCT_MODBUS_RTU_T15_US;
    *t35_us = PRODUCT_MODBUS_RTU_T35_US;
#else
    uint32_t character_time_us =
        SerialConfiguration_GetCharacterTimeUs(configuration);
    if (configuration->line.baud_rate > 19200UL)
    {
        *t15_us = 750UL;
        *t35_us = 1750UL;
    }
    else
    {
        *t15_us = ((character_time_us * 15UL) + 9UL) / 10UL;
        *t35_us = ((character_time_us * 35UL) + 9UL) / 10UL;
    }
#endif
}

static bool ConfigureReceiveTiming(const SerialConfiguration_t *configuration)
{
    uint32_t t15_us;
    uint32_t t35_us;

    if (configuration->protocol == SERIAL_PROTOCOL_MODBUS_ASCII)
    {
        return (BspRs485_SetReceiveTimeoutUs(
                    BSP_RS485_PORT_EXTERNAL_SLAVE,
                    PRODUCT_MODBUS_FRAME_TIMEOUT_US) == BSP_RS485_STATUS_OK);
    }
    GetRtuTiming(configuration, &t15_us, &t35_us);
    return (BspRs485_SetRtuTimingUs(
                BSP_RS485_PORT_EXTERNAL_SLAVE,
                t15_us, t35_us) == BSP_RS485_STATUS_OK);
}

static bool ConfigureDirectionTiming(const SerialConfiguration_t *configuration)
{
    uint32_t bit_time_us =
        (1000000UL + configuration->line.baud_rate - 1UL) /
        configuration->line.baud_rate;
    uint32_t pre_tx_delay_us =
        bit_time_us * PRODUCT_RS485_DE_ASSERT_DELAY_BITS;
    uint32_t post_tx_delay_us =
        bit_time_us * PRODUCT_RS485_DE_RELEASE_DELAY_BITS;

    return (BspRs485_SetDirectionDelayUs(
                BSP_RS485_PORT_EXTERNAL_SLAVE,
                pre_tx_delay_us,
                post_tx_delay_us) == BSP_RS485_STATUS_OK) &&
           (BspRs485_SetDirectionDelayUs(
                BSP_RS485_PORT_MACHINE_MASTER,
                pre_tx_delay_us,
                post_tx_delay_us) == BSP_RS485_STATUS_OK);
}

static size_t GetRxCapacity(void)
{
    return (s_modbus.protocol == SERIAL_PROTOCOL_MODBUS_ASCII) ?
        (size_t)MODBUS_ASCII_MAX_ADU_LENGTH :
        (size_t)MODBUS_RTU_MAX_ADU_LENGTH;
}

static bool StartReceive(void)
{
    BspRs485Status_t status = BspRs485_StartReceive(
        BSP_RS485_PORT_EXTERNAL_SLAVE,
        s_modbus.rxBuffer,
        GetRxCapacity());
    if (status == BSP_RS485_STATUS_OK)
    {
        s_modbus.rxScannedLength = 0U;
        s_modbus.state = PRODUCT_MODBUS_RECEIVING;
        return true;
    }
    s_modbus.state = PRODUCT_MODBUS_STOPPED;
    s_modbus.statistics.transportErrors++;
    return false;
}

static bool SynchronizeActiveConfiguration(void)
{
    ModbusSerialPortConfiguration_t active;
    uint16_t revision;

    if (!ModbusRegisterAdapter_GetActiveSerialConfiguration(0U, &active) ||
        (ModbusRegisterAdapter_ReadSerialRegister(
             0x120AU, &revision) != MODBUS_EXCEPTION_NONE))
    {
        return false;
    }
    if (revision == s_modbus.configurationRevision)
    {
        return true;
    }

    s_modbus.protocol = active.serial.protocol;
    s_modbus.slave.unit_address = (uint8_t)active.unit_id;
    s_modbus.configurationRevision = revision;
    return ConfigureDirectionTiming(&active.serial) &&
           ConfigureReceiveTiming(&active.serial);
}

static bool DetectAsciiEnd(void)
{
    size_t received_count;
    size_t index;
    BspRs485Status_t status;

    if (s_modbus.protocol != SERIAL_PROTOCOL_MODBUS_ASCII)
    {
        return true;
    }
    status = BspRs485_GetReceiveCount(
        BSP_RS485_PORT_EXTERNAL_SLAVE, &received_count);
    if (status == BSP_RS485_STATUS_NO_TRANSFER)
    {
        return true;
    }
    if (status != BSP_RS485_STATUS_OK)
    {
        return false;
    }
    for (index = s_modbus.rxScannedLength; index < received_count; index++)
    {
        if ((index != 0U) &&
            (s_modbus.rxBuffer[index - 1U] == MODBUS_ASCII_CR_CHARACTER) &&
            (s_modbus.rxBuffer[index] == MODBUS_ASCII_LF_CHARACTER))
        {
            s_modbus.rxScannedLength = index + 1U;
            return (BspRs485_CompleteReceive(
                        BSP_RS485_PORT_EXTERNAL_SLAVE,
                        index + 1U) == BSP_RS485_STATUS_OK);
        }
    }
    s_modbus.rxScannedLength = received_count;
    return true;
}

static ModbusSlaveResult_t ProcessFrame(size_t rx_length, size_t *tx_length)
{
    if (s_modbus.protocol == SERIAL_PROTOCOL_MODBUS_ASCII)
    {
        return ModbusSlave_ProcessAsciiRequest(
            &s_modbus.slave,
            s_modbus.rxBuffer, rx_length,
            s_modbus.txBuffer, sizeof(s_modbus.txBuffer), tx_length);
    }
    return ModbusSlave_ProcessRtuRequest(
        &s_modbus.slave,
        s_modbus.rxBuffer, rx_length,
        s_modbus.txBuffer, sizeof(s_modbus.txBuffer), tx_length);
}

static void RecordResult(ModbusSlaveResult_t result)
{
    if (result == MODBUS_SLAVE_RESULT_CRC_ERROR)
    {
        s_modbus.statistics.crcErrors++;
    }
    else if (result == MODBUS_SLAVE_RESULT_LRC_ERROR)
    {
        s_modbus.statistics.lrcErrors++;
    }
    else if (result == MODBUS_SLAVE_RESULT_IGNORED)
    {
        s_modbus.statistics.ignoredFrames++;
    }
    else if ((result == MODBUS_SLAVE_RESULT_INVALID_ARGUMENT) ||
             (result == MODBUS_SLAVE_RESULT_RESPONSE_TOO_SMALL))
    {
        s_modbus.statistics.transportErrors++;
    }
}

static bool SendPreparedResponse(void)
{
    if (SerialConfigurationApplyService_WriteResponse(
            HAL_SERIAL_PORT_0,
            s_modbus.txBuffer,
            s_modbus.txLength) != SERIAL_SERVICE_STATUS_OK)
    {
        return false;
    }
    s_modbus.state = PRODUCT_MODBUS_WAITING_TX;
    s_modbus.statistics.transmittedResponses++;
    return true;
}

static bool StartResponse(void)
{
#if (PRODUCT_MODBUS_FC0_RESPONSE_DELAY_US == 0UL)
    return SendPreparedResponse();
#else
    if (BspRs485_StartSlaveResponseDelayUs(
            PRODUCT_MODBUS_FC0_RESPONSE_DELAY_US) != BSP_RS485_STATUS_OK)
    {
        return false;
    }
    s_modbus.state = PRODUCT_MODBUS_WAITING_RESPONSE_DELAY;
    return true;
#endif
}

bool L03_ProductModbus_Init(void)
{
    ModbusSerialPortConfiguration_t slave_configuration;
    ModbusSerialPortConfiguration_t master_configuration;

    (void)memset(&s_modbus, 0, sizeof(s_modbus));
    SetInitialSerialConfiguration(
        &slave_configuration,
        SERIAL_ROLE_MODBUS_SLAVE,
        PRODUCT_MODBUS_FC0_UNIT_ADDRESS);
    SetInitialSerialConfiguration(
        &master_configuration,
        SERIAL_ROLE_MODBUS_MASTER,
        PRODUCT_MODBUS_FC1_LOCAL_UNIT_ADDRESS);

    if ((SerialConfigurationApplyService_InitializePort(
             HAL_SERIAL_PORT_0, &slave_configuration,
             PRODUCT_MODBUS_APPLY_RESPONSE_TIMEOUT_MS,
             NULL, NULL) != SERIAL_SERVICE_STATUS_OK) ||
        (SerialConfigurationApplyService_InitializePort(
             HAL_SERIAL_PORT_1, &master_configuration,
             PRODUCT_MODBUS_APPLY_RESPONSE_TIMEOUT_MS,
             NULL, NULL) != SERIAL_SERVICE_STATUS_OK))
    {
        return false;
    }

    if (!ConfigureDirectionTiming(&slave_configuration.serial) ||
        !ConfigureReceiveTiming(&slave_configuration.serial))
    {
        return false;
    }

    s_modbus.protocol = slave_configuration.serial.protocol;
    s_modbus.slave.unit_address =
        (uint8_t)slave_configuration.unit_id;
    ProductModbusRegisterAdapter_GetInterface(&s_modbus.slave.registers);
    s_modbus.initialized = true;
    return StartReceive();
}

void L03_ProductModbus_Tick1ms(void)
{
    SerialConfigurationApplyService_Tick1ms();
}

void L03_ProductModbus_Process(void)
{
    size_t rx_length;
    size_t tx_length = 0U;
    bool elapsed;
    bool timing_error;
    BspRs485Status_t receive_status;
    ModbusSlaveResult_t result;

    SerialConfigurationApplyService_Process();
    if (!s_modbus.initialized)
    {
        return;
    }

    if (s_modbus.state == PRODUCT_MODBUS_RECEIVING)
    {
        if (!DetectAsciiEnd())
        {
            s_modbus.statistics.transportErrors++;
            (void)BspRs485_CancelReceive(BSP_RS485_PORT_EXTERNAL_SLAVE);
            s_modbus.state = PRODUCT_MODBUS_STOPPED;
            return;
        }
        receive_status = BspRs485_TakeReceivedFrame(
            BSP_RS485_PORT_EXTERNAL_SLAVE, &rx_length);
        if (receive_status == BSP_RS485_STATUS_NO_DATA)
        {
            return;
        }
        if ((receive_status != BSP_RS485_STATUS_OK) ||
            (BspRs485_GetLastReceiveTimingError(
                BSP_RS485_PORT_EXTERNAL_SLAVE,
                &timing_error) != BSP_RS485_STATUS_OK))
        {
            s_modbus.statistics.transportErrors++;
            s_modbus.state = PRODUCT_MODBUS_STOPPED;
            return;
        }
        if ((s_modbus.protocol == SERIAL_PROTOCOL_MODBUS_RTU) && timing_error)
        {
            s_modbus.statistics.rtuInterCharacterErrors++;
            (void)StartReceive();
            return;
        }

        s_modbus.statistics.receivedFrames++;
        result = ProcessFrame(rx_length, &tx_length);
        RecordResult(result);
        if (result == MODBUS_SLAVE_RESULT_RESPONSE_READY)
        {
            s_modbus.txLength = tx_length;
            if (StartResponse())
            {
                return;
            }
            s_modbus.statistics.transportErrors++;
        }
        (void)StartReceive();
    }
    else if (s_modbus.state == PRODUCT_MODBUS_WAITING_RESPONSE_DELAY)
    {
        if ((BspRs485_TakeSlaveResponseDelayElapsed(&elapsed) !=
             BSP_RS485_STATUS_OK) ||
            (elapsed && !SendPreparedResponse()))
        {
            s_modbus.statistics.transportErrors++;
            s_modbus.state = PRODUCT_MODBUS_STOPPED;
        }
    }
    else if (s_modbus.state == PRODUCT_MODBUS_WAITING_TX)
    {
        if (!HalSerial_IsWriteBusy(HAL_SERIAL_PORT_0))
        {
            if (!SynchronizeActiveConfiguration() || !StartReceive())
            {
                s_modbus.statistics.transportErrors++;
            }
        }
    }
    else
    {
        (void)StartReceive();
    }
}

bool L03_ProductModbus_GetStatistics(
    l03_product_modbus_statistics_t *statistics)
{
    if ((statistics == NULL) || !s_modbus.initialized)
    {
        return false;
    }
    *statistics = s_modbus.statistics;
    return true;
}

bool L03_ProductModbus_IsInitialized(void)
{
    return s_modbus.initialized;
}
