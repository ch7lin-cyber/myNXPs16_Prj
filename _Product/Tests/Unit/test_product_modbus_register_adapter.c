#include <assert.h>
#include <stdint.h>

#include "ProductConfig.h"
#include "EventService.h"
#include "FaultService.h"
#include "ModbusRegisterAdapter.h"
#include "HalPwm.h"
#include "HalDac.h"
#include "PwmOutputService.h"
#include "bsp_analog_output.h"
#include "product_modbus_register_adapter.h"
#include "product_fram_bank_test.h"
#include "product_temperature_input_types.h"

typedef struct
{
    uint32_t periodMs;
    uint16_t dutyPermille;
    HalPwmPeriodUpdateMode_t updateMode;
} MockPwmDriver_t;

static MockPwmDriver_t g_pwmDriver[PRODUCT_MODBUS_PWM_CHANNEL_COUNT];

typedef struct
{
    uint16_t code;
    bool failNextWrite;
} MockDacDriver_t;

static MockDacDriver_t g_dacDriver[PRODUCT_MODBUS_DAC_CHANNEL_COUNT];

static HalDacStatus_t MockDacInitialize(void *context)
{
    MockDacDriver_t *driver = (MockDacDriver_t *)context;
    driver->code = 0U;
    driver->failNextWrite = false;
    return HAL_DAC_STATUS_OK;
}

static HalDacStatus_t MockDacWriteCode(void *context, uint16_t code)
{
    MockDacDriver_t *driver = (MockDacDriver_t *)context;

    if (driver->failNextWrite)
    {
        driver->failNextWrite = false;
        return HAL_DAC_STATUS_IO_ERROR;
    }
    driver->code = code;
    return HAL_DAC_STATUS_OK;
}

static HalPwmStatus_t MockPwmInitialize(void *context)
{
    MockPwmDriver_t *driver = (MockPwmDriver_t *)context;
    driver->periodMs = PWM_OUTPUT_PERIOD_DEFAULT_MS;
    driver->dutyPermille = 0U;
    driver->updateMode = HAL_PWM_PERIOD_UPDATE_IMMEDIATE;
    return HAL_PWM_STATUS_OK;
}

static HalPwmStatus_t MockPwmSetDuty(void *context, uint16_t dutyPermille)
{
    ((MockPwmDriver_t *)context)->dutyPermille = dutyPermille;
    return HAL_PWM_STATUS_OK;
}

static HalPwmStatus_t MockPwmSetPeriod(
    void *context,
    uint32_t periodMs,
    HalPwmPeriodUpdateMode_t updateMode)
{
    MockPwmDriver_t *driver = (MockPwmDriver_t *)context;
    driver->periodMs = periodMs;
    driver->updateMode = updateMode;
    return HAL_PWM_STATUS_OK;
}

static bool MockPwmIsInhibited(uint8_t channel, void *context)
{
    (void)channel;
    (void)context;
    return false;
}

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
    assert(values[7] == 48U);
    assert(values[8] == 0U);
    assert(values[9] == 0U);
}

static void TestFactoryFramRegistersRequireFactoryMode(void)
{
    ModbusSlaveRegisterInterface_t interface;
    uint16_t values[13];

    ProductModbusRegisterAdapter_GetInterface(&interface);
    assert(interface.read_holding_registers(
               interface.context,
               PRODUCT_MODBUS_FACTORY_FRAM_BASE_ADDRESS,
               13U, values) == MODBUS_EXCEPTION_NONE);
    assert(values[0] == 0U);
    assert(values[1] == PRODUCT_FRAM_TEST_STATE_IDLE);
    assert(values[2] == PRODUCT_FRAM_TEST_ERROR_NONE);

    assert(interface.write_single_register(
               interface.context,
               PRODUCT_MODBUS_FACTORY_FRAM_COMMAND_ADDRESS,
               PRODUCT_FACTORY_FRAM_COMMAND_START) ==
           MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE);
    assert(interface.read_holding_registers(
               interface.context,
               PRODUCT_MODBUS_FACTORY_FRAM_STATE_ADDRESS,
               2U, values) == MODBUS_EXCEPTION_NONE);
    assert(values[0] == PRODUCT_FRAM_TEST_STATE_FAILED);
    assert(values[1] == PRODUCT_FRAM_TEST_ERROR_FACTORY_MODE_LOCKED);
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
    const uint16_t applyKey[1] = {PRODUCT_MODBUS_APPLY_KEY_VALUE};
    TemperatureInputConfigurationChangedEvent_t event;

    ProductModbusRegisterAdapter_GetInterface(&interface);
    ProductModbusRegisterAdapter_DiscardPendingTemperatureInputConfig();
    assert(!ProductModbusRegisterAdapter_HasPendingTemperatureInputConfig());
    assert(interface.write_multiple_registers(
               interface.context, 0x1003U,
               filterTwoSeconds, 2U) == MODBUS_EXCEPTION_NONE);
    assert(interface.write_single_register(
               interface.context, 0x1007U, 62U) == MODBUS_EXCEPTION_NONE);

    assert(ProductModbusRegisterAdapter_HasPendingTemperatureInputConfig());
    ProductModbusRegisterAdapter_GetTemperatureInputConfig(&activeConfig);
    ProductModbusRegisterAdapter_GetPendingTemperatureInputConfig(
        &pendingConfig);
    assert(activeConfig.filterTimeConstantSeconds == 0.5F);
    assert(activeConfig.sensorType == 48U);
    assert(pendingConfig.filterTimeConstantSeconds == 2.0F);
    assert(pendingConfig.sensorType == 62U);

    assert(interface.read_holding_registers(
               interface.context, 0x1003U, 6U, activeRegisters) ==
           MODBUS_EXCEPTION_NONE);
    assert(activeRegisters[0] == 0x3F00U);
    assert(activeRegisters[1] == 0x0000U);
    assert(activeRegisters[4] == 48U);
    assert(activeRegisters[5] == 0U);

    assert(interface.write_single_register(
               interface.context, 0x1007U, 96U) ==
           MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE);
    ProductModbusRegisterAdapter_GetPendingTemperatureInputConfig(
        &pendingConfig);
    assert(pendingConfig.sensorType == 62U);

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
    assert(activeConfig.sensorType == 62U);
    assert(ProductModbusRegisterAdapter_GetTemperatureInputConfigurationRevision() ==
           1U);
    assert(EventService_GetTemperatureInputConfigurationChanged(0U, &event));
    assert(event.configuration_revision == 1U);
    assert(event.changed_mask == EVENT_TEMPERATURE_INPUT_CHANGE_ALL);
    assert(event.old_configuration.filter_time_constant_seconds == 0.5F);
    assert(event.old_configuration.sensor_type == 48U);
    assert(event.new_configuration.filter_time_constant_seconds == 2.0F);
    assert(event.new_configuration.sensor_type == 62U);
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
               interface.context, PRODUCT_MODBUS_SENSOR_TYPE_ADDRESS, 48U) ==
           MODBUS_EXCEPTION_NONE);
    assert(interface.write_single_register(
               interface.context, PRODUCT_MODBUS_APPLY_KEY_ADDRESS,
               PRODUCT_MODBUS_APPLY_KEY_VALUE) == MODBUS_EXCEPTION_NONE);
    assert(EventService_GetTemperatureInputConfigurationChanged(0U, &event));
    assert(event.changed_mask == EVENT_TEMPERATURE_INPUT_CHANGE_SENSOR_TYPE);
    assert(event.configuration_revision == 2U);

    assert(interface.write_single_register(
               interface.context, PRODUCT_MODBUS_SENSOR_TYPE_ADDRESS, 62U) ==
           MODBUS_EXCEPTION_NONE);
    assert(interface.write_single_register(
               interface.context, PRODUCT_MODBUS_APPLY_KEY_ADDRESS,
               PRODUCT_MODBUS_APPLY_KEY_VALUE) ==
           MODBUS_EXCEPTION_SERVER_DEVICE_FAILURE);
    ProductModbusRegisterAdapter_GetTemperatureInputConfig(&activeConfig);
    assert(activeConfig.sensorType == 48U);
    assert(ProductModbusRegisterAdapter_HasPendingTemperatureInputConfig());
    assert(ProductModbusRegisterAdapter_GetTemperatureInputConfigurationRevision() ==
           2U);

    assert(EventService_Acknowledge(
        event.event_id, EVENT_ACK_TEMPERATURE_INPUT_REQUIRED_DEFAULT));
    assert(interface.write_single_register(
               interface.context, PRODUCT_MODBUS_APPLY_KEY_ADDRESS,
               PRODUCT_MODBUS_APPLY_KEY_VALUE) == MODBUS_EXCEPTION_NONE);
    ProductModbusRegisterAdapter_GetTemperatureInputConfig(&activeConfig);
    assert(activeConfig.sensorType == 62U);
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
               interface.context, PRODUCT_MODBUS_SENSOR_TYPE_ADDRESS, 62U) ==
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
               interface.context, 0x1007U, 62U) ==
           MODBUS_EXCEPTION_NONE);
    assert(ProductModbusRegisterAdapter_HasPendingTemperatureInputConfig());

    ProductModbusRegisterAdapter_GetTemperatureInputConfig(&activeConfig);
    ProductModbusRegisterAdapter_GetPendingTemperatureInputConfig(
        &pendingConfig);
    assert(activeConfig.sensorType == 48U);
    assert(pendingConfig.sensorType == 62U);

    ProductModbusRegisterAdapter_DiscardPendingTemperatureInputConfig();
}

static void TestFourIndependentTemperatureInputs(void)
{
    ModbusSlaveRegisterInterface_t interface;
    product_temperature_input_config_t channelZero;
    product_temperature_input_config_t channelOne;
    TemperatureInputConfigurationChangedEvent_t event;
    uint16_t channelOneImage[10];
    const uint16_t channelOneSensorAndApply[3] =
        {113U, 0U, PRODUCT_MODBUS_APPLY_KEY_VALUE};

    ProductModbusRegisterAdapter_GetInterface(&interface);
    ProductModbusRegisterAdapter_DiscardPendingTemperatureInputConfigForChannel(
        1U);

    assert(interface.write_multiple_registers(
               interface.context, 0x1017U,
               channelOneSensorAndApply, 3U) == MODBUS_EXCEPTION_NONE);

    ProductModbusRegisterAdapter_GetTemperatureInputConfig(&channelZero);
    assert(ProductModbusRegisterAdapter_GetTemperatureInputConfigForChannel(
        1U, &channelOne));
    assert(channelZero.sensorType == 62U);
    assert(channelOne.sensorType == 113U);
    assert(interface.read_holding_registers(
               interface.context, 0x1010U, 10U, channelOneImage) ==
           MODBUS_EXCEPTION_NONE);
    assert(channelOneImage[7] == 113U);
    assert(channelOneImage[8] == 0U);
    assert(ProductModbusRegisterAdapter_GetTemperatureInputConfigurationRevisionForChannel(
               1U) == 1U);
    assert(EventService_GetTemperatureInputConfigurationChanged(1U, &event));
    assert(event.new_configuration.sensor_type == 113U);
    assert(EventService_Acknowledge(
        event.event_id, EVENT_ACK_TEMPERATURE_INPUT_REQUIRED_DEFAULT));
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
    /* The former Thermocouple-family value is not a Sensor Type. */
    assert(interface.write_single_register(
               interface.context, PRODUCT_MODBUS_SENSOR_TYPE_ADDRESS, 95U) ==
           MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE);
    /* The former TC Type register is retained as read-only Reserved. */
    assert(interface.write_single_register(
               interface.context, PRODUCT_MODBUS_SENSOR_RESERVED_ADDRESS,
               PRODUCT_SENSOR_TYPE_TC_K) ==
           MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS);
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

static void TestPwmPendingAndApply(void)
{
    ModbusSlaveRegisterInterface_t interface;
    product_pwm_output_config_t config;
    uint16_t image[15];
    const uint16_t channelOneToThreeAndApply[10] =
    {
        2500U, 750U, PRODUCT_MODBUS_PWM_UPDATE_IMMEDIATE,
        100U, 500U, PRODUCT_MODBUS_PWM_UPDATE_NEXT_CYCLE,
        10000U, 1000U, PRODUCT_MODBUS_PWM_UPDATE_NEXT_CYCLE,
        PRODUCT_MODBUS_PWM_APPLY_KEY_VALUE
    };
    const uint16_t invalidChannelZero[2] = {750U, 1001U};

    ProductModbusRegisterAdapter_GetInterface(&interface);
    ProductModbusRegisterAdapter_DiscardPendingPwmConfig();
    assert(interface.read_holding_registers(
               interface.context, PRODUCT_MODBUS_PWM_BASE_ADDRESS,
               15U, image) == MODBUS_EXCEPTION_NONE);
    assert(image[0] == PWM_OUTPUT_PERIOD_DEFAULT_MS);
    assert(image[1] == 0U);
    assert(image[2] == PRODUCT_MODBUS_PWM_UPDATE_NEXT_CYCLE);
    assert(image[12] == 0U);
    assert(image[13] == 0U);
    assert(image[14] == 0U);

    assert(interface.write_single_register(
               interface.context, 0x1300U, 500U) == MODBUS_EXCEPTION_NONE);
    assert(interface.write_single_register(
               interface.context, 0x1301U, 333U) == MODBUS_EXCEPTION_NONE);
    assert(interface.write_single_register(
               interface.context, 0x1302U,
               PRODUCT_MODBUS_PWM_UPDATE_NEXT_CYCLE) ==
           MODBUS_EXCEPTION_NONE);
    assert(ProductModbusRegisterAdapter_GetPwmPendingMask() == 0x0001U);
    assert(ProductModbusRegisterAdapter_GetPwmConfig(0U, &config));
    assert(config.periodMs == PWM_OUTPUT_PERIOD_DEFAULT_MS);
    assert(config.dutyPermille == 0U);
    assert(ProductModbusRegisterAdapter_GetPendingPwmConfig(0U, &config));
    assert(config.periodMs == 500U);
    assert(config.dutyPermille == 333U);

    assert(interface.write_single_register(
               interface.context, 0x1300U,
               PWM_OUTPUT_PERIOD_MIN_MS - 1U) ==
           MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE);
    assert(interface.write_single_register(
               interface.context, PRODUCT_MODBUS_PWM_APPLY_KEY_ADDRESS,
               0x5A5AU) == MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE);
    assert(ProductModbusRegisterAdapter_GetPwmPendingMask() == 0x0001U);
    assert(interface.write_single_register(
               interface.context, PRODUCT_MODBUS_PWM_APPLY_KEY_ADDRESS,
               PRODUCT_MODBUS_PWM_APPLY_KEY_VALUE) == MODBUS_EXCEPTION_NONE);
    assert(g_pwmDriver[0].periodMs == 500U);
    assert(g_pwmDriver[0].dutyPermille == 333U);
    assert(g_pwmDriver[0].updateMode == HAL_PWM_PERIOD_UPDATE_NEXT_CYCLE);
    assert(ProductModbusRegisterAdapter_GetPwmConfigurationRevision() == 1U);
    assert(ProductModbusRegisterAdapter_GetPwmPendingMask() == 0U);

    assert(interface.write_multiple_registers(
               interface.context, 0x1303U,
               channelOneToThreeAndApply, 10U) == MODBUS_EXCEPTION_NONE);
    assert(g_pwmDriver[1].periodMs == 2500U);
    assert(g_pwmDriver[1].dutyPermille == 750U);
    assert(g_pwmDriver[1].updateMode == HAL_PWM_PERIOD_UPDATE_IMMEDIATE);
    assert(g_pwmDriver[2].periodMs == 100U);
    assert(g_pwmDriver[2].dutyPermille == 500U);
    assert(g_pwmDriver[3].periodMs == 10000U);
    assert(g_pwmDriver[3].dutyPermille == 1000U);
    assert(ProductModbusRegisterAdapter_GetPwmConfigurationRevision() == 2U);

    assert(interface.write_multiple_registers(
               interface.context, 0x1300U,
               invalidChannelZero, 2U) ==
           MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE);
    assert(ProductModbusRegisterAdapter_GetPwmPendingMask() == 0U);
    assert(ProductModbusRegisterAdapter_GetPendingPwmConfig(0U, &config));
    assert(config.periodMs == 500U);
    assert(config.dutyPermille == 333U);

    assert(interface.write_single_register(
               interface.context, 0x1300U, 500U) == MODBUS_EXCEPTION_NONE);
    assert(interface.write_single_register(
               interface.context, PRODUCT_MODBUS_PWM_APPLY_KEY_ADDRESS,
               PRODUCT_MODBUS_PWM_APPLY_KEY_VALUE) == MODBUS_EXCEPTION_NONE);
    assert(ProductModbusRegisterAdapter_GetPwmConfigurationRevision() == 2U);
    assert(ProductModbusRegisterAdapter_GetPwmPendingMask() == 0U);

    assert(interface.write_single_register(
               interface.context, PRODUCT_MODBUS_PWM_REVISION_ADDRESS,
               7U) == MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS);
}

static void TestDacPendingApplyAndRollback(void)
{
    ModbusSlaveRegisterInterface_t interface;
    uint16_t image[9];
    uint16_t code;
    const uint16_t channelOneToThreeAndApply[4] =
    {
        0x4000U, 0x8000U, 0xFFFFU,
        PRODUCT_MODBUS_DAC_APPLY_KEY_VALUE
    };
    const uint16_t channelZeroAndOne[2] = {0x1111U, 0x2222U};
    const uint16_t invalidRange[3] =
    {
        0x3333U, PRODUCT_MODBUS_DAC_APPLY_KEY_VALUE, 0U
    };

    ProductModbusRegisterAdapter_GetInterface(&interface);
    ProductModbusRegisterAdapter_DiscardPendingDacCodes();
    assert(interface.read_holding_registers(
               interface.context, PRODUCT_MODBUS_DAC_BASE_ADDRESS,
               9U, image) == MODBUS_EXCEPTION_NONE);
    assert(image[0] == 0U);
    assert(image[1] == 0U);
    assert(image[2] == 0U);
    assert(image[3] == 0U);
    assert(image[4] == 0U);
    assert(image[5] == 0U);
    assert(image[6] == 0U);
    assert(image[7] == PRODUCT_MODBUS_DAC_STATUS_READY);
    assert(image[8] == PRODUCT_MODBUS_DAC_FAILED_CHANNEL_NONE);

    assert(interface.write_single_register(
               interface.context, PRODUCT_MODBUS_DAC_BASE_ADDRESS,
               0x8000U) == MODBUS_EXCEPTION_NONE);
    assert(ProductModbusRegisterAdapter_GetDacPendingMask() == 0x0001U);
    assert(ProductModbusRegisterAdapter_GetDacCode(0U, &code));
    assert(code == 0U);
    assert(ProductModbusRegisterAdapter_GetPendingDacCode(0U, &code));
    assert(code == 0x8000U);
    assert(g_dacDriver[0].code == 0U);

    assert(interface.write_single_register(
               interface.context, PRODUCT_MODBUS_DAC_APPLY_KEY_ADDRESS,
               0x5A5AU) == MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE);
    assert(interface.write_single_register(
               interface.context, PRODUCT_MODBUS_DAC_APPLY_KEY_ADDRESS,
               PRODUCT_MODBUS_DAC_APPLY_KEY_VALUE) == MODBUS_EXCEPTION_NONE);
    assert(g_dacDriver[0].code == 0x8000U);
    assert(ProductModbusRegisterAdapter_GetDacConfigurationRevision() == 1U);
    assert(ProductModbusRegisterAdapter_GetDacPendingMask() == 0U);
    assert(interface.read_holding_registers(
               interface.context, PRODUCT_MODBUS_DAC_BASE_ADDRESS,
               9U, image) == MODBUS_EXCEPTION_NONE);
    assert(image[0] == 0x8000U);
    assert(image[4] == 0U);
    assert(image[5] == 1U);
    assert(image[6] == 0U);

    assert(interface.write_multiple_registers(
               interface.context, PRODUCT_MODBUS_DAC_BASE_ADDRESS + 1U,
               channelOneToThreeAndApply, 4U) == MODBUS_EXCEPTION_NONE);
    assert(g_dacDriver[1].code == 0x4000U);
    assert(g_dacDriver[2].code == 0x8000U);
    assert(g_dacDriver[3].code == 0xFFFFU);
    assert(ProductModbusRegisterAdapter_GetDacConfigurationRevision() == 2U);

    assert(interface.write_multiple_registers(
               interface.context, PRODUCT_MODBUS_DAC_BASE_ADDRESS,
               channelZeroAndOne, 2U) == MODBUS_EXCEPTION_NONE);
    g_dacDriver[1].failNextWrite = true;
    assert(interface.write_single_register(
               interface.context, PRODUCT_MODBUS_DAC_APPLY_KEY_ADDRESS,
               PRODUCT_MODBUS_DAC_APPLY_KEY_VALUE) ==
           MODBUS_EXCEPTION_SERVER_DEVICE_FAILURE);
    assert(g_dacDriver[0].code == 0x8000U);
    assert(g_dacDriver[1].code == 0x4000U);
    assert(ProductModbusRegisterAdapter_GetDacConfigurationRevision() == 2U);
    assert(ProductModbusRegisterAdapter_GetDacPendingMask() == 0x0003U);
    assert(ProductModbusRegisterAdapter_GetDacStatus() ==
           PRODUCT_MODBUS_DAC_STATUS_APPLY_FAILED);
    assert(ProductModbusRegisterAdapter_GetDacFailedChannel() == 1U);
    assert(interface.read_holding_registers(
               interface.context, PRODUCT_MODBUS_DAC_STATUS_ADDRESS,
               2U, &image[7]) == MODBUS_EXCEPTION_NONE);
    assert(image[7] == PRODUCT_MODBUS_DAC_STATUS_APPLY_FAILED);
    assert(image[8] == 1U);

    assert(interface.write_single_register(
               interface.context, PRODUCT_MODBUS_DAC_APPLY_KEY_ADDRESS,
               PRODUCT_MODBUS_DAC_APPLY_KEY_VALUE) == MODBUS_EXCEPTION_NONE);
    assert(g_dacDriver[0].code == 0x1111U);
    assert(g_dacDriver[1].code == 0x2222U);
    assert(ProductModbusRegisterAdapter_GetDacConfigurationRevision() == 3U);
    assert(ProductModbusRegisterAdapter_GetDacStatus() ==
           PRODUCT_MODBUS_DAC_STATUS_READY);
    assert(ProductModbusRegisterAdapter_GetDacFailedChannel() ==
           PRODUCT_MODBUS_DAC_FAILED_CHANNEL_NONE);

    assert(interface.write_multiple_registers(
               interface.context, PRODUCT_MODBUS_DAC_BASE_ADDRESS + 3U,
               invalidRange, 3U) == MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE);
    assert(ProductModbusRegisterAdapter_GetDacPendingMask() == 0U);
    assert(interface.write_single_register(
               interface.context, PRODUCT_MODBUS_DAC_STATUS_ADDRESS,
               0U) == MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS);
}

int main(void)
{
    static const HalPwmDriverOps_t pwmOps =
        {MockPwmInitialize, MockPwmSetDuty, MockPwmSetPeriod};
    static const HalDacDriverOps_t dacOps =
        {MockDacInitialize, MockDacWriteCode};
    uint8_t channel;

    for (channel = 0U;
         channel < PRODUCT_MODBUS_PWM_CHANNEL_COUNT;
         channel++)
    {
        assert(HalPwm_RegisterDriver(
                   channel, &pwmOps, &g_pwmDriver[channel]) ==
               HAL_PWM_STATUS_OK);
    }
    assert(PwmOutputService_Initialize(
               PRODUCT_MODBUS_PWM_CHANNEL_COUNT,
               MockPwmIsInhibited, NULL) == PWM_OUTPUT_STATUS_OK);
    for (channel = 0U;
         channel < PRODUCT_MODBUS_DAC_CHANNEL_COUNT;
         channel++)
    {
        assert(HalDac_RegisterDriver(
                   channel, &dacOps, &g_dacDriver[channel]) ==
               HAL_DAC_STATUS_OK);
    }
    assert(BspAnalogOutput_Initialize());
    TestDefaultRegisterImage();
    TestFactoryFramRegistersRequireFactoryMode();
    TestMonitorAndReadOnlyRegisters();
    TestSingleWriteIsPending();
    TestWritableConfiguration();
    TestEventBlocksApplyUntilAcknowledged();
    TestApplyWithoutEffectiveChangeDoesNotRaiseEvent();
    TestFourIndependentTemperatureInputs();
    TestInvalidRanges();
    TestProductSerialPolicy();
    TestProductDiagnosticsFaultRegisters();
    TestProductVersionRegisters();
    TestPwmPendingAndApply();
    TestDacPendingApplyAndRollback();
    return 0;
}
