#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "ProductConfig.h"
#include "EventService.h"
#include "FaultService.h"
#include "ModbusRegisterAdapter.h"
#include "HalPwm.h"
#include "HalDac.h"
#include "HalGpio.h"
#include "PwmOutputService.h"
#include "DigitalInputService.h"
#include "DigitalOutputService.h"
#include "SafetyService.h"
#include "SnapshotService.h"
#include "SystemEventService.h"
#include "WarningService.h"
#include "bsp_analog_output.h"
#include "product_modbus_register_adapter.h"
#include "product_fram_bank_test.h"
#include "product_dip_switch_driver.h"
#include "product_adc_driver.h"
#include "product_sensor_configuration_consumer.h"
#include "product_rotary_switch_driver.h"
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

typedef struct
{
    bool state;
    bool failNextAccess;
} MockGpioDriver_t;

static MockGpioDriver_t g_digitalInput[PRODUCT_MODBUS_DI_CHANNEL_COUNT];
static MockGpioDriver_t g_digitalOutput[PRODUCT_MODBUS_DO_CHANNEL_COUNT];
static ProductDipSwitchSnapshot_t g_dipSwitchSnapshot;
static ProductRotarySwitchSnapshot_t g_rotarySwitchSnapshot;
static ProductAdcDriverDiagnostics_t g_adcDiagnostics[4U];

static ProductSensorConfigurationDiagnostics_t g_configDiagnostics[4U];

bool ProductSensorConfigurationConsumer_GetDiagnostics(
    uint8_t device, ProductSensorConfigurationDiagnostics_t *diagnostics)
{
    if ((device >= 4U) || (diagnostics == NULL)) return false;
    *diagnostics = g_configDiagnostics[device];
    return true;
}

bool AnalogInputService_GetDiagnostics(uint8_t device, AnalogInputDiagnostics_t *diagnostics)
{
    if ((device >= 4U) || (diagnostics == NULL)) return false;
    (void)memset(diagnostics, 0, sizeof(*diagnostics));
    diagnostics->online = true;
    diagnostics->driver_errors = 0x12345678UL;
    return true;
}

bool ProductAdcDriver_GetDiagnostics(
    uint8_t device, ProductAdcDriverDiagnostics_t *diagnostics)
{
    if ((device >= 4U) || (diagnostics == NULL))
    {
        return false;
    }
    *diagnostics = g_adcDiagnostics[device];
    return true;
}

bool ProductDipSwitchDriver_GetSnapshot(ProductDipSwitchSnapshot_t *snapshot)
{
    if (snapshot == NULL)
    {
        return false;
    }
    *snapshot = g_dipSwitchSnapshot;
    return true;
}

bool ProductRotarySwitchDriver_GetSnapshot(
    ProductRotarySwitchSnapshot_t *snapshot)
{
    if (snapshot == NULL)
    {
        return false;
    }
    *snapshot = g_rotarySwitchSnapshot;
    return true;
}

static HalGpioStatus_t MockGpioInitialize(void *context)
{
    return (context != NULL) ? HAL_GPIO_STATUS_OK :
                               HAL_GPIO_STATUS_INVALID_ARGUMENT;
}

static HalGpioStatus_t MockGpioRead(void *context, bool *active)
{
    MockGpioDriver_t *driver = (MockGpioDriver_t *)context;
    if ((driver == NULL) || (active == NULL))
    {
        return HAL_GPIO_STATUS_INVALID_ARGUMENT;
    }
    if (driver->failNextAccess)
    {
        driver->failNextAccess = false;
        return HAL_GPIO_STATUS_IO_ERROR;
    }
    *active = driver->state;
    return HAL_GPIO_STATUS_OK;
}

static HalGpioStatus_t MockGpioWrite(void *context, bool active)
{
    MockGpioDriver_t *driver = (MockGpioDriver_t *)context;
    if (driver == NULL)
    {
        return HAL_GPIO_STATUS_INVALID_ARGUMENT;
    }
    if (driver->failNextAccess)
    {
        driver->failNextAccess = false;
        return HAL_GPIO_STATUS_IO_ERROR;
    }
    driver->state = active;
    return HAL_GPIO_STATUS_OK;
}

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
    uint16_t values[15];
    const uint16_t invalidClear[2] =
        {FAULT_CODE_NVM_ERASE_FAILED, 0x0000U};
    const uint16_t validClear[2] =
        {FAULT_CODE_NVM_ERASE_FAILED,
         PRODUCT_DIAGNOSTICS_CLEAR_KEY_VALUE};

    FaultService_Initialize();
    ProductModbusRegisterAdapter_GetInterface(&interface);
    ProductModbusRegisterAdapter_SetSystemTimestamp(1234U);
    assert(FaultService_Raise(FAULT_CODE_NVM_ERASE_FAILED,
                              3U, 7U, 0x12345678UL));
    assert(interface.read_holding_registers(
               interface.context,
               PRODUCT_MODBUS_DIAGNOSTICS_BASE_ADDRESS,
               15U, values) == MODBUS_EXCEPTION_NONE);
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
    assert(values[11] == PRODUCT_DIAGNOSTICS_RESET_RESULT_READY);

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
    assert(interface.read_holding_registers(
               interface.context,
               PRODUCT_MODBUS_DIAGNOSTICS_RESET_RESULT_ADDRESS,
               4U, &values[11]) == MODBUS_EXCEPTION_NONE);
    assert(values[11] == PRODUCT_DIAGNOSTICS_RESET_RESULT_INVALID_KEY);
    assert(values[12] == FAULT_CODE_NVM_ERASE_FAILED);
    assert(values[13] == 0U);
    assert(values[14] == 1234U);
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
    assert(interface.read_holding_registers(
               interface.context,
               PRODUCT_MODBUS_DIAGNOSTICS_RESET_RESULT_ADDRESS,
               2U, &values[11]) == MODBUS_EXCEPTION_NONE);
    assert(values[11] == PRODUCT_DIAGNOSTICS_RESET_RESULT_SUCCESS);
    assert(values[12] == FAULT_CODE_NVM_ERASE_FAILED);
}

static void TestAdcDiagnosticRegisters(void)
{
    ModbusSlaveRegisterInterface_t interface;
    uint16_t values[64U];
    uint16_t boundary[2U];

    (void)memset(g_adcDiagnostics, 0, sizeof(g_adcDiagnostics));
    g_adcDiagnostics[0].initialized = true;
    g_adcDiagnostics[0].device_id = 0x04U;
    g_adcDiagnostics[0].last_status_register = 0x42U;
    g_adcDiagnostics[0].active_fault_categories =
        PRODUCT_ADC_FAULT_REFERENCE | PRODUCT_ADC_FAULT_CONVERSION;
    g_adcDiagnostics[0].last_driver_status = -4;
    g_adcDiagnostics[0].initial_error_register = 0x000123UL;
    g_adcDiagnostics[0].last_error_register = 0x020800UL;
    g_adcDiagnostics[0].latched_error_register = 0x028804UL;
    g_adcDiagnostics[0].successful_samples = 0x00012345UL;
    g_adcDiagnostics[0].reference_faults = 7U;
    g_adcDiagnostics[0].conversion_faults = 9U;
    g_adcDiagnostics[0].last_raw_code = 0x00ABCDEFUL;
    g_adcDiagnostics[0].last_channel = 3U;
    g_adcDiagnostics[0].last_microvolts = -12345;
    g_adcDiagnostics[0].consecutive_transaction_errors = 2U;
    g_adcDiagnostics[0].consecutive_clean_samples = 6U;
    g_adcDiagnostics[0].configure_attempts = 0x00010002UL;
    g_adcDiagnostics[0].configured_io_control1 = 0x000000F0UL;
    g_adcDiagnostics[0].configured_channel0 = 0x00008061UL;
    g_adcDiagnostics[0].configured_config0 = 0x00000875UL;
    g_adcDiagnostics[0].configured_filter0 = 0x00100180UL;
    g_adcDiagnostics[0].configuration_registers_valid = true;
    g_adcDiagnostics[1].initialized = true;
    g_adcDiagnostics[1].device_id = 0x06U;

    ProductModbusRegisterAdapter_GetInterface(&interface);
    assert(interface.read_holding_registers(
               interface.context,
               PRODUCT_MODBUS_ADC0_DIAGNOSTICS_BASE_ADDRESS,
               PRODUCT_MODBUS_ADC_DIAGNOSTICS_REGISTER_COUNT,
               values) == MODBUS_EXCEPTION_NONE);
    assert(values[0] == 1U);
    assert(values[1] == 0x04U);
    assert(values[2] == 0x42U);
    assert(values[3] == (PRODUCT_ADC_FAULT_REFERENCE |
                         PRODUCT_ADC_FAULT_CONVERSION));
    assert(values[5] == 0xFFFFU);
    assert(values[6] == 0xFFFCU);
    assert(values[7] == 0x0000U);
    assert(values[8] == 0x0123U);
    assert(values[11] == 0x0002U);
    assert(values[12] == 0x0800U);
    assert(values[13] == 0x0002U);
    assert(values[14] == 0x8804U);
    assert(values[17] == 0x0001U);
    assert(values[18] == 0x2345U);
    assert(values[37] == 0U);
    assert(values[38] == 7U);
    assert(values[39] == 0U);
    assert(values[40] == 9U);
    assert(values[47] == 0x00ABU);
    assert(values[48] == 0xCDEFU);
    assert(values[49] == 3U);
    assert(values[50] == 0x0206U);
    assert(values[51] == 0xFFFFU);
    assert(values[52] == 0xCFC7U);
    assert(values[53] == 0x0001U);
    assert(values[54] == 0x0002U);
    assert(values[55] == 0x0000U);
    assert(values[56] == 0x00F0U);
    assert(values[57] == 0x0000U);
    assert(values[58] == 0x8061U);
    assert(values[59] == 0x0000U);
    assert(values[60] == 0x0875U);
    assert(values[61] == 0x0010U);
    assert(values[62] == 0x0180U);
    assert(values[63] == 1U);

    assert(interface.read_holding_registers(
               interface.context,
               (uint16_t)(PRODUCT_MODBUS_ADC1_DIAGNOSTICS_BASE_ADDRESS - 1U),
               2U, boundary) == MODBUS_EXCEPTION_NONE);
    assert(boundary[0] == 1U);
    assert(boundary[1] == 1U);
    assert(interface.write_single_register(
               interface.context,
               PRODUCT_MODBUS_ADC0_DIAGNOSTICS_BASE_ADDRESS,
               0U) == MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS);
}

static void TestAdcConversionRegisters(void)
{
    ModbusSlaveRegisterInterface_t interface;
    uint16_t values[28U];
    uint16_t boundary[2U];
    HalAdcConversionDiagnostics_t *d = &g_adcDiagnostics[0].conversion_diagnostics;
    d->result = HAL_ADC_CONVERSION_OUT_OF_RANGE;
    d->raw_code = 0x8955DDUL;
    d->reference_uv = 2500000UL;
    d->gain = 32U;
    d->bipolar = true;
    d->output_valid = true;
    d->numerator = -0x123456789LL;
    d->denominator = 268435456LL;
    d->quotient = -78125LL;
    d->minimum = -2147483648LL;
    d->maximum = 2147483647LL;
    ProductModbusRegisterAdapter_GetInterface(&interface);
    assert(interface.read_holding_registers(interface.context,
        PRODUCT_MODBUS_ADC_CONVERSION_BASE_ADDRESS, 28U, values) == MODBUS_EXCEPTION_NONE);
    assert(values[0] == HAL_ADC_CONVERSION_OUT_OF_RANGE);
    assert(values[1] == 0x89U && values[2] == 0x55DDU);
    assert(values[3] == 0x26U && values[4] == 0x25A0U);
    assert(values[5] == 32U && values[6] == 1U && values[7] == 1U);
    assert(values[8] == 0xFFFFU && values[9] == 0xFFFEU);
    assert(values[10] == 0xDCBAU && values[11] == 0x9877U);
    assert(values[12] == 0U && values[13] == 0U);
    assert(values[14] == 0x1000U && values[15] == 0U);
    assert(values[20] == 0xFFFFU && values[21] == 0xFFFFU);
    assert(values[22] == 0x8000U && values[23] == 0U);
    assert(values[24] == 0U && values[25] == 0U);
    assert(values[26] == 0x7FFFU && values[27] == 0xFFFFU);
    assert(interface.read_holding_registers(interface.context,
        PRODUCT_MODBUS_ADC_CONVERSION_BASE_ADDRESS + 63U, 2U, boundary) == MODBUS_EXCEPTION_NONE);
    assert(boundary[0] == 0U && boundary[1] == 0U);
    assert(interface.write_single_register(interface.context,
        PRODUCT_MODBUS_ADC_CONVERSION_BASE_ADDRESS, 0U) == MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS);
    assert(interface.read_holding_registers(interface.context,
        PRODUCT_MODBUS_ADC_CONVERSION_LAST_ADDRESS, 2U, boundary) == MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS);
}

static void TestAdcTraceRegisters(void)
{
    ModbusSlaveRegisterInterface_t interface;
    uint16_t values[52U];
    uint16_t boundary[2U];
    g_adcDiagnostics[0].last_configure_source = PRODUCT_ADC_CONFIG_SOURCE_RECOVERY;
    g_adcDiagnostics[0].last_configure_stage = PRODUCT_ADC_CONFIG_STAGE_READBACK;
    g_adcDiagnostics[0].last_configure_result = HAL_ADC_STATUS_IO_ERROR;
    g_adcDiagnostics[0].configure_source_counts[3] = 0x10002UL;
    g_adcDiagnostics[0].first_sample_discards = 30UL;
    g_adcDiagnostics[0].fault_sample_discards = 2UL;
    g_configDiagnostics[0].event_id = 0xABCDEUL;
    g_configDiagnostics[0].revision = 7U;
    g_configDiagnostics[0].ack_failures = 10UL;
    g_configDiagnostics[0].stage = 4U;
    g_adcDiagnostics[0].last_read_stage = PRODUCT_ADC_READ_STAGE_CONVERT;
    g_adcDiagnostics[0].last_read_result = HAL_ADC_STATUS_DEVICE_ERROR;
    g_adcDiagnostics[0].last_read_failure_stage = PRODUCT_ADC_READ_STAGE_CONVERT;
    g_adcDiagnostics[0].last_read_failure_result = HAL_ADC_STATUS_DEVICE_ERROR;
    g_adcDiagnostics[0].failure_reference_uv = 2500000UL;
    g_adcDiagnostics[0].failure_raw_code = 0x9C0DD9UL;
    g_adcDiagnostics[0].read_failures = 357UL;
    g_adcDiagnostics[0].failure_config_valid = true;
    g_adcDiagnostics[0].failure_driver_status = -3;
    ProductModbusRegisterAdapter_GetInterface(&interface);
    assert(interface.read_holding_registers(interface.context,
        PRODUCT_MODBUS_ADC_TRACE_BASE_ADDRESS, 52U, values) == MODBUS_EXCEPTION_NONE);
    assert(values[0] == 3U && values[1] == 5U);
    assert(values[2] == HAL_ADC_STATUS_IO_ERROR);
    assert(values[8] == 1U && values[9] == 2U);
    assert(values[12] == 0xAU && values[13] == 0xBCDEU);
    assert(values[14] == 7U);
    assert(values[23] == 30U && values[25] == 2U);
    assert(values[26] == 1U);
    assert(values[28] == 0x1234U && values[29] == 0x5678U);
    assert(values[33] == 10U && values[34] == 4U);
    assert(values[35] == PRODUCT_ADC_READ_STAGE_CONVERT);
    assert(values[36] == HAL_ADC_STATUS_DEVICE_ERROR);
    assert(values[37] == PRODUCT_ADC_READ_STAGE_CONVERT);
    assert(values[38] == HAL_ADC_STATUS_DEVICE_ERROR);
    assert(values[40] == 0x26U && values[41] == 0x25A0U);
    assert(values[45] == 0x9CU && values[46] == 0x0DD9U);
    assert(values[48] == 357U && values[49] == 1U);
    assert(values[50] == 0xFFFFU && values[51] == 0xFFFDU);
    assert(interface.read_holding_registers(interface.context,
        PRODUCT_MODBUS_ADC_TRACE_BASE_ADDRESS + 63U, 2U, boundary) == MODBUS_EXCEPTION_NONE);
    assert(boundary[0] == 0U && boundary[1] == 0U);
    assert(interface.write_single_register(interface.context,
        PRODUCT_MODBUS_ADC_TRACE_BASE_ADDRESS, 0U) == MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS);
    assert(interface.read_holding_registers(interface.context,
        PRODUCT_MODBUS_ADC_TRACE_LAST_ADDRESS, 2U, boundary) == MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS);
}

static void TestSystemRoutineDiagnosticRegisters(void)
{
    ModbusSlaveRegisterInterface_t interface;
    uint16_t status[16U];
    uint16_t event[12U];
    uint16_t snapshot[27U];
    int32_t values[SNAPSHOT_SERVICE_VALUE_COUNT] = {1234, 0};

    SnapshotService_Initialize();
    SystemEventService_Initialize();
    WarningService_Initialize();
    assert(SafetyService_Initialize(
        SAFETY_SOURCE_LOW_VOLTAGE, NULL, NULL));
    assert(WarningService_UpdateSource(
        WARNING_SOURCE_ADC_REFERENCE, true, 0x12U,
        100U, 3U, 7U, values));
    assert(SafetyService_UpdateSource(
        SAFETY_SOURCE_LOW_VOLTAGE, true, 1U,
        101U, 3U, 8U, values));
    assert(SafetyService_Process());

    ProductModbusRegisterAdapter_GetInterface(&interface);
    assert(interface.read_holding_registers(
               interface.context, PRODUCT_MODBUS_SYSTEM_STATUS_BASE_ADDRESS,
               16U, status) == MODBUS_EXCEPTION_NONE);
    assert(status[0] == 0U);
    assert(status[1] == WARNING_SOURCE_ADC_REFERENCE);
    assert(status[3] == SAFETY_SOURCE_LOW_VOLTAGE);
    assert(status[5] == SAFETY_SOURCE_LOW_VOLTAGE);
    assert(status[7] == SAFETY_SOURCE_LOW_VOLTAGE);
    assert(status[8] == 2U);
    assert(status[9] == 2U);
    assert(status[14] == SAFETY_STATE_TRIPPED);
    assert(status[15] == 1U);

    assert(interface.read_holding_registers(
               interface.context, PRODUCT_MODBUS_SYSTEM_EVENT_BASE_ADDRESS,
               12U, event) == MODBUS_EXCEPTION_NONE);
    assert(event[0] == 0U);
    assert(event[5] == SYSTEM_EVENT_DOMAIN_WARNING);
    assert(event[7] == WARNING_SOURCE_ADC_REFERENCE);
    assert(interface.write_single_register(
               interface.context, PRODUCT_MODBUS_SYSTEM_EVENT_INDEX_ADDRESS,
               1U) == MODBUS_EXCEPTION_NONE);
    assert(interface.read_holding_registers(
               interface.context, PRODUCT_MODBUS_SYSTEM_EVENT_BASE_ADDRESS,
               12U, event) == MODBUS_EXCEPTION_NONE);
    assert(event[5] == SYSTEM_EVENT_DOMAIN_SAFETY);

    assert(interface.read_holding_registers(
               interface.context, PRODUCT_MODBUS_SNAPSHOT_BASE_ADDRESS,
               27U, snapshot) == MODBUS_EXCEPTION_NONE);
    assert(snapshot[0] == 0U);
    assert(snapshot[5] == SNAPSHOT_SOURCE_WARNING);
    assert(snapshot[11] == 0U);
    assert(snapshot[12] == 1234U);
    assert(interface.write_single_register(
               interface.context, PRODUCT_MODBUS_SNAPSHOT_INDEX_ADDRESS,
               2U) == MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE);
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

static void TestDigitalIoRegisters(void)
{
    ModbusSlaveRegisterInterface_t interface;
    uint16_t diImage[2];
    uint16_t doImage[6];
    const uint16_t doMaskAndApply[2] =
        {0x000DU, PRODUCT_MODBUS_DO_APPLY_KEY_VALUE};

    ProductModbusRegisterAdapter_GetInterface(&interface);
    g_digitalInput[0].state = true;
    g_digitalInput[2].state = true;
    assert(DigitalInputService_Process() == DIGITAL_INPUT_STATUS_OK);
    assert(interface.read_holding_registers(
               interface.context, PRODUCT_MODBUS_DI_BASE_ADDRESS,
               2U, diImage) == MODBUS_EXCEPTION_NONE);
    assert(diImage[0] == 0x0005U);
    assert(diImage[1] == 1U);
    assert(interface.write_single_register(
               interface.context, PRODUCT_MODBUS_DI_BASE_ADDRESS,
               1U) == MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS);

    assert(interface.read_holding_registers(
               interface.context, PRODUCT_MODBUS_DO_BASE_ADDRESS,
               6U, doImage) == MODBUS_EXCEPTION_NONE);
    assert(doImage[0] == 0U);
    assert(doImage[2] == 0U);
    assert(doImage[3] == 0U);

    assert(interface.write_single_register(
               interface.context, PRODUCT_MODBUS_DO_BASE_ADDRESS,
               0x0001U) == MODBUS_EXCEPTION_NONE);
    assert(!g_digitalOutput[0].state);
    assert(interface.read_holding_registers(
               interface.context, PRODUCT_MODBUS_DO_BASE_ADDRESS,
               6U, doImage) == MODBUS_EXCEPTION_NONE);
    assert(doImage[0] == 0U);
    assert(doImage[3] == PRODUCT_MODBUS_DO_VALID_MASK);
    assert(interface.write_single_register(
               interface.context, PRODUCT_MODBUS_DO_APPLY_KEY_ADDRESS,
               0x5A5AU) == MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE);
    assert(interface.write_single_register(
               interface.context, PRODUCT_MODBUS_DO_APPLY_KEY_ADDRESS,
               PRODUCT_MODBUS_DO_APPLY_KEY_VALUE) == MODBUS_EXCEPTION_NONE);
    assert(g_digitalOutput[0].state);

    assert(interface.write_multiple_registers(
               interface.context, PRODUCT_MODBUS_DO_BASE_ADDRESS,
               doMaskAndApply, 2U) == MODBUS_EXCEPTION_NONE);
    assert(g_digitalOutput[0].state);
    assert(!g_digitalOutput[1].state);
    assert(g_digitalOutput[2].state);
    assert(g_digitalOutput[3].state);
    assert(interface.read_holding_registers(
               interface.context, PRODUCT_MODBUS_DO_BASE_ADDRESS,
               6U, doImage) == MODBUS_EXCEPTION_NONE);
    assert(doImage[2] == 2U);
    assert(doImage[0] == 0x000DU);
    assert(doImage[4] == PRODUCT_MODBUS_DO_STATUS_READY);

    assert(interface.write_single_register(
               interface.context, PRODUCT_MODBUS_DO_BASE_ADDRESS,
               0x0010U) == MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE);
    assert(interface.write_single_register(
               interface.context, PRODUCT_MODBUS_DO_BASE_ADDRESS,
               0x0002U) == MODBUS_EXCEPTION_NONE);
    g_digitalOutput[1].failNextAccess = true;
    assert(interface.write_single_register(
               interface.context, PRODUCT_MODBUS_DO_APPLY_KEY_ADDRESS,
               PRODUCT_MODBUS_DO_APPLY_KEY_VALUE) ==
           MODBUS_EXCEPTION_SERVER_DEVICE_FAILURE);
    assert(g_digitalOutput[0].state);
    assert(!g_digitalOutput[1].state);
    assert(interface.read_holding_registers(
               interface.context, PRODUCT_MODBUS_DO_BASE_ADDRESS,
               6U, doImage) == MODBUS_EXCEPTION_NONE);
    assert(doImage[2] == 2U);
    assert(doImage[3] == PRODUCT_MODBUS_DO_VALID_MASK);
    assert(doImage[0] == 0x000DU);
    assert(doImage[4] == PRODUCT_MODBUS_DO_STATUS_APPLY_FAILED);
    assert(doImage[5] == 1U);

    assert(interface.write_single_register(
               interface.context, PRODUCT_MODBUS_DO_APPLY_KEY_ADDRESS,
               PRODUCT_MODBUS_DO_APPLY_KEY_VALUE) == MODBUS_EXCEPTION_NONE);
    assert(!g_digitalOutput[0].state);
    assert(g_digitalOutput[1].state);
}

static void TestDipSwitchRegisters(void)
{
    ModbusSlaveRegisterInterface_t interface;
    uint16_t image[4];

    ProductModbusRegisterAdapter_GetInterface(&interface);
    g_dipSwitchSnapshot.logical_mask = 0xA5U;
    g_dipSwitchSnapshot.raw_value = 0x5AU;
    g_dipSwitchSnapshot.revision = 7U;
    g_dipSwitchSnapshot.status = PRODUCT_DIP_SWITCH_STATUS_READY;

    assert(interface.read_holding_registers(
               interface.context, PRODUCT_MODBUS_DIP_SWITCH_BASE_ADDRESS,
               4U, image) == MODBUS_EXCEPTION_NONE);
    assert(image[0] == 0x00A5U);
    assert(image[1] == 0x005AU);
    assert(image[2] == 7U);
    assert(image[3] == PRODUCT_MODBUS_DIP_SWITCH_STATUS_READY);

    assert(interface.write_single_register(
               interface.context,
               PRODUCT_MODBUS_DIP_SWITCH_BASE_ADDRESS,
               1U) == MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS);
}

static void TestRotarySwitchRegisters(void)
{
    ModbusSlaveRegisterInterface_t interface;
    uint16_t image[4];

    ProductModbusRegisterAdapter_GetInterface(&interface);
    g_rotarySwitchSnapshot.position = 9U;
    g_rotarySwitchSnapshot.raw_value = 0x06U;
    g_rotarySwitchSnapshot.revision = 3U;
    g_rotarySwitchSnapshot.status = PRODUCT_ROTARY_SWITCH_STATUS_READY;

    assert(interface.read_holding_registers(
               interface.context, PRODUCT_MODBUS_ROTARY_SWITCH_BASE_ADDRESS,
               4U, image) == MODBUS_EXCEPTION_NONE);
    assert(image[0] == 9U);
    assert(image[1] == 0x06U);
    assert(image[2] == 3U);
    assert(image[3] == PRODUCT_MODBUS_ROTARY_SWITCH_STATUS_READY);

    assert(interface.write_single_register(
               interface.context,
               PRODUCT_MODBUS_ROTARY_SWITCH_POSITION_ADDRESS,
               1U) == MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS);
}

static void TestLowVoltageRegisters(void)
{
    ModbusSlaveRegisterInterface_t interface;
    product_low_voltage_monitor_t monitor =
        {0x12345678UL, 9U, PRODUCT_MODBUS_LOW_VOLTAGE_STATUS_READY,
         true, true, true};
    uint16_t image[7];

    ProductModbusRegisterAdapter_GetInterface(&interface);
    ProductModbusRegisterAdapter_SetLowVoltageMonitor(&monitor);
    assert(interface.read_input_registers(
               interface.context, PRODUCT_MODBUS_LOW_VOLTAGE_BASE_ADDRESS,
               7U, image) == MODBUS_EXCEPTION_NONE);
    assert(image[0] == 1U);
    assert(image[1] == 1U);
    assert(image[2] == 1U);
    assert(image[3] == 9U);
    assert(image[4] == 0x1234U);
    assert(image[5] == 0x5678U);
    assert(image[6] == PRODUCT_MODBUS_LOW_VOLTAGE_STATUS_READY);
    assert(interface.write_single_register(
               interface.context,
               PRODUCT_MODBUS_LOW_VOLTAGE_BASE_ADDRESS,
               0U) == MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS);
}

int main(void)
{
    static const HalPwmDriverOps_t pwmOps =
        {MockPwmInitialize, MockPwmSetDuty, MockPwmSetPeriod};
    static const HalDacDriverOps_t dacOps =
        {MockDacInitialize, MockDacWriteCode};
    static const HalGpioInputDriverOps_t gpioInputOps =
        {MockGpioInitialize, MockGpioRead};
    static const HalGpioOutputDriverOps_t gpioOutputOps =
        {MockGpioInitialize, MockGpioWrite};
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
    for (channel = 0U; channel < PRODUCT_MODBUS_DI_CHANNEL_COUNT; channel++)
    {
        assert(HalGpio_RegisterInputDriver(
                   channel, &gpioInputOps, &g_digitalInput[channel]) ==
               HAL_GPIO_STATUS_OK);
        assert(HalGpio_RegisterOutputDriver(
                   channel, &gpioOutputOps, &g_digitalOutput[channel]) ==
               HAL_GPIO_STATUS_OK);
    }
    assert(DigitalInputService_Initialize(PRODUCT_MODBUS_DI_CHANNEL_COUNT) ==
           DIGITAL_INPUT_STATUS_OK);
    assert(DigitalOutputService_Initialize(PRODUCT_MODBUS_DO_CHANNEL_COUNT) ==
           DIGITAL_OUTPUT_STATUS_OK);
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
    TestAdcDiagnosticRegisters();
    TestAdcConversionRegisters();
    TestAdcTraceRegisters();
    TestSystemRoutineDiagnosticRegisters();
    TestProductVersionRegisters();
    TestPwmPendingAndApply();
    TestDacPendingApplyAndRollback();
    TestDigitalIoRegisters();
    TestDipSwitchRegisters();
    TestRotarySwitchRegisters();
    TestLowVoltageRegisters();
    return 0;
}
