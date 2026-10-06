#include "product_application.h"

#include <stddef.h>
#include <string.h>

#include "AlarmConfigurationEventConsumer.h"
#include "AnalogInputService.h"
#include "EventService.h"
#include "FaultService.h"
#include "FactoryCalibrationService.h"
#include "FactoryModeService.h"
#include "NvmConfigurationEventConsumer.h"
#include "NvmService.h"
#include "PwmOutputService.h"
#include "DigitalInputService.h"
#include "DigitalOutputService.h"
#include "SafetyConfigurationEventConsumer.h"
#include "SafetyService.h"
#include "SnapshotService.h"
#include "SystemRoutine.h"
#include "WarningService.h"
#include "product_temperature_range_resolver.h"
#include "product_modbus_register_adapter.h"
#include "product_adc_driver.h"
#include "product_dip_switch_driver.h"
#include "product_rotary_switch_driver.h"
#include "bsp_internal_adc.h"
#include "ProductAdcConfig.h"
#include "ProductDipSwitchConfig.h"
#include "ProductInternalAdcConfig.h"
#include "ProductLowVoltageConfig.h"
#include "ProductRotarySwitchConfig.h"
#include "ProductSafetyConfig.h"
#include "product_low_voltage_safety.h"
#include "product_mcu_temperature_safety.h"
#include "product_fram_bank_test.h"
#include "product_sensor_configuration_consumer.h"
#include "product_sensor_measurement_service.h"
#include "product_status_led.h"
#include "product_temperature_input_types.h"

static bool g_last_pwm_inhibited[4U] = {true, true, true, true};
static uint16_t g_adc_poll_elapsed_ms;
static uint16_t g_adc_recovery_elapsed_ms;
static uint16_t g_adc_diagnostic_elapsed_ms;
static uint16_t g_cjc_elapsed_ms;
static uint16_t g_mcu_temperature_elapsed_ms;
static bool g_adc_poll_due;
static bool g_adc_recovery_due;
static bool g_adc_diagnostic_due;
static bool g_cjc_due;
static bool g_mcu_temperature_due;
static bool g_digital_input_due;
static uint16_t g_dip_switch_elapsed_ms;
static bool g_dip_switch_due;
static uint16_t g_rotary_switch_elapsed_ms;
static bool g_rotary_switch_due;
static uint16_t g_low_voltage_elapsed_ms;
static bool g_low_voltage_due;
static uint32_t g_adc_last_successful_samples[HAL_ADC_DEVICE_COUNT];
static uint16_t g_adc_stale_elapsed_ms[HAL_ADC_DEVICE_COUNT];
static uint8_t g_adc_last_fault_device_mask[7U];
static uint32_t g_adc_fault_event_id;
static uint32_t g_system_timestamp_ms;

static const uint32_t g_adc_warning_sources[7U] =
{
    WARNING_SOURCE_ADC_COMMUNICATION,
    WARNING_SOURCE_ADC_INTEGRITY,
    WARNING_SOURCE_ADC_REFERENCE,
    WARNING_SOURCE_ADC_CONVERSION,
    WARNING_SOURCE_ADC_INPUT_VOLTAGE,
    WARNING_SOURCE_ADC_INTERNAL,
    WARNING_SOURCE_ADC_STALE
};

static bool ApplySystemSafetyOutput(bool inhibit,
                                    uint32_t source_mask,
                                    void *context)
{
    bool result = true;

    (void)context;
    result = SafetyConfigurationEventConsumer_UpdateGlobalOutputInhibit(
        PRODUCT_SAFETY_INHIBIT_MCU_OVERTEMPERATURE,
        inhibit && ((source_mask & SAFETY_SOURCE_MCU_OVERTEMPERATURE) != 0U)) &&
        result;
    result = SafetyConfigurationEventConsumer_UpdateGlobalOutputInhibit(
        PRODUCT_SAFETY_INHIBIT_LOW_VOLTAGE,
        inhibit && ((source_mask & SAFETY_SOURCE_LOW_VOLTAGE) != 0U)) &&
        result;
    return result;
}

static bool UpdateSystemSafetySource(
    uint32_t source_mask,
    bool active,
    uint16_t detail,
    const int32_t values[SNAPSHOT_SERVICE_VALUE_COUNT])
{
    bool result = SafetyService_UpdateSource(
        source_mask, active, detail, g_system_timestamp_ms,
        0U, 0U, values);

    if (!active)
    {
        SafetyResetResult_t reset_result = SafetyService_Reset(source_mask);

        result = ((reset_result == SAFETY_RESET_OK) ||
                  (reset_result == SAFETY_RESET_BLOCKED_ACTIVE)) && result;
    }
    return result;
}

static uint8_t FoldAdcErrorRegister(uint32_t error_register)
{
    return (uint8_t)(error_register ^ (error_register >> 8U) ^
                     (error_register >> 16U));
}

static void RaiseAdcCategoryFault(uint8_t category_index,
                                  FaultCode_t code,
                                  uint8_t device_mask,
                                  uint8_t subcause)
{
    if (device_mask == 0U)
    {
        g_adc_last_fault_device_mask[category_index] = 0U;
        return;
    }
    if ((device_mask != g_adc_last_fault_device_mask[category_index]) ||
        !FaultService_IsActive(code))
    {
        uint16_t detail = (uint16_t)(((uint16_t)subcause << 8U) |
                                     device_mask);
        g_adc_fault_event_id++;
        (void)FaultService_Raise(code, detail, 0U, g_adc_fault_event_id);
        g_adc_last_fault_device_mask[category_index] = device_mask;
    }
}

static bool UpdateAdcCategoryWarning(uint8_t category_index,
                                     uint8_t device_mask,
                                     uint8_t subcause)
{
    int32_t values[SNAPSHOT_SERVICE_VALUE_COUNT] = {0};
    uint8_t device;

    values[0] = (int32_t)device_mask;
    values[1] = (int32_t)subcause;
    values[6] = (int32_t)category_index;
    for (device = 0U; device < HAL_ADC_DEVICE_COUNT; device++)
    {
        ProductAdcDriverDiagnostics_t diagnostics;

        if (ProductAdcDriver_GetDiagnostics(device, &diagnostics))
        {
            values[2U + device] =
                (int32_t)(diagnostics.last_error_register & 0x00FFFFFFUL);
        }
    }

    return WarningService_UpdateSource(
        g_adc_warning_sources[category_index],
        device_mask != 0U,
        (uint16_t)(((uint16_t)subcause << 8U) | device_mask),
        g_system_timestamp_ms, 0U, g_adc_fault_event_id, values);
}

static bool ProcessAdcFaults(void)
{
    static const uint16_t categories[6U] =
    {
        PRODUCT_ADC_FAULT_COMMUNICATION,
        PRODUCT_ADC_FAULT_INTEGRITY,
        PRODUCT_ADC_FAULT_REFERENCE,
        PRODUCT_ADC_FAULT_CONVERSION,
        PRODUCT_ADC_FAULT_INPUT_VOLTAGE,
        PRODUCT_ADC_FAULT_INTERNAL
    };
    static const FaultCode_t codes[6U] =
    {
        FAULT_CODE_ADC_COMMUNICATION,
        FAULT_CODE_ADC_INTEGRITY,
        FAULT_CODE_ADC_REFERENCE,
        FAULT_CODE_ADC_CONVERSION,
        FAULT_CODE_ADC_INPUT_VOLTAGE,
        FAULT_CODE_ADC_INTERNAL
    };
    uint8_t device_masks[6U] = {0U};
    uint8_t subcauses[6U] = {0U};
    uint8_t stale_mask = 0U;
    uint8_t device;
    uint8_t category;
    bool result = true;

    for (device = 0U; device < HAL_ADC_DEVICE_COUNT; device++)
    {
        ProductAdcDriverDiagnostics_t driver_diagnostics;
        AnalogInputDiagnostics_t input_diagnostics;
        if (!ProductAdcDriver_GetDiagnostics(device, &driver_diagnostics) ||
            !AnalogInputService_GetDiagnostics(device, &input_diagnostics))
        {
            continue;
        }
        for (category = 0U; category < 6U; category++)
        {
            if ((driver_diagnostics.active_fault_categories &
                 categories[category]) != 0U)
            {
                device_masks[category] |= (uint8_t)(1U << device);
                subcauses[category] |= FoldAdcErrorRegister(
                    driver_diagnostics.last_error_register);
                if ((subcauses[category] == 0U) &&
                    (driver_diagnostics.last_driver_status < 0))
                {
                    subcauses[category] |= (uint8_t)
                        (-driver_diagnostics.last_driver_status);
                }
            }
        }
        if (!input_diagnostics.online ||
            (g_adc_stale_elapsed_ms[device] >= 500U))
        {
            stale_mask |= (uint8_t)(1U << device);
        }
    }
    for (category = 0U; category < 6U; category++)
    {
        RaiseAdcCategoryFault(category, codes[category],
                              device_masks[category], subcauses[category]);
        result = UpdateAdcCategoryWarning(
            category, device_masks[category], subcauses[category]) && result;
    }
    RaiseAdcCategoryFault(6U, FAULT_CODE_ADC_STALE_OFFLINE,
                          stale_mask, 0U);
    result = UpdateAdcCategoryWarning(6U, stale_mask, 0U) && result;
    return result;
}

static bool ProcessMcuTemperatureSafety(void)
{
    BspInternalAdcMcuTemperature_t temperature;
    int32_t values[SNAPSHOT_SERVICE_VALUE_COUNT] = {0};
    bool active;

    if (!BspInternalAdc_GetMcuTemperature(&temperature))
    {
        return true;
    }
    ProductMcuTemperatureSafety_Process(
        temperature.valid,
        temperature.overtemperature,
        temperature.temperature_centi_c);
    if (!temperature.valid)
    {
        return true;
    }

    values[0] = temperature.temperature_centi_c;
    active = temperature.overtemperature ||
        FaultService_IsActive(FAULT_CODE_MCU_OVERTEMPERATURE);
    return UpdateSystemSafetySource(
        SAFETY_SOURCE_MCU_OVERTEMPERATURE,
        active,
        (temperature.temperature_centi_c <= 0) ? 0U :
        ((temperature.temperature_centi_c >= (int32_t)UINT16_MAX) ?
         UINT16_MAX : (uint16_t)temperature.temperature_centi_c),
        values);
}

static bool ProcessLowVoltageSafety(void)
{
    ProductLowVoltageSnapshot_t snapshot;
    int32_t values[SNAPSHOT_SERVICE_VALUE_COUNT] = {0};

    ProductLowVoltageSafety_Process();
    if (!ProductLowVoltageSafety_GetSnapshot(&snapshot))
    {
        return false;
    }

    values[0] = snapshot.raw_active ? 1L : 0L;
    values[1] = (int32_t)snapshot.interrupt_count;
    values[2] = (int32_t)snapshot.revision;
    return UpdateSystemSafetySource(
        SAFETY_SOURCE_LOW_VOLTAGE,
        snapshot.confirmed_active || snapshot.fault_active,
        snapshot.raw_active ? 1U : 0U,
        values);
}

static bool ProductSystemFastMonitor(uint32_t timestamp_ms, void *context)
{
    bool result = true;

    (void)timestamp_ms;
    (void)context;
    if (g_low_voltage_due)
    {
        g_low_voltage_due = false;
        result = ProcessLowVoltageSafety() && result;
    }
    result = ProcessMcuTemperatureSafety() && result;
    return result;
}

static bool ProductSystemControlMonitor(uint32_t timestamp_ms, void *context)
{
    (void)timestamp_ms;
    (void)context;
    return ProcessAdcFaults();
}

static bool ConfigureAdcWarnings(void)
{
    const WarningSourceConfiguration_t configuration = {1U, 1U, false};
    uint8_t category;

    for (category = 0U; category < 7U; category++)
    {
        if (!WarningService_ConfigureSource(
                g_adc_warning_sources[category], &configuration))
        {
            return false;
        }
    }
    return true;
}

static bool IsPwmOutputInhibited(uint8_t channel, void *context)
{
    (void)context;
    return SafetyConfigurationEventConsumer_IsOutputInhibited(channel);
}

bool ProductApplication_Init(void)
{
    AnalogInputStatus_t adc_status;
    SystemRoutineConfiguration_t system_routine_configuration;
    uint8_t adc_device;
    uint8_t adc_route_count;

    (void)memset(&system_routine_configuration, 0,
                 sizeof(system_routine_configuration));
    system_routine_configuration.safety_latching_source_mask =
        SAFETY_SOURCE_MCU_OVERTEMPERATURE |
        SAFETY_SOURCE_LOW_VOLTAGE;
    system_routine_configuration.safety_output_action =
        ApplySystemSafetyOutput;
    system_routine_configuration.fast_monitor =
        ProductSystemFastMonitor;
    system_routine_configuration.control_monitor =
        ProductSystemControlMonitor;
    if (!SystemRoutine_Initialize(&system_routine_configuration))
    {
        return false;
    }
    if (!ConfigureAdcWarnings())
    {
        return false;
    }
    FactoryModeService_Initialize();
    FactoryCalibrationService_Initialize();

    if (!ProductStatusLed_Initialize())
    {
        return false;
    }

    if ((DigitalInputService_Initialize(4U) != DIGITAL_INPUT_STATUS_OK) ||
        (DigitalOutputService_Initialize(4U) != DIGITAL_OUTPUT_STATUS_OK))
    {
        return false;
    }

    if (!ProductAdcDriver_Init())
    {
        return false;
    }
    if (!BspInternalAdc_Initialize())
    {
        return false;
    }
    for (adc_device = 0U; adc_device < HAL_ADC_DEVICE_COUNT; adc_device++)
    {
        if (AnalogInputService_SetDeviceConfiguration(
                adc_device,
                ProductAdcDriver_GetDeviceConfig(adc_device)) !=
            ANALOG_INPUT_STATUS_OK)
        {
            return false;
        }
    }
    if (AnalogInputService_SetRoutes(
            ProductAdcDriver_GetRoutes(&adc_route_count), adc_route_count) !=
        ANALOG_INPUT_STATUS_OK)
    {
        return false;
    }
    adc_status = AnalogInputService_Initialize(HAL_ADC_DEVICE_COUNT);
    if (adc_status == ANALOG_INPUT_STATUS_INVALID_ARGUMENT)
    {
        return false;
    }

    if (!EventService_ConfigureTemperatureInputRequiredAckMask(
            EVENT_ACK_ANALOG_INPUT | EVENT_ACK_ALARM |
            EVENT_ACK_SAFETY | EVENT_ACK_NVM))
    {
        return false;
    }

    if (!AlarmConfigurationEventConsumer_Initialize(
            ProductTemperatureRangeResolver_Resolve, NULL))
    {
        return false;
    }

    if (!SafetyConfigurationEventConsumer_Initialize(
            ProductTemperatureRangeResolver_Resolve, NULL))
    {
        return false;
    }
    ProductMcuTemperatureSafety_Initialize();
    ProductLowVoltageSafety_Initialize();
    ProductLowVoltageSafety_Process();
    ProductSensorConfigurationConsumer_Initialize();
    ProductSensorMeasurementService_Initialize();
    ProductDipSwitchDriver_Initialize();
    (void)ProductDipSwitchDriver_Process();
    ProductRotarySwitchDriver_Initialize();
    ProductRotarySwitchDriver_Process();

    if (!NvmConfigurationEventConsumer_Initialize())
    {
        return false;
    }
    ProductFramBankTest_Initialize();

    {
        uint8_t channel;
        for (channel = 0U;
             channel < PRODUCT_MODBUS_TEMPERATURE_INPUT_COUNT;
             channel++)
        {
            EventTemperatureInputConfiguration_t stored;
            product_temperature_input_config_t product_config;
            uint16_t stored_revision;
            if (!NvmService_GetLoadedTemperatureInputConfigurationForChannel(
                    channel, &stored_revision, &stored))
            {
                stored.filter_time_constant_seconds = 0.5F;
                stored.sensor_type = PRODUCT_SENSOR_TYPE_TC_K;
                stored_revision = 1U;
            }
            product_config.filterTimeConstantSeconds =
                stored.filter_time_constant_seconds;
            product_config.sensorType = stored.sensor_type;
            if (!ProductModbusRegisterAdapter_RestoreTemperatureInputConfigForChannel(
                        channel, &product_config, stored_revision) ||
                !EventService_RaiseTemperatureInputConfigurationChanged(
                    channel, stored_revision,
                    EVENT_TEMPERATURE_INPUT_CHANGE_ALL,
                    &stored, &stored, NULL))
            {
                return false;
            }
        }
    }

    if (PwmOutputService_Initialize(
            4U, IsPwmOutputInhibited, NULL) != PWM_OUTPUT_STATUS_OK)
    {
        return false;
    }
    (void)memset(g_last_pwm_inhibited, 1, sizeof(g_last_pwm_inhibited));
    g_adc_poll_elapsed_ms = 0U;
    g_adc_recovery_elapsed_ms = 0U;
    g_adc_diagnostic_elapsed_ms = 0U;
    g_cjc_elapsed_ms = 0U;
    g_mcu_temperature_elapsed_ms = 0U;
    g_adc_poll_due = false;
    g_adc_recovery_due = false;
    g_adc_diagnostic_due = false;
    g_cjc_due = false;
    g_mcu_temperature_due = false;
    g_digital_input_due = false;
    g_dip_switch_elapsed_ms = 0U;
    g_dip_switch_due = false;
    g_rotary_switch_elapsed_ms = 0U;
    g_rotary_switch_due = false;
    g_low_voltage_elapsed_ms = 0U;
    g_low_voltage_due = false;
    (void)memset(g_adc_last_successful_samples, 0,
                 sizeof(g_adc_last_successful_samples));
    (void)memset(g_adc_stale_elapsed_ms, 0,
                 sizeof(g_adc_stale_elapsed_ms));
    (void)memset(g_adc_last_fault_device_mask, 0,
                 sizeof(g_adc_last_fault_device_mask));
    g_adc_fault_event_id = 0U;
    g_system_timestamp_ms = 0U;
    (void)BspInternalAdc_RequestCjcSamples();
    (void)BspInternalAdc_RequestMcuTemperature();
    return true;
}

void ProductApplication_Tick1ms(void)
{
    g_system_timestamp_ms++;
    ProductStatusLed_Tick1ms();
    g_digital_input_due = true;

    if (++g_dip_switch_elapsed_ms >= PRODUCT_DIP_SWITCH_SAMPLE_PERIOD_MS)
    {
        g_dip_switch_elapsed_ms = 0U;
        g_dip_switch_due = true;
    }
    if (++g_low_voltage_elapsed_ms >= PRODUCT_LOW_VOLTAGE_SAMPLE_PERIOD_MS)
    {
        g_low_voltage_elapsed_ms = 0U;
        g_low_voltage_due = true;
    }
    if (++g_rotary_switch_elapsed_ms >=
        PRODUCT_ROTARY_SWITCH_SAMPLE_PERIOD_MS)
    {
        g_rotary_switch_elapsed_ms = 0U;
        g_rotary_switch_due = true;
    }

    if (++g_adc_poll_elapsed_ms >= PRODUCT_ADC_POLL_PERIOD_MS)
    {
        g_adc_poll_elapsed_ms = 0U;
        g_adc_poll_due = true;
    }
    if (++g_adc_recovery_elapsed_ms >= PRODUCT_ADC_RECOVERY_PERIOD_MS)
    {
        g_adc_recovery_elapsed_ms = 0U;
        g_adc_recovery_due = true;
    }
    if (++g_adc_diagnostic_elapsed_ms >= PRODUCT_ADC_DIAGNOSTIC_PERIOD_MS)
    {
        g_adc_diagnostic_elapsed_ms = 0U;
        g_adc_diagnostic_due = true;
    }
    if (++g_cjc_elapsed_ms >= PRODUCT_CJC_SAMPLE_PERIOD_MS)
    {
        g_cjc_elapsed_ms = 0U;
        g_cjc_due = true;
    }
    if (++g_mcu_temperature_elapsed_ms >=
        PRODUCT_MCU_TEMPERATURE_SAMPLE_PERIOD_MS)
    {
        g_mcu_temperature_elapsed_ms = 0U;
        g_mcu_temperature_due = true;
    }
}

void ProductApplication_Process(void)
{
    uint8_t input;

    ProductStatusLed_Process();
    if (g_digital_input_due)
    {
        g_digital_input_due = false;
        (void)DigitalInputService_Process();
    }
    if (g_dip_switch_due)
    {
        g_dip_switch_due = false;
        (void)ProductDipSwitchDriver_Process();
    }
    if (g_rotary_switch_due)
    {
        g_rotary_switch_due = false;
        ProductRotarySwitchDriver_Process();
    }
    BspInternalAdc_Process();
    (void)SystemRoutine_ExecuteFast(g_system_timestamp_ms);
    if (g_cjc_due)
    {
        g_cjc_due = false;
        (void)BspInternalAdc_RequestCjcSamples();
    }
    if (g_mcu_temperature_due)
    {
        g_mcu_temperature_due = false;
        (void)BspInternalAdc_RequestMcuTemperature();
    }

    if (g_adc_poll_due)
    {
        uint8_t device;
        g_adc_poll_due = false;
        /* g_next_device starts at zero; four calls produce ADC0,1,2,3. */
        for (device = 0U; device < HAL_ADC_DEVICE_COUNT; device++)
        {
            (void)AnalogInputService_Process();
        }
        for (device = 0U; device < HAL_ADC_DEVICE_COUNT; device++)
        {
            ProductAdcDriverDiagnostics_t diagnostics;
            if (ProductAdcDriver_GetDiagnostics(device, &diagnostics))
            {
                if (diagnostics.successful_samples !=
                    g_adc_last_successful_samples[device])
                {
                    g_adc_last_successful_samples[device] =
                        diagnostics.successful_samples;
                    g_adc_stale_elapsed_ms[device] = 0U;
                }
                else if (g_adc_stale_elapsed_ms[device] <=
                         (UINT16_MAX - PRODUCT_ADC_POLL_PERIOD_MS))
                {
                    g_adc_stale_elapsed_ms[device] +=
                        PRODUCT_ADC_POLL_PERIOD_MS;
                }
            }
        }
    }
    if (g_adc_diagnostic_due)
    {
        g_adc_diagnostic_due = false;
        (void)ProductAdcDriver_AuditNextDevice();
    }
    if (g_adc_recovery_due)
    {
        uint8_t device;
        g_adc_recovery_due = false;
        for (device = 0U; device < HAL_ADC_DEVICE_COUNT; device++)
        {
            AnalogInputDiagnostics_t diagnostics;
            if (AnalogInputService_GetDiagnostics(device, &diagnostics) &&
                !diagnostics.online)
            {
                (void)AnalogInputService_RetryDevice(device);
            }
        }
    }
    (void)SystemRoutine_ExecuteControl(g_system_timestamp_ms);
    for (input = 0U; input < FACTORY_CALIBRATION_INPUT_COUNT; input++)
    {
        AnalogInputSample_t sample;
        if (AnalogInputService_GetLatestByInput(input, &sample))
        {
            FactoryCalibrationService_UpdateLiveMicrovolts(
                input, sample.microvolts);
        }
    }
    for (input = 0U;
         input < PRODUCT_MODBUS_TEMPERATURE_INPUT_COUNT;
         input++)
    {
        (void)ProductSensorConfigurationConsumer_Process(input);
        (void)ProductSensorMeasurementService_Process(input);
        (void)AlarmConfigurationEventConsumer_Process(input);
        (void)SafetyConfigurationEventConsumer_Process(input);
    }
    ProductFramBankTest_Process();
    if (!ProductFramBankTest_IsBusy())
    {
        for (input = 0U;
             input < PRODUCT_MODBUS_TEMPERATURE_INPUT_COUNT;
             input++)
        {
            (void)NvmConfigurationEventConsumer_Process(input);
        }
    }

    for (input = 0U; input < 4U; input++)
    {
        bool inhibited =
            SafetyConfigurationEventConsumer_IsOutputInhibited(input);
        if (inhibited != g_last_pwm_inhibited[input])
        {
            (void)PwmOutputService_RefreshSafety(input);
            g_last_pwm_inhibited[input] = inhibited;
        }
    }
}
