#include <assert.h>
#include <stdint.h>

#include "ProductConfig.h"
#include "EventService.h"
#include "FaultService.h"
#include "ModbusRegisterAdapter.h"
#include "product_modbus_register_adapter.h"

static void TestDefaultRegisterImage(void)
{
    ModbusSlaveRegisterInterface_t interface;
    uint16_t values[10];

    ProductModbusRegisterAdapter_GetInterface(&interface);
    assert(interface.read_holding_registers(
               interface.context, 0x1000U, 10U, values) ==
           MODBUS_EXCEPTION_NONE);
    assert(values[0] == 0x0000U);
    assert(values[1] == 0x0000U);
    assert(values[2] == 61U);
    assert(values[3] == 0x3F00U);
    assert(values[4] == 0x0000U);
    assert(values[5] == 0x0000U);
    assert(values[6] == 0x0000U);
    assert(values[7] == 62U);
    assert(values[8] == 46U);
    assert(values[9] == 0U);
}

static void TestMonitorAndReadOnlyRegisters(void)
{
    ModbusSlaveRegisterInterface_t interface;
    product_temperature_input_monitor_t monitor = {25.5F, 65U, 24.25F};
    uint16_t values[3];

    ProductModbusRegisterAdapter_GetInterface(&interface);
    assert(ProductModbusRegisterAdapter_SetTemperatureInputMonitor(&monitor));
    assert(interface.read_holding_registers(
               interface.context, 0x1000U, 3U, values) ==
           MODBUS_EXCEPTION_NONE);
    assert(values[0] == 0x41CCU);
    assert(values[1] == 0x0000U);
    assert(values[2] == 65U);
    assert(interface.write_single_register(
               interface.context, 0x1002U, 0U) ==
           MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS);
}

static void TestWritableConfiguration(void)
{
    ModbusSlaveRegisterInterface_t interface;
    product_temperature_input_config_t activeConfig;
    product_temperature_input_config_t pendingConfig;
    uint16_t activeRegisters[6];
    const uint16_t filterTwoSeconds[2] = {0x4000U, 0x0000U};
    const uint16_t sensorAndLinearization[2] = {95U, 48U};
    const uint16_t invalidSensorAndLinearization[2] = {96U, 48U};
    const uint16_t applyKey[1] = {PRODUCT_MODBUS_APPLY_KEY_VALUE};
    TemperatureInputConfigurationChangedEvent_t event;

    ProductModbusRegisterAdapter_GetInterface(&interface);
    ProductModbusRegisterAdapter_DiscardPendingTemperatureInputConfig();
    assert(!ProductModbusRegisterAdapter_HasPendingTemperatureInputConfig());
    assert(interface.write_multiple_registers(
               interface.context, 0x1003U,
               filterTwoSeconds, 2U) == MODBUS_EXCEPTION_NONE);
    assert(interface.write_multiple_registers(
               interface.context, 0x1007U,
               sensorAndLinearization, 2U) == MODBUS_EXCEPTION_NONE);

    assert(ProductModbusRegisterAdapter_HasPendingTemperatureInputConfig());
    ProductModbusRegisterAdapter_GetTemperatureInputConfig(&activeConfig);
    ProductModbusRegisterAdapter_GetPendingTemperatureInputConfig(
        &pendingConfig);
    assert(activeConfig.filterTimeConstantSeconds == 0.5F);
    assert(activeConfig.sensorType == 62U);
    assert(activeConfig.tcLinearization == 46U);
    assert(pendingConfig.filterTimeConstantSeconds == 2.0F);
    assert(pendingConfig.sensorType == 95U);
    assert(pendingConfig.tcLinearization == 48U);

    assert(interface.read_holding_registers(
               interface.context, 0x1003U, 6U, activeRegisters) ==
           MODBUS_EXCEPTION_NONE);
    assert(activeRegisters[0] == 0x3F00U);
    assert(activeRegisters[1] == 0x0000U);
    assert(activeRegisters[4] == 62U);
    assert(activeRegisters[5] == 46U);

    assert(interface.write_multiple_registers(
               interface.context, 0x1007U,
               invalidSensorAndLinearization, 2U) ==
           MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE);
    ProductModbusRegisterAdapter_GetPendingTemperatureInputConfig(
        &pendingConfig);
    assert(pendingConfig.sensorType == 95U);
    assert(pendingConfig.tcLinearization == 48U);

    assert(interface.write_single_register(
               interface.context, PRODUCT_MODBUS_APPLY_KEY_ADDRESS,
               0x5A5AU) == MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE);
    assert(ProductModbusRegisterAdapter_HasPendingTemperatureInputConfig());

    assert(interface.write_multiple_registers(
               interface.context, PRODUCT_MODBUS_APPLY_KEY_ADDRESS,
               applyKey, 1U) == MODBUS_EXCEPTION_NONE);
    assert(!ProductModbusRegisterAdapter_HasPendingTemperatureInputConfig());
    ProductModbusRegisterAdapter_GetTemperatureInputConfig(&activeConfig);
    assert(activeConfig.filterTimeConstantSeconds == 2.0F);
    assert(activeConfig.sensorType == 95U);
    assert(activeConfig.tcLinearization == 48U);
    assert(ProductModbusRegisterAdapter_GetTemperatureInputConfigurationRevision() ==
           1U);
    assert(EventService_GetTemperatureInputConfigurationChanged(0U, &event));
    assert(event.configuration_revision == 1U);
    assert(event.changed_mask == EVENT_TEMPERATURE_INPUT_CHANGE_ALL);
    assert(event.old_configuration.filter_time_constant_seconds == 0.5F);
    assert(event.old_configuration.sensor_type == 62U);
    assert(event.old_configuration.tc_linearization == 46U);
    assert(event.new_configuration.filter_time_constant_seconds == 2.0F);
    assert(event.new_configuration.sensor_type == 95U);
    assert(event.new_configuration.tc_linearization == 48U);
    assert(EventService_Acknowledge(
        event.event_id, EVENT_ACK_TEMPERATURE_INPUT_REQUIRED_DEFAULT));
    assert(!EventService_IsTemperatureInputConfigurationChangedPending(0U));
    assert(interface.write_single_register(
               interface.context, PRODUCT_MODBUS_APPLY_KEY_ADDRESS,
               PRODUCT_MODBUS_APPLY_KEY_VALUE) ==
           MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE);
}

static void TestEventBlocksApplyUntilAcknowledged(void)
{
    ModbusSlaveRegisterInterface_t interface;
    product_temperature_input_config_t activeConfig;
    TemperatureInputConfigurationChangedEvent_t event;

    ProductModbusRegisterAdapter_GetInterface(&interface);
    ProductModbusRegisterAdapter_DiscardPendingTemperatureInputConfig();

    assert(interface.write_single_register(
               interface.context, PRODUCT_MODBUS_SENSOR_TYPE_ADDRESS, 62U) ==
           MODBUS_EXCEPTION_NONE);
    assert(interface.write_single_register(
               interface.context, PRODUCT_MODBUS_APPLY_KEY_ADDRESS,
               PRODUCT_MODBUS_APPLY_KEY_VALUE) == MODBUS_EXCEPTION_NONE);
    assert(EventService_GetTemperatureInputConfigurationChanged(0U, &event));
    assert(event.changed_mask == EVENT_TEMPERATURE_INPUT_CHANGE_SENSOR_TYPE);
    assert(event.configuration_revision == 2U);

    assert(interface.write_single_register(
               interface.context, PRODUCT_MODBUS_SENSOR_TYPE_ADDRESS, 95U) ==
           MODBUS_EXCEPTION_NONE);
    assert(interface.write_single_register(
               interface.context, PRODUCT_MODBUS_APPLY_KEY_ADDRESS,
               PRODUCT_MODBUS_APPLY_KEY_VALUE) ==
           MODBUS_EXCEPTION_SERVER_DEVICE_FAILURE);
    ProductModbusRegisterAdapter_GetTemperatureInputConfig(&activeConfig);
    assert(activeConfig.sensorType == 62U);
    assert(ProductModbusRegisterAdapter_HasPendingTemperatureInputConfig());
    assert(ProductModbusRegisterAdapter_GetTemperatureInputConfigurationRevision() ==
           2U);

    assert(EventService_Acknowledge(
        event.event_id, EVENT_ACK_TEMPERATURE_INPUT_REQUIRED_DEFAULT));
    assert(interface.write_single_register(
               interface.context, PRODUCT_MODBUS_APPLY_KEY_ADDRESS,
               PRODUCT_MODBUS_APPLY_KEY_VALUE) == MODBUS_EXCEPTION_NONE);
    ProductModbusRegisterAdapter_GetTemperatureInputConfig(&activeConfig);
    assert(activeConfig.sensorType == 95U);
    assert(ProductModbusRegisterAdapter_GetTemperatureInputConfigurationRevision() ==
           3U);
    assert(EventService_GetTemperatureInputConfigurationChanged(0U, &event));
    assert(EventService_Acknowledge(
        event.event_id, EVENT_ACK_TEMPERATURE_INPUT_REQUIRED_DEFAULT));
}

static void TestApplyWithoutEffectiveChangeDoesNotRaiseEvent(void)
{
    ModbusSlaveRegisterInterface_t interface;
    uint16_t revision;

    ProductModbusRegisterAdapter_GetInterface(&interface);
    ProductModbusRegisterAdapter_DiscardPendingTemperatureInputConfig();
    revision =
        ProductModbusRegisterAdapter_GetTemperatureInputConfigurationRevision();
    assert(interface.write_single_register(
               interface.context, PRODUCT_MODBUS_SENSOR_TYPE_ADDRESS, 95U) ==
           MODBUS_EXCEPTION_NONE);
    assert(interface.write_single_register(
               interface.context, PRODUCT_MODBUS_APPLY_KEY_ADDRESS,
               PRODUCT_MODBUS_APPLY_KEY_VALUE) == MODBUS_EXCEPTION_NONE);
    assert(ProductModbusRegisterAdapter_GetTemperatureInputConfigurationRevision() ==
           revision);
    assert(!EventService_IsTemperatureInputConfigurationChangedPending(0U));
}

static void TestSingleWriteIsPending(void)
{
    ModbusSlaveRegisterInterface_t interface;
    product_temperature_input_config_t activeConfig;
    product_temperature_input_config_t pendingConfig;

    ProductModbusRegisterAdapter_GetInterface(&interface);
    ProductModbusRegisterAdapter_DiscardPendingTemperatureInputConfig();

    assert(interface.write_single_register(
               interface.context, 0x1007U, 95U) ==
           MODBUS_EXCEPTION_NONE);
    assert(ProductModbusRegisterAdapter_HasPendingTemperatureInputConfig());

    ProductModbusRegisterAdapter_GetTemperatureInputConfig(&activeConfig);
    ProductModbusRegisterAdapter_GetPendingTemperatureInputConfig(
        &pendingConfig);
    assert(activeConfig.sensorType == 62U);
    assert(pendingConfig.sensorType == 95U);

    ProductModbusRegisterAdapter_DiscardPendingTemperatureInputConfig();
}

static void TestInvalidRanges(void)
{
    ModbusSlaveRegisterInterface_t interface;
    uint16_t value;

    ProductModbusRegisterAdapter_GetInterface(&interface);
    assert(interface.read_holding_registers(
               interface.context, 0x0FFFU, 1U, &value) ==
           MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS);
    assert(interface.read_holding_registers(
               interface.context, 0x1009U, 2U, &value) ==
           MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS);
    assert(interface.write_single_register(
               interface.context, 0x1007U, 0U) ==
           MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE);
}

static void TestProductSerialPolicy(void)
{
    ModbusSlaveRegisterInterface_t interface;
    ModbusSerialPortConfiguration_t port0 =
    {
        {{115200UL, HAL_SERIAL_DATA_BITS_8, HAL_SERIAL_PARITY_NONE,
          HAL_SERIAL_STOP_BITS_1},
         SERIAL_PROTOCOL_MODBUS_RTU, SERIAL_ROLE_MODBUS_SLAVE, 1000UL},
        2U
    };
    ModbusSerialPortConfiguration_t port1 = port0;
    uint16_t value;

    port1.serial.role = SERIAL_ROLE_MODBUS_MASTER;
    port1.unit_id = 1U;
    assert(ModbusRegisterAdapter_InitializeSerialPort(0U, &port0));
    assert(ModbusRegisterAdapter_InitializeSerialPort(1U, &port1));
    ProductModbusRegisterAdapter_GetInterface(&interface);

    assert(interface.write_single_register(
               interface.context, 0x1200U,
               MODBUS_SERIAL_BAUD_230400) == MODBUS_EXCEPTION_NONE);
    assert(ModbusRegisterAdapter_ReadSerialRegister(0x1210U, &value) ==
           MODBUS_EXCEPTION_NONE);
    assert(value == MODBUS_SERIAL_BAUD_230400);

    assert(interface.write_single_register(
               interface.context, 0x1204U,
               SERIAL_PROTOCOL_RAW) == MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE);
    assert(interface.write_single_register(
               interface.context, 0x1205U,
               SERIAL_ROLE_MODBUS_MASTER) ==
           MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS);
    assert(interface.write_single_register(
               interface.context, 0x1216U, 7U) ==
           MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS);

    assert(interface.write_single_register(
               interface.context, 0x1208U,
               MODBUS_SERIAL_APPLY_KEY) == MODBUS_EXCEPTION_NONE);
    assert(ModbusRegisterAdapter_IsApplyRequested(0U));
    assert(ModbusRegisterAdapter_IsApplyRequested(1U));
    assert(ModbusRegisterAdapter_CancelApply(0U));
    assert(ModbusRegisterAdapter_CancelApply(1U));
}

static void TestProductDiagnosticsFaultRegisters(void)
{
    ModbusSlaveRegisterInterface_t interface;
    uint16_t values[11];
    const uint16_t invalidClear[2] =
        {FAULT_CODE_NVM_ERASE_FAILED, 0x0000U};
    const uint16_t validClear[2] =
        {FAULT_CODE_NVM_ERASE_FAILED,
         PRODUCT_DIAGNOSTICS_CLEAR_KEY_VALUE};

    FaultService_Initialize();
    ProductModbusRegisterAdapter_GetInterface(&interface);
    assert(FaultService_Raise(FAULT_CODE_NVM_ERASE_FAILED,
                              3U, 7U, 0x12345678UL));
    assert(interface.read_holding_registers(
               interface.context,
               PRODUCT_MODBUS_DIAGNOSTICS_BASE_ADDRESS,
               11U, values) == MODBUS_EXCEPTION_NONE);
    assert(values[0] == 1U);
    assert(values[1] == 0U);
    assert(values[2] == FAULT_CODE_NVM_ERASE_FAILED);
    assert(values[3] == 3U);
    assert(values[4] == 7U);
    assert(values[5] == 0x1234U);
    assert(values[6] == 0x5678U);
    assert(values[7] == 0U);
    assert(values[8] == 1U);
    assert(values[9] == 0U);
    assert(values[10] == 0U);

    assert(interface.write_single_register(
               interface.context,
               PRODUCT_MODBUS_DIAGNOSTICS_CLEAR_KEY_ADDRESS,
               PRODUCT_DIAGNOSTICS_CLEAR_KEY_VALUE) ==
           MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS);
    assert(interface.write_multiple_registers(
               interface.context,
               PRODUCT_MODBUS_DIAGNOSTICS_CLEAR_CODE_ADDRESS,
               invalidClear, 2U) == MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE);
    assert(FaultService_IsActive(FAULT_CODE_NVM_ERASE_FAILED));
    assert(interface.write_multiple_registers(
               interface.context,
               PRODUCT_MODBUS_DIAGNOSTICS_CLEAR_CODE_ADDRESS,
               validClear, 2U) == MODBUS_EXCEPTION_NONE);
    assert(!FaultService_IsActive(FAULT_CODE_NVM_ERASE_FAILED));
    assert(interface.read_holding_registers(
               interface.context,
               PRODUCT_MODBUS_DIAGNOSTICS_BASE_ADDRESS,
               3U, values) == MODBUS_EXCEPTION_NONE);
    assert(values[0] == 0U);
    assert(values[2] == FAULT_CODE_NONE);
}

static void TestProductVersionRegisters(void)
{
    ModbusSlaveRegisterInterface_t interface;
    uint16_t values[9];

    ProductModbusRegisterAdapter_GetInterface(&interface);
    assert(interface.read_holding_registers(
               interface.context, PRODUCT_MODBUS_VERSION_BASE_ADDRESS,
               9U, values) == MODBUS_EXCEPTION_NONE);
    assert(values[0] == PRODUCT_FIRMWARE_VERSION_U16);
    assert(values[1] == PRODUCT_FIRMWARE_VERSION_SUB1_U16);
    assert(values[2] == PRODUCT_FIRMWARE_VERSION_SUB2_U16);
    assert(values[3] == PRODUCT_COMPATIBLE_FIRMWARE_VERSION_MIN_U16);
    assert(values[4] == PRODUCT_COMPATIBLE_FIRMWARE_VERSION_MAX_U16);
    assert(values[5] == PRODUCT_COMPATIBLE_PARAMETER_VERSION_MIN_U16);
    assert(values[6] == PRODUCT_COMPATIBLE_PARAMETER_VERSION_MAX_U16);
    assert(values[7] == PRODUCT_COMPATIBLE_SOFTWARE_VERSION_MIN_U16);
    assert(values[8] == PRODUCT_COMPATIBLE_SOFTWARE_VERSION_MAX_U16);
    assert(interface.write_single_register(
               interface.context, PRODUCT_MODBUS_FW_VERSION_ADDRESS, 2U) ==
           MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS);
}

int main(void)
{
    TestDefaultRegisterImage();
    TestMonitorAndReadOnlyRegisters();
    TestSingleWriteIsPending();
    TestWritableConfiguration();
    TestEventBlocksApplyUntilAcknowledged();
    TestApplyWithoutEffectiveChangeDoesNotRaiseEvent();
    TestInvalidRanges();
    TestProductSerialPolicy();
    TestProductDiagnosticsFaultRegisters();
    TestProductVersionRegisters();
    return 0;
}
