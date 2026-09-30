#include "product_sensor_configuration_consumer.h"

#include <stddef.h>
#include <string.h>

#include "AnalogInputService.h"
#include "EventService.h"
#include "FaultService.h"
#include "ProductAdcConfig.h"
#include "product_temperature_input_types.h"

#define PRODUCT_SENSOR_INPUT_COUNT (4U)

typedef struct
{
    HalAdcSetupConfig_t setup;
    HalAdcChannelConfig_t adc_channel;
    HalAdcDeviceConfig_t device;
} ProductSensorRuntimeConfiguration_t;

static ProductSensorRuntimeConfiguration_t
    g_runtime_configuration[PRODUCT_SENSOR_INPUT_COUNT];

static bool IsRtd(uint16_t sensor_type)
{
    return (sensor_type >= PRODUCT_SENSOR_TYPE_RTD_100_OHM) &&
           (sensor_type <= PRODUCT_SENSOR_TYPE_RTD_CU50);
}

static HalAdcGain_t ThermocoupleGain(uint16_t linearization)
{
    switch (linearization)
    {
        case PRODUCT_TC_LINEARIZATION_B:
        case PRODUCT_TC_LINEARIZATION_S:
            return PRODUCT_ADC_GAIN_SB;
        case PRODUCT_TC_LINEARIZATION_T:
        case PRODUCT_TC_LINEARIZATION_R:
        case PRODUCT_TC_LINEARIZATION_C:
            return PRODUCT_ADC_GAIN_TRC;
        case PRODUCT_TC_LINEARIZATION_J:
            return PRODUCT_ADC_GAIN_J_JPT100;
        default:
            return PRODUCT_ADC_GAIN_GENERAL;
    }
}

static HalAdcGain_t SensorGain(uint16_t sensor_type,
                               uint16_t linearization)
{
    switch (sensor_type)
    {
        case PRODUCT_SENSOR_TYPE_THERMOCOUPLE:
            return ThermocoupleGain(linearization);
        case PRODUCT_SENSOR_TYPE_RTD_JPT100:
            return PRODUCT_ADC_GAIN_J_JPT100;
        case PRODUCT_SENSOR_TYPE_RTD_100_OHM:
        case PRODUCT_SENSOR_TYPE_RTD_NI120:
            return PRODUCT_ADC_GAIN_PT100_NI120;
        case PRODUCT_SENSOR_TYPE_RTD_1000_OHM:
            return PRODUCT_ADC_GAIN_PT1000;
        case PRODUCT_SENSOR_TYPE_CURRENT_0_20MA:
            return PRODUCT_ADC_GAIN_CURRENT_0_20MA;
        case PRODUCT_SENSOR_TYPE_CURRENT_4_20MA:
            return PRODUCT_ADC_GAIN_CURRENT_4_20MA;
        default:
            return PRODUCT_ADC_GAIN_GENERAL;
    }
}

static uint16_t ExcitationCurrent(uint16_t sensor_type)
{
    switch (sensor_type)
    {
        case PRODUCT_SENSOR_TYPE_RTD_1000_OHM:
            return PRODUCT_ADC_IEX_UA_PT1000;
        case PRODUCT_SENSOR_TYPE_RTD_JPT100:
            return PRODUCT_ADC_IEX_UA_JPT100;
        case PRODUCT_SENSOR_TYPE_RTD_100_OHM:
            return PRODUCT_ADC_IEX_UA_PT100;
        case PRODUCT_SENSOR_TYPE_RTD_NI120:
            return PRODUCT_ADC_IEX_UA_NI120;
        case PRODUCT_SENSOR_TYPE_RTD_CU50:
            return PRODUCT_ADC_IEX_UA_CU50;
        default:
            return PRODUCT_ADC_IEX_UA_DEFAULT;
    }
}

static AnalogInputSensorClass_t SensorClass(uint16_t sensor_type)
{
    if (sensor_type == PRODUCT_SENSOR_TYPE_OFF)
    {
        return ANALOG_INPUT_SENSOR_DISABLED;
    }
    if (sensor_type == PRODUCT_SENSOR_TYPE_THERMOCOUPLE)
    {
        return ANALOG_INPUT_SENSOR_THERMOCOUPLE;
    }
    if (IsRtd(sensor_type))
    {
        return ANALOG_INPUT_SENSOR_RTD;
    }
    if ((sensor_type == PRODUCT_SENSOR_TYPE_CURRENT_0_20MA) ||
        (sensor_type == PRODUCT_SENSOR_TYPE_CURRENT_4_20MA))
    {
        return ANALOG_INPUT_SENSOR_CURRENT;
    }
    return ANALOG_INPUT_SENSOR_VOLTAGE;
}

static void BuildConfiguration(
    uint8_t channel,
    const EventTemperatureInputConfiguration_t *configuration)
{
    ProductSensorRuntimeConfiguration_t *runtime =
        &g_runtime_configuration[channel];
    bool is_rtd = IsRtd(configuration->sensor_type);
    bool is_current =
        (configuration->sensor_type == PRODUCT_SENSOR_TYPE_CURRENT_0_20MA) ||
        (configuration->sensor_type == PRODUCT_SENSOR_TYPE_CURRENT_4_20MA);

    runtime->setup.reference = is_rtd ? PRODUCT_ADC_RTD_REFERENCE :
                                      PRODUCT_ADC_TC_REFERENCE;
    runtime->setup.filter = is_rtd ? PRODUCT_ADC_RTD_FILTER :
                                    PRODUCT_ADC_TC_FILTER;
    runtime->setup.gain = SensorGain(configuration->sensor_type,
                                     configuration->tc_linearization);
    runtime->setup.filter_word = is_rtd ? PRODUCT_ADC_RTD_FILTER_WORD :
                                         PRODUCT_ADC_TC_FILTER_WORD;
    runtime->setup.bipolar = true;
    runtime->setup.input_buffer_enabled = true;
    runtime->setup.reference_buffer_enabled = false;

    runtime->adc_channel.channel = PRODUCT_ADC_ACTIVE_CHANNEL;
    runtime->adc_channel.setup = PRODUCT_ADC_DEFAULT_SETUP;
    runtime->adc_channel.enabled =
        configuration->sensor_type != PRODUCT_SENSOR_TYPE_OFF;
    if (configuration->sensor_type == PRODUCT_SENSOR_TYPE_THERMOCOUPLE)
    {
        runtime->adc_channel.positive_input = PRODUCT_ADC_TC_AIN_POSITIVE;
        runtime->adc_channel.negative_input = PRODUCT_ADC_TC_AIN_NEGATIVE;
    }
    else if (is_rtd)
    {
        runtime->adc_channel.positive_input = PRODUCT_ADC_RTD_AIN_POSITIVE;
        runtime->adc_channel.negative_input = PRODUCT_ADC_RTD_AIN_NEGATIVE;
    }
    else if (is_current)
    {
        runtime->adc_channel.positive_input = PRODUCT_ADC_CURRENT_AIN_POSITIVE;
        runtime->adc_channel.negative_input = PRODUCT_ADC_CURRENT_AIN_NEGATIVE;
    }
    else
    {
        runtime->adc_channel.positive_input = PRODUCT_ADC_VOLTAGE_AIN_POSITIVE;
        runtime->adc_channel.negative_input = PRODUCT_ADC_VOLTAGE_AIN_NEGATIVE;
    }

    runtime->device.setups = &runtime->setup;
    runtime->device.setup_count = 1U;
    runtime->device.channels = &runtime->adc_channel;
    runtime->device.channel_count = 1U;
    runtime->device.input_mode = is_current ? HAL_ADC_INPUT_MODE_CURRENT :
                                             HAL_ADC_INPUT_MODE_VOLTAGE;
    runtime->device.excitation_current_ua =
        ExcitationCurrent(configuration->sensor_type);
    runtime->device.excitation_output0 = PRODUCT_ADC_IEX1_AIN;
    runtime->device.excitation_output1 = PRODUCT_ADC_IEX2_AIN;
}

void ProductSensorConfigurationConsumer_Initialize(void)
{
    (void)memset(g_runtime_configuration, 0,
                 sizeof(g_runtime_configuration));
}

bool ProductSensorConfigurationConsumer_Process(uint8_t channel)
{
    TemperatureInputConfigurationChangedEvent_t event;
    AnalogInputStatus_t status;

    if (channel >= PRODUCT_SENSOR_INPUT_COUNT)
    {
        return false;
    }
    if (!EventService_GetTemperatureInputConfigurationChanged(channel,
                                                               &event))
    {
        return true;
    }
    if ((event.completed_ack_mask & EVENT_ACK_ANALOG_INPUT) != 0U)
    {
        return true;
    }
    if ((event.changed_mask &
         (EVENT_TEMPERATURE_INPUT_CHANGE_SENSOR_TYPE |
          EVENT_TEMPERATURE_INPUT_CHANGE_TC_LINEARIZATION)) == 0U)
    {
        return EventService_Acknowledge(event.event_id,
                                        EVENT_ACK_ANALOG_INPUT);
    }

    BuildConfiguration(channel, &event.new_configuration);
    status = AnalogInputService_ReconfigureDevice(
        channel, &g_runtime_configuration[channel].device);
    if ((status != ANALOG_INPUT_STATUS_OK) ||
        !AnalogInputService_SetInputSensorClass(
            channel, SensorClass(event.new_configuration.sensor_type)))
    {
        (void)FaultService_Raise(
            FAULT_CODE_ANALOG_INPUT_RECONFIGURE_FAILED,
            (uint16_t)status, event.configuration_revision, event.event_id);
        return false;
    }

    (void)FaultService_Clear(FAULT_CODE_ANALOG_INPUT_RECONFIGURE_FAILED);
    return EventService_Acknowledge(event.event_id, EVENT_ACK_ANALOG_INPUT);
}
