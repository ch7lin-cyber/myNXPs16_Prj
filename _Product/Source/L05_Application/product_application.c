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
#include "product_temperature_range_resolver.h"
#include "product_modbus_register_adapter.h"
#include "product_adc_driver.h"
#include "product_dip_switch_driver.h"
#include "bsp_internal_adc.h"
#include "ProductAdcConfig.h"
#include "ProductDipSwitchConfig.h"
#include "ProductInternalAdcConfig.h"
#include "ProductLowVoltageConfig.h"
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
static uint16_t g_cjc_elapsed_ms;
static uint16_t g_mcu_temperature_elapsed_ms;
static bool g_adc_poll_due;
static bool g_adc_recovery_due;
static bool g_cjc_due;
static bool g_mcu_temperature_due;
static bool g_digital_input_due;
static uint16_t g_dip_switch_elapsed_ms;
static bool g_dip_switch_due;
static uint16_t g_low_voltage_elapsed_ms;
static bool g_low_voltage_due;

static void ProcessMcuTemperatureSafety(void)
{
    BspInternalAdcMcuTemperature_t temperature;

    if (!BspInternalAdc_GetMcuTemperature(&temperature))
    {
        return;
    }
    ProductMcuTemperatureSafety_Process(
        temperature.valid,
        temperature.overtemperature,
        temperature.temperature_centi_c);
}

static bool IsPwmOutputInhibited(uint8_t channel, void *context)
{
    (void)context;
    return SafetyConfigurationEventConsumer_IsOutputInhibited(channel);
}

bool ProductApplication_Init(void)
{
    AnalogInputStatus_t adc_status;
    uint8_t adc_device;
    uint8_t adc_route_count;

    FaultService_Initialize();
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
    g_cjc_elapsed_ms = 0U;
    g_mcu_temperature_elapsed_ms = 0U;
    g_adc_poll_due = false;
    g_adc_recovery_due = false;
    g_cjc_due = false;
    g_mcu_temperature_due = false;
    g_digital_input_due = false;
    g_dip_switch_elapsed_ms = 0U;
    g_dip_switch_due = false;
    g_low_voltage_elapsed_ms = 0U;
    g_low_voltage_due = false;
    (void)BspInternalAdc_RequestCjcSamples();
    (void)BspInternalAdc_RequestMcuTemperature();
    return true;
}

void ProductApplication_Tick1ms(void)
{
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
    if (g_low_voltage_due)
    {
        g_low_voltage_due = false;
        ProductLowVoltageSafety_Process();
    }
    BspInternalAdc_Process();
    ProcessMcuTemperatureSafety();
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
