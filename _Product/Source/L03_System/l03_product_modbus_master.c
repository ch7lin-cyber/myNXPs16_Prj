#include "l03_product_modbus_master.h"

#include <string.h>

#include "HalSerial.h"
#include "ModbusAsciiFramer.h"
#include "ModbusRegisterAdapter.h"
#include "ModbusRtuFramer.h"
#include "ProductConfig.h"
#include "SerialConfiguration.h"
#include "SerialService.h"
#include "bsp_rs485.h"

typedef enum
{
    MASTER_TRANSPORT_IDLE = 0,
    MASTER_TRANSPORT_WAIT_TX,
    MASTER_TRANSPORT_WAIT_RESPONSE
} MasterTransportState_t;

typedef struct
{
    ModbusMasterRequest_t request;
    uint16_t write_values[MODBUS_MASTER_WRITE_QUANTITY_MAX];
    ProductModbusMasterResult_t result;
    SerialProtocol_t protocol;
    MasterTransportState_t transport_state;
    uint8_t tx_buffer[MODBUS_ASCII_MAX_ADU_LENGTH];
    uint8_t rx_buffer[MODBUS_ASCII_MAX_ADU_LENGTH];
    size_t tx_length;
    size_t rx_scanned_length;
    uint32_t elapsed_ms;
    uint32_t response_timeout_ms;
    bool initialized;
} ProductMasterContext_t;

static ProductMasterContext_t s_master;

static bool ConfigureReceiveTiming(const SerialConfiguration_t *configuration)
{
    uint32_t t15_us;
    uint32_t t35_us;

    if (configuration->protocol == SERIAL_PROTOCOL_MODBUS_ASCII)
    {
        return (BspRs485_SetReceiveTimeoutUs(
                    BSP_RS485_PORT_MACHINE_MASTER,
                    PRODUCT_MODBUS_FRAME_TIMEOUT_US) == BSP_RS485_STATUS_OK);
    }
#if ((PRODUCT_MODBUS_RTU_T15_US != 0UL) && \
     (PRODUCT_MODBUS_RTU_T35_US != 0UL))
    t15_us = PRODUCT_MODBUS_RTU_T15_US;
    t35_us = PRODUCT_MODBUS_RTU_T35_US;
#else
    uint32_t character_time_us =
        SerialConfiguration_GetCharacterTimeUs(configuration);
    if (configuration->line.baud_rate > 19200UL)
    {
        t15_us = 750UL;
        t35_us = 1750UL;
    }
    else
    {
        t15_us = ((character_time_us * 15UL) + 9UL) / 10UL;
        t35_us = ((character_time_us * 35UL) + 9UL) / 10UL;
    }
#endif
    return (BspRs485_SetRtuTimingUs(
                BSP_RS485_PORT_MACHINE_MASTER,
                t15_us, t35_us) == BSP_RS485_STATUS_OK);
}

static bool LoadActiveConfiguration(void)
{
    ModbusSerialPortConfiguration_t active;

    if (!ModbusRegisterAdapter_GetActiveSerialConfiguration(1U, &active) ||
        (active.serial.role != SERIAL_ROLE_MODBUS_MASTER) ||
        !ConfigureReceiveTiming(&active.serial))
    {
        return false;
    }
    s_master.protocol = active.serial.protocol;
    s_master.response_timeout_ms = active.serial.response_timeout_ms;
    return true;
}

static size_t GetRxCapacity(void)
{
    return (s_master.protocol == SERIAL_PROTOCOL_MODBUS_ASCII) ?
        (size_t)MODBUS_ASCII_MAX_ADU_LENGTH :
        (size_t)MODBUS_RTU_MAX_ADU_LENGTH;
}

static void SetError(ModbusMasterStatus_t status)
{
    (void)BspRs485_CancelReceive(BSP_RS485_PORT_MACHINE_MASTER);
    s_master.result.state = PRODUCT_MODBUS_MASTER_ERROR;
    s_master.result.status = status;
    s_master.transport_state = MASTER_TRANSPORT_IDLE;
}

static bool DetectAsciiEnd(void)
{
    size_t count;
    size_t index;
    BspRs485Status_t status;

    if (s_master.protocol != SERIAL_PROTOCOL_MODBUS_ASCII)
    {
        return true;
    }
    status = BspRs485_GetReceiveCount(BSP_RS485_PORT_MACHINE_MASTER, &count);
    if (status == BSP_RS485_STATUS_NO_TRANSFER)
    {
        return true;
    }
    if (status != BSP_RS485_STATUS_OK)
    {
        return false;
    }
    for (index = s_master.rx_scanned_length; index < count; index++)
    {
        if ((index != 0U) &&
            (s_master.rx_buffer[index - 1U] == MODBUS_ASCII_CR_CHARACTER) &&
            (s_master.rx_buffer[index] == MODBUS_ASCII_LF_CHARACTER))
        {
            s_master.rx_scanned_length = index + 1U;
            return (BspRs485_CompleteReceive(
                        BSP_RS485_PORT_MACHINE_MASTER,
                        index + 1U) == BSP_RS485_STATUS_OK);
        }
    }
    s_master.rx_scanned_length = count;
    return true;
}

bool L03_ProductModbusMaster_Init(void)
{
    (void)memset(&s_master, 0, sizeof(s_master));
    if (!LoadActiveConfiguration())
    {
        return false;
    }
    s_master.result.state = PRODUCT_MODBUS_MASTER_IDLE;
    s_master.initialized = true;
    return true;
}

bool L03_ProductModbusMaster_Submit(const ModbusMasterRequest_t *request)
{
    if (!s_master.initialized || (request == NULL) ||
        (s_master.transport_state != MASTER_TRANSPORT_IDLE) ||
        (s_master.result.state == PRODUCT_MODBUS_MASTER_BUSY))
    {
        return false;
    }
    if (!LoadActiveConfiguration())
    {
        return false;
    }

    s_master.request = *request;
    if ((request->function_code == 0x06U) ||
        (request->function_code == 0x10U))
    {
        if ((request->write_values == NULL) ||
            (request->quantity > MODBUS_MASTER_WRITE_QUANTITY_MAX))
        {
            return false;
        }
        (void)memcpy(s_master.write_values, request->write_values,
                     (size_t)request->quantity * sizeof(uint16_t));
        s_master.request.write_values = s_master.write_values;
    }

    s_master.result.status = ModbusMaster_BuildRequest(
        s_master.protocol,
        &s_master.request,
        s_master.tx_buffer,
        sizeof(s_master.tx_buffer),
        &s_master.tx_length);
    if (s_master.result.status != MODBUS_MASTER_STATUS_OK)
    {
        s_master.result.state = PRODUCT_MODBUS_MASTER_ERROR;
        return false;
    }
    if (SerialService_Write(
            HAL_SERIAL_PORT_1,
            s_master.tx_buffer,
            s_master.tx_length) != SERIAL_SERVICE_STATUS_OK)
    {
        SetError(MODBUS_MASTER_STATUS_FRAME_ERROR);
        return false;
    }

    s_master.elapsed_ms = 0U;
    s_master.result.value_count = 0U;
    s_master.result.exception = MODBUS_EXCEPTION_NONE;
    s_master.result.state = PRODUCT_MODBUS_MASTER_BUSY;
    s_master.transport_state = MASTER_TRANSPORT_WAIT_TX;
    return true;
}

void L03_ProductModbusMaster_Tick1ms(void)
{
    if ((s_master.result.state == PRODUCT_MODBUS_MASTER_BUSY) &&
        (s_master.elapsed_ms < UINT32_MAX))
    {
        s_master.elapsed_ms++;
    }
}

void L03_ProductModbusMaster_Process(void)
{
    size_t rx_length;
    bool timing_error;
    BspRs485Status_t receive_status;

    if (!s_master.initialized ||
        (s_master.result.state != PRODUCT_MODBUS_MASTER_BUSY))
    {
        return;
    }
    if (s_master.elapsed_ms >= s_master.response_timeout_ms)
    {
        SetError(MODBUS_MASTER_STATUS_TIMEOUT);
        return;
    }

    if (s_master.transport_state == MASTER_TRANSPORT_WAIT_TX)
    {
        if (!HalSerial_IsWriteBusy(HAL_SERIAL_PORT_1))
        {
            s_master.rx_scanned_length = 0U;
            if (BspRs485_StartReceive(
                    BSP_RS485_PORT_MACHINE_MASTER,
                    s_master.rx_buffer,
                    GetRxCapacity()) != BSP_RS485_STATUS_OK)
            {
                SetError(MODBUS_MASTER_STATUS_FRAME_ERROR);
                return;
            }
            s_master.transport_state = MASTER_TRANSPORT_WAIT_RESPONSE;
        }
        return;
    }

    if (!DetectAsciiEnd())
    {
        SetError(MODBUS_MASTER_STATUS_FRAME_ERROR);
        return;
    }
    receive_status = BspRs485_TakeReceivedFrame(
        BSP_RS485_PORT_MACHINE_MASTER, &rx_length);
    if (receive_status == BSP_RS485_STATUS_NO_DATA)
    {
        return;
    }
    if ((receive_status != BSP_RS485_STATUS_OK) ||
        (BspRs485_GetLastReceiveTimingError(
             BSP_RS485_PORT_MACHINE_MASTER,
             &timing_error) != BSP_RS485_STATUS_OK) ||
        ((s_master.protocol == SERIAL_PROTOCOL_MODBUS_RTU) && timing_error))
    {
        SetError(MODBUS_MASTER_STATUS_FRAME_ERROR);
        return;
    }

    s_master.result.status = ModbusMaster_ParseResponse(
        s_master.protocol,
        &s_master.request,
        s_master.rx_buffer,
        rx_length,
        s_master.result.values,
        MODBUS_MASTER_READ_QUANTITY_MAX,
        &s_master.result.value_count,
        &s_master.result.exception);
    s_master.result.state =
        (s_master.result.status == MODBUS_MASTER_STATUS_OK) ?
            PRODUCT_MODBUS_MASTER_COMPLETE : PRODUCT_MODBUS_MASTER_ERROR;
    s_master.transport_state = MASTER_TRANSPORT_IDLE;
}

bool L03_ProductModbusMaster_GetResult(ProductModbusMasterResult_t *result)
{
    if ((result == NULL) || !s_master.initialized)
    {
        return false;
    }
    *result = s_master.result;
    return true;
}

void L03_ProductModbusMaster_ClearResult(void)
{
    if (s_master.initialized &&
        (s_master.result.state != PRODUCT_MODBUS_MASTER_BUSY))
    {
        s_master.result.state = PRODUCT_MODBUS_MASTER_IDLE;
        s_master.result.status = MODBUS_MASTER_STATUS_OK;
        s_master.result.exception = MODBUS_EXCEPTION_NONE;
        s_master.result.value_count = 0U;
    }
}
