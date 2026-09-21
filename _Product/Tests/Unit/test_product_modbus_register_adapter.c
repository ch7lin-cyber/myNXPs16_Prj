#include <assert.h>
#include <stdint.h>

#include "product_modbus_register_adapter.h"

static void TestDefaultRegisterImage(void)
{
    ModbusSlaveRegisterInterface_t interface;
    uint16_t values[9];

    ProductModbusRegisterAdapter_GetInterface(&interface);
    assert(interface.read_holding_registers(
               interface.context, 0x1000U, 9U, values) ==
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

    ProductModbusRegisterAdapter_DiscardPendingTemperatureInputConfig();
    assert(!ProductModbusRegisterAdapter_HasPendingTemperatureInputConfig());
    ProductModbusRegisterAdapter_GetPendingTemperatureInputConfig(
        &pendingConfig);
    assert(pendingConfig.filterTimeConstantSeconds == 0.5F);
    assert(pendingConfig.sensorType == 62U);
    assert(pendingConfig.tcLinearization == 46U);
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
               interface.context, 0x1008U, 2U, &value) ==
           MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS);
    assert(interface.write_single_register(
               interface.context, 0x1007U, 0U) ==
           MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE);
}

int main(void)
{
    TestDefaultRegisterImage();
    TestMonitorAndReadOnlyRegisters();
    TestWritableConfiguration();
    TestSingleWriteIsPending();
    TestInvalidRanges();
    return 0;
}
