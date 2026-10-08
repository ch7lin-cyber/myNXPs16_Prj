#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

#include "AnalogInputService.h"
#include "EventService.h"
#include "FaultService.h"
#include "HalAdc.h"
#include "ProductAdcConfig.h"
#include "product_adc_driver.h"
#include "product_sensor_configuration_consumer.h"
#include "product_temperature_input_types.h"

typedef struct
{
    HalAdcSetupConfig_t setup;
    HalAdcChannelConfig_t channel;
    HalAdcInputMode_t input_mode;
    uint16_t excitation_current_ua;
    uint8_t excitation_output0;
    uint8_t excitation_output1;
    uint16_t configure_count;
} MockAdc_t;

static MockAdc_t g_adc[PRODUCT_ADC_DEVICE_COUNT];
static uint32_t g_source_marks[PRODUCT_ADC_DEVICE_COUNT];
void ProductAdcDriver_SetConfigureSource(uint8_t device, ProductAdcConfigureSource_t source)
{
    assert(device < PRODUCT_ADC_DEVICE_COUNT);
    assert(source == PRODUCT_ADC_CONFIG_SOURCE_SENSOR_EVENT);
    g_source_marks[device]++;
}

static HalAdcStatus_t MockInitialize(void *context)
{
    (void)context;
    return HAL_ADC_STATUS_OK;
}

static HalAdcStatus_t MockConfigure(void *context,
                                    const HalAdcDeviceConfig_t *config)
{
    MockAdc_t *adc = (MockAdc_t *)context;

    assert(config != NULL);
    assert(config->setup_count == 1U);
    assert(config->channel_count == 1U);
    adc->setup = config->setups[0];
    adc->channel = config->channels[0];
    adc->input_mode = config->input_mode;
    adc->excitation_current_ua = config->excitation_current_ua;
    adc->excitation_output0 = config->excitation_output0;
    adc->excitation_output1 = config->excitation_output1;
    adc->configure_count++;
    return HAL_ADC_STATUS_OK;
}

static HalAdcStatus_t MockTryRead(void *context, HalAdcSample_t *sample)
{
    (void)context;
    (void)sample;
    return HAL_ADC_STATUS_NOT_READY;
}

static void RaiseAndProcess(uint8_t input, uint16_t sensor_type)
{
    EventTemperatureInputConfiguration_t old_config =
        {0.5F, PRODUCT_SENSOR_TYPE_TC_K};
    EventTemperatureInputConfiguration_t new_config =
        {0.5F, sensor_type};
    uint32_t event_id;

    assert(EventService_RaiseTemperatureInputConfigurationChanged(
        input, 1U,
        EVENT_TEMPERATURE_INPUT_CHANGE_SENSOR_TYPE,
        &old_config, &new_config, &event_id));
    assert(ProductSensorConfigurationConsumer_Process(input, 0U));
    assert(!EventService_IsTemperatureInputConfigurationChangedPending(input));
}

int main(int argc, char **argv)
{
    static const HalAdcDriverOps_t ops =
        {MockInitialize, MockConfigure, MockTryRead};
    static const HalAdcSetupConfig_t initial_setup =
        {HAL_ADC_REFERENCE_INTERNAL, HAL_ADC_FILTER_SINC4, HAL_ADC_GAIN_32,
         384U, true, true, false};
    static const HalAdcChannelConfig_t initial_channel =
        {0U, 0U, 5U, 4U, true};
    static const HalAdcDeviceConfig_t initial_config =
        {&initial_setup, 1U, &initial_channel, 1U,
         HAL_ADC_INPUT_MODE_VOLTAGE, 0U, 0U, 7U};
    AnalogInputRoute_t routes[PRODUCT_ADC_DEVICE_COUNT];
    AnalogInputRoute_t route;
    uint8_t input;

    for (input = 0U; input < PRODUCT_ADC_DEVICE_COUNT; input++)
    {
        routes[input].logical_input = input;
        routes[input].device = input;
        routes[input].channel = 0U;
        routes[input].sensor_class = ANALOG_INPUT_SENSOR_THERMOCOUPLE;
        assert(HalAdc_RegisterDriver(input, &ops, &g_adc[input]) ==
               HAL_ADC_STATUS_OK);
        assert(AnalogInputService_SetDeviceConfiguration(
                   input, &initial_config) == ANALOG_INPUT_STATUS_OK);
    }
    (void)argv;
    assert(AnalogInputService_SetRoutes(
               routes, (argc > 1) ? 2U : PRODUCT_ADC_DEVICE_COUNT) ==
           ANALOG_INPUT_STATUS_OK);
    assert(AnalogInputService_Initialize(PRODUCT_ADC_DEVICE_COUNT) ==
           ANALOG_INPUT_STATUS_OK);
    assert(EventService_Initialize(EVENT_ACK_COMMUNICATION));
    assert(EventService_ConfigureTemperatureInputRequiredAckMask(
        EVENT_ACK_ANALOG_INPUT));
    FaultService_Initialize();
    ProductSensorConfigurationConsumer_Initialize();

    if (argc > 1)
    {
        EventTemperatureInputConfiguration_t old_config =
            {0.5F, PRODUCT_SENSOR_TYPE_TC_K};
        EventTemperatureInputConfiguration_t new_config =
            {0.5F, PRODUCT_SENSOR_TYPE_VOLTAGE_0_5V};
        uint32_t event_id;
        uint16_t configure_count = g_adc[2].configure_count;

        assert(EventService_RaiseTemperatureInputConfigurationChanged(
            2U, 1U, EVENT_TEMPERATURE_INPUT_CHANGE_SENSOR_TYPE,
            &old_config, &new_config, &event_id));
        for (input = 0U; input < 10U; input++)
        {
            assert(!ProductSensorConfigurationConsumer_Process(2U, 0U));
            assert(g_adc[2].configure_count == configure_count);
        }
#if PRODUCT_ADC_DEBUG_ENABLE
        ProductSensorConfigurationDiagnostics_t diagnostics;
        assert(ProductSensorConfigurationConsumer_GetDiagnostics(2U, &diagnostics));
        assert(diagnostics.stage == 1U);
        assert(diagnostics.apply_failures == 10U);
        assert(diagnostics.configure_attempts == 0U);
#endif
        assert(g_source_marks[2] == 0U);
        assert(EventService_IsTemperatureInputConfigurationChangedPending(2U));
        return 0;
    }

    RaiseAndProcess(0U, PRODUCT_SENSOR_TYPE_RTD_100_OHM);
    assert(g_adc[0].setup.gain == HAL_ADC_GAIN_8);
    assert(g_adc[0].setup.reference == HAL_ADC_REFERENCE_EXTERNAL_1);
    assert(g_adc[0].setup.reference_buffer_enabled);
    assert(g_adc[0].setup.filter_word == 47U);
    assert(g_adc[0].channel.positive_input == PRODUCT_ADC_RTD_AIN_POSITIVE);
    assert(g_adc[0].channel.negative_input == PRODUCT_ADC_RTD_AIN_NEGATIVE);
    assert(g_adc[0].excitation_current_ua == 500U);
    assert(g_adc[0].excitation_output0 == PRODUCT_ADC_IEX1_AIN);
    assert(g_adc[0].excitation_output1 == PRODUCT_ADC_IEX2_AIN);
    assert(AnalogInputService_GetRoute(0U, &route));
    assert(route.sensor_class == ANALOG_INPUT_SENSOR_RTD);

    RaiseAndProcess(1U, PRODUCT_SENSOR_TYPE_RTD_1000_OHM);
    assert(g_adc[1].setup.gain == HAL_ADC_GAIN_1);
    assert(g_adc[1].excitation_current_ua == 250U);

    RaiseAndProcess(2U, PRODUCT_SENSOR_TYPE_CURRENT_4_20MA);
    assert(g_adc[2].setup.gain == HAL_ADC_GAIN_64);
    assert(g_adc[2].channel.positive_input == PRODUCT_ADC_CURRENT_AIN_POSITIVE);
    assert(g_adc[2].channel.negative_input == PRODUCT_ADC_CURRENT_AIN_NEGATIVE);
    assert(g_adc[2].input_mode == HAL_ADC_INPUT_MODE_CURRENT);
    assert(g_adc[2].excitation_current_ua == 0U);

    RaiseAndProcess(2U, PRODUCT_SENSOR_TYPE_VOLTAGE_0_50MV);
    assert(g_adc[2].channel.positive_input == PRODUCT_ADC_TC_AIN_POSITIVE);
    assert(g_adc[2].channel.negative_input == PRODUCT_ADC_TC_AIN_NEGATIVE);
    assert(g_adc[2].input_mode == HAL_ADC_INPUT_MODE_VOLTAGE);

    RaiseAndProcess(2U, PRODUCT_SENSOR_TYPE_VOLTAGE_0_5V);
    assert(g_adc[2].setup.gain == HAL_ADC_GAIN_32);
    assert(g_adc[2].setup.reference == HAL_ADC_REFERENCE_INTERNAL);
    assert(g_adc[2].channel.positive_input ==
           PRODUCT_ADC_VOLTAGE_AIN_POSITIVE);
    assert(g_adc[2].channel.negative_input ==
           PRODUCT_ADC_VOLTAGE_AIN_NEGATIVE);
    assert(g_adc[2].input_mode == HAL_ADC_INPUT_MODE_VOLTAGE);
    {
        uint16_t configure_count = g_adc[2].configure_count;

        assert(ProductSensorConfigurationConsumer_Process(2U, 0U));
        assert(g_adc[2].configure_count == configure_count);
    }

    RaiseAndProcess(3U, PRODUCT_SENSOR_TYPE_TC_B);
    assert(g_adc[3].setup.gain == HAL_ADC_GAIN_128);
    assert(g_adc[3].channel.positive_input == PRODUCT_ADC_TC_AIN_POSITIVE);
    assert(g_adc[3].channel.negative_input == PRODUCT_ADC_TC_AIN_NEGATIVE);
    assert(g_adc[3].input_mode == HAL_ADC_INPUT_MODE_VOLTAGE);
    /* Force ACK rejection: configuration must still happen exactly once. */
    assert(EventService_ConfigureTemperatureInputRequiredAckMask(EVENT_ACK_NVM));
    {
        EventTemperatureInputConfiguration_t old_config = {0.5F, PRODUCT_SENSOR_TYPE_TC_K};
        EventTemperatureInputConfiguration_t new_config = {0.5F, PRODUCT_SENSOR_TYPE_VOLTAGE_0_5V};
#if PRODUCT_ADC_DEBUG_ENABLE
        ProductSensorConfigurationDiagnostics_t before, after;
#endif
        uint32_t event_id;
        uint16_t configure_count = g_adc[0].configure_count;
        uint32_t marks = g_source_marks[0];
#if PRODUCT_ADC_DEBUG_ENABLE
        assert(ProductSensorConfigurationConsumer_GetDiagnostics(0U, &before));
#endif
        assert(EventService_RaiseTemperatureInputConfigurationChanged(0U, 9U,
            EVENT_TEMPERATURE_INPUT_CHANGE_SENSOR_TYPE, &old_config, &new_config, &event_id));
        for (input = 0U; input < 10U; input++)
        {
            assert(!ProductSensorConfigurationConsumer_Process(0U, 100U));
        }
        assert(g_adc[0].configure_count == configure_count + 1U);
        assert(g_source_marks[0] == marks + 1U);
#if PRODUCT_ADC_DEBUG_ENABLE
        assert(ProductSensorConfigurationConsumer_GetDiagnostics(0U, &after));
        assert(after.event_id == event_id && after.revision == 9U);
        assert(after.configure_attempts == before.configure_attempts + 1U);
        assert(after.apply_failures == before.apply_failures);
        assert(after.ack_attempts == before.ack_attempts + 10U);
        assert(after.ack_failures == before.ack_failures + 10U);
        assert(after.stage == 4U);
#endif
    }
    return 0;
}
