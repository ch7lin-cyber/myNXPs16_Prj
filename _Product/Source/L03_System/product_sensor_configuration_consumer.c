#include "product_sensor_configuration_consumer.h"

#include <stddef.h>
#include <string.h>

#include "AnalogInputService.h"
#include "EventService.h"
#include "FaultService.h"
#include "SystemFaultService.h"
#include "ProductAdcConfig.h"
#include "product_adc_driver.h"
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
static uint32_t g_applied_event_id[PRODUCT_SENSOR_INPUT_COUNT];
static uint16_t g_applied_revision[PRODUCT_SENSOR_INPUT_COUNT];
#if PRODUCT_ADC_DEBUG_ENABLE
static ProductSensorConfigurationDiagnostics_t g_diagnostics[PRODUCT_SENSOR_INPUT_COUNT];

bool ProductSensorConfigurationConsumer_GetDiagnostics(
    uint8_t channel, ProductSensorConfigurationDiagnostics_t *diagnostics)
{
    if ((channel >= PRODUCT_SENSOR_INPUT_COUNT) || (diagnostics == NULL))
    {
        return false;
    }
    *diagnostics = g_diagnostics[channel];
    return true;
}

#else
bool ProductSensorConfigurationConsumer_GetDiagnostics(
    uint8_t channel, ProductSensorConfigurationDiagnostics_t *diagnostics)
{
    (void)channel;
    (void)diagnostics;
    return false;
}
#endif

static bool AcknowledgeConfiguration(uint8_t channel, uint32_t event_id)
{
#if PRODUCT_ADC_DEBUG_ENABLE
    ProductSensorConfigurationDiagnostics_t *diagnostics = &g_diagnostics[channel];
#else
    (void)channel;
#endif
    PRODUCT_ADC_DEBUG_ONLY(diagnostics->stage = 4U);
    PRODUCT_ADC_DEBUG_ONLY(diagnostics->ack_attempts++);
    if (!EventService_Acknowledge(event_id, EVENT_ACK_ANALOG_INPUT))
    {
        PRODUCT_ADC_DEBUG_ONLY(diagnostics->ack_failures++);
        return false;
    }
    PRODUCT_ADC_DEBUG_ONLY(diagnostics->stage = 5U);
    return true;
}

static bool IsRtd(uint16_t sensor_type)
{
    return (sensor_type >= PRODUCT_SENSOR_TYPE_RTD_100_OHM) &&
           (sensor_type <= PRODUCT_SENSOR_TYPE_RTD_CU50);
}

static bool IsThermocouple(uint16_t sensor_type)
{
    switch (sensor_type)
    {
        case PRODUCT_SENSOR_TYPE_TC_B:
        case PRODUCT_SENSOR_TYPE_TC_C:
        case PRODUCT_SENSOR_TYPE_TC_D:
        case PRODUCT_SENSOR_TYPE_TC_E:
        case PRODUCT_SENSOR_TYPE_TC_J:
        case PRODUCT_SENSOR_TYPE_TC_K:
        case PRODUCT_SENSOR_TYPE_TC_N:
        case PRODUCT_SENSOR_TYPE_TC_R:
        case PRODUCT_SENSOR_TYPE_TC_S:
        case PRODUCT_SENSOR_TYPE_TC_T:
        case PRODUCT_SENSOR_TYPE_TC_L:
        case PRODUCT_SENSOR_TYPE_TC_U:
        case PRODUCT_SENSOR_TYPE_TC_TXK:
            return true;
        default:
            return false;
    }
}

static HalAdcGain_t SensorGain(uint16_t sensor_type)
{
    switch (sensor_type)
    {
        case PRODUCT_SENSOR_TYPE_TC_B:
        case PRODUCT_SENSOR_TYPE_TC_S:
            return PRODUCT_ADC_GAIN_SB;
        case PRODUCT_SENSOR_TYPE_TC_T:
        case PRODUCT_SENSOR_TYPE_TC_R:
        case PRODUCT_SENSOR_TYPE_TC_C:
            return PRODUCT_ADC_GAIN_TRC;
        case PRODUCT_SENSOR_TYPE_TC_J:
            return PRODUCT_ADC_GAIN_J_JPT100;
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
    if (IsThermocouple(sensor_type))
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
    bool is_millivolt =
        configuration->sensor_type == PRODUCT_SENSOR_TYPE_VOLTAGE_0_50MV;

    runtime->setup.reference = is_rtd ? PRODUCT_ADC_RTD_REFERENCE :
                                      PRODUCT_ADC_TC_REFERENCE;
    runtime->setup.filter = is_rtd ? PRODUCT_ADC_RTD_FILTER :
                                    PRODUCT_ADC_TC_FILTER;
    runtime->setup.gain = SensorGain(configuration->sensor_type);
    runtime->setup.filter_word = is_rtd ? PRODUCT_ADC_RTD_FILTER_WORD :
                                         PRODUCT_ADC_TC_FILTER_WORD;
    runtime->setup.bipolar = true;
    runtime->setup.input_buffer_enabled = true;
    runtime->setup.reference_buffer_enabled = is_rtd;

    runtime->adc_channel.channel = PRODUCT_ADC_ACTIVE_CHANNEL;
    runtime->adc_channel.setup = PRODUCT_ADC_DEFAULT_SETUP;
    runtime->adc_channel.enabled =
        configuration->sensor_type != PRODUCT_SENSOR_TYPE_OFF;
    if (IsThermocouple(configuration->sensor_type) || is_millivolt)
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
    (void)memset(g_applied_event_id, 0, sizeof(g_applied_event_id));
    (void)memset(g_applied_revision, 0, sizeof(g_applied_revision));
    PRODUCT_ADC_DEBUG_ONLY((void)memset(g_diagnostics, 0, sizeof(g_diagnostics)));
}

bool ProductSensorConfigurationConsumer_Process(uint8_t channel,
                                                uint32_t timestamp_ms)
{
    TemperatureInputConfigurationChangedEvent_t event;
    AnalogInputStatus_t status;
    AnalogInputRoute_t route;

    if (channel >= PRODUCT_SENSOR_INPUT_COUNT)
    {
        return false;
    }
    if (!EventService_GetTemperatureInputConfigurationChanged(channel,
                                                               &event))
    {
        return true;
    }
    PRODUCT_ADC_DEBUG_ONLY(g_diagnostics[channel].event_id = event.event_id);
    PRODUCT_ADC_DEBUG_ONLY(g_diagnostics[channel].revision = event.configuration_revision);
    if ((event.completed_ack_mask & EVENT_ACK_ANALOG_INPUT) != 0U)
    {
        return true;
    }
    /*
     * Hardware reconfiguration and event acknowledgement are two separate
     * operations.  If acknowledgement must be retried, never configure the
     * ADC again: doing so continually rearms the mandatory first-sample
     * discard and prevents the measurement chain from publishing a sample.
     */
    if ((g_applied_event_id[channel] == event.event_id) &&
        (g_applied_revision[channel] == event.configuration_revision))
    {
        return AcknowledgeConfiguration(channel, event.event_id);
    }
    if ((event.changed_mask &
         EVENT_TEMPERATURE_INPUT_CHANGE_SENSOR_TYPE) == 0U)
    {
        return AcknowledgeConfiguration(channel, event.event_id);
    }

    /* Validate routing before touching hardware on every event retry. */
    PRODUCT_ADC_DEBUG_ONLY(g_diagnostics[channel].stage = 1U);
    status = ANALOG_INPUT_STATUS_INVALID_ARGUMENT;
    if (AnalogInputService_GetRoute(channel, &route) &&
        (route.device == channel) &&
        (route.channel == PRODUCT_ADC_ACTIVE_CHANNEL))
    {
        BuildConfiguration(channel, &event.new_configuration);
        PRODUCT_ADC_DEBUG_ONLY(g_diagnostics[channel].stage = 2U);
        PRODUCT_ADC_DEBUG_ONLY(g_diagnostics[channel].configure_attempts++);
        ProductAdcDriver_SetConfigureSource(channel, PRODUCT_ADC_CONFIG_SOURCE_SENSOR_EVENT);
        status = AnalogInputService_ReconfigureDevice(
            channel, &g_runtime_configuration[channel].device);
    }
    PRODUCT_ADC_DEBUG_ONLY(g_diagnostics[channel].last_status = (uint16_t)status);
    if (status == ANALOG_INPUT_STATUS_OK)
    {
        PRODUCT_ADC_DEBUG_ONLY(g_diagnostics[channel].stage = 3U);
        if (!AnalogInputService_SetInputSensorClass(
                channel, SensorClass(event.new_configuration.sensor_type)))
        {
            status = ANALOG_INPUT_STATUS_INVALID_ARGUMENT;
            PRODUCT_ADC_DEBUG_ONLY(g_diagnostics[channel].last_status = (uint16_t)status);
        }
    }
    if (status != ANALOG_INPUT_STATUS_OK)
    {
        PRODUCT_ADC_DEBUG_ONLY(g_diagnostics[channel].apply_failures++);
        int32_t values[SNAPSHOT_SERVICE_VALUE_COUNT] = {0};

        values[0] = channel;
        values[1] = (int32_t)status;
        values[2] = event.new_configuration.sensor_type;
        (void)SystemFaultService_Raise(
            FAULT_CODE_ANALOG_INPUT_RECONFIGURE_FAILED,
            (uint16_t)status, event.configuration_revision, event.event_id,
            timestamp_ms, values);
        return false;
    }

    (void)SystemFaultService_ClearRecovered(
        FAULT_CODE_ANALOG_INPUT_RECONFIGURE_FAILED,
        timestamp_ms, event.event_id);
    g_applied_event_id[channel] = event.event_id;
    g_applied_revision[channel] = event.configuration_revision;
    return AcknowledgeConfiguration(channel, event.event_id);
}
