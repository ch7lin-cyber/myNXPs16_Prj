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
    product_temperature_input_config_t config;
    const uint16_t filterTwoSeconds[2] = {0x4000U, 0x0000U};
    const uint16_t sensorAndLinearization[2] = {95U, 48U};
    const uint16_t invalidSensorAndLinearization[2] = {96U, 48U};

    ProductModbusRegisterAdapter_GetInterface(&interface);
    assert(interface.write_multiple_registers(
               interface.context, 0x1003U,
               filterTwoSeconds, 2U) == MODBUS_EXCEPTION_NONE);
    assert(interface.write_multiple_registers(
               interface.context, 0x1007U,
               sensorAndLinearization, 2U) == MODBUS_EXCEPTION_NONE);

    ProductModbusRegisterAdapter_GetTemperatureInputConfig(&config);
    assert(config.filterTimeConstantSeconds == 2.0F);
    assert(config.sensorType == 95U);
    assert(config.tcLinearization == 48U);

    assert(interface.write_multiple_registers(
               interface.context, 0x1007U,
               invalidSensorAndLinearization, 2U) ==
           MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE);
    ProductModbusRegisterAdapter_GetTemperatureInputConfig(&config);
    assert(config.sensorType == 95U);
    assert(config.tcLinearization == 48U);
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
    TestInvalidRanges();
    return 0;
}
