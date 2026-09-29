#include "product_adc_driver.h"

#include <stddef.h>
#include <stdint.h>

#include "HalAdc.h"
#include "HalAdcMeasurement.h"
#include "ProductAdcConfig.h"
#include "adi_ad7124_driver.h"
#include "product_ad7124_driver.h"

typedef struct
{
    uint8_t device_index;
    bool discard_next_sample;
} ProductAdcDriverContext_t;

static ProductAdcDriverContext_t g_adc_context[HAL_ADC_DEVICE_COUNT] =
{
    {0U, false}, {1U, false}, {2U, false}, {3U, false}
};

/* One product input per AD7124-4; all four factory-default to K type. */
static const HalAdcSetupConfig_t g_setups[] =
{
    {PRODUCT_ADC_TC_REFERENCE, PRODUCT_ADC_TC_FILTER, PRODUCT_ADC_TC_GAIN,
     PRODUCT_ADC_TC_FILTER_WORD, true, true, false}
};

#define ADC_DEFAULT_K_CHANNEL \
    {PRODUCT_ADC_ACTIVE_CHANNEL, PRODUCT_ADC_DEFAULT_SETUP, \
     PRODUCT_ADC_TC_AIN_POSITIVE, PRODUCT_ADC_TC_AIN_NEGATIVE, true}

static const HalAdcChannelConfig_t
    g_channels[HAL_ADC_DEVICE_COUNT][PRODUCT_ADC_CHANNELS_PER_DEVICE] =
{
    {ADC_DEFAULT_K_CHANNEL}, {ADC_DEFAULT_K_CHANNEL},
    {ADC_DEFAULT_K_CHANNEL}, {ADC_DEFAULT_K_CHANNEL}
};

static const HalAdcDeviceConfig_t g_device_config[HAL_ADC_DEVICE_COUNT] =
{
    {g_setups, 1U, g_channels[0], PRODUCT_ADC_CHANNELS_PER_DEVICE},
    {g_setups, 1U, g_channels[1], PRODUCT_ADC_CHANNELS_PER_DEVICE},
    {g_setups, 1U, g_channels[2], PRODUCT_ADC_CHANNELS_PER_DEVICE},
    {g_setups, 1U, g_channels[3], PRODUCT_ADC_CHANNELS_PER_DEVICE}
};

static const AnalogInputRoute_t g_routes[PRODUCT_ADC_DEVICE_COUNT] =
{
    {0U, 0U, 0U, ANALOG_INPUT_SENSOR_THERMOCOUPLE},
    {1U, 1U, 0U, ANALOG_INPUT_SENSOR_THERMOCOUPLE},
    {2U, 2U, 0U, ANALOG_INPUT_SENSOR_THERMOCOUPLE},
    {3U, 3U, 0U, ANALOG_INPUT_SENSOR_THERMOCOUPLE}
};

static HalAdcStatus_t MapDriverStatus(adi_ad7124_status_t status)
{
    if (status == kAdiAd7124_Ok)
    {
        return HAL_ADC_STATUS_OK;
    }
    if (status == kAdiAd7124_NotReady)
    {
        return HAL_ADC_STATUS_NOT_READY;
    }
    if (status == kAdiAd7124_InvalidArgument)
    {
        return HAL_ADC_STATUS_INVALID_ARGUMENT;
    }
    if ((status == kAdiAd7124_TransportError) ||
        (status == kAdiAd7124_Timeout))
    {
        return HAL_ADC_STATUS_IO_ERROR;
    }
    return HAL_ADC_STATUS_DEVICE_ERROR;
}

static HalAdcStatus_t ProductAdcInitialize(void *driver_context)
{
    ProductAdcDriverContext_t *context =
        (ProductAdcDriverContext_t *)driver_context;

    if (context == NULL)
    {
        return HAL_ADC_STATUS_INVALID_ARGUMENT;
    }
    return MapDriverStatus(
        ProductAd7124_InitDevice(context->device_index));
}

static uint8_t MapGain(HalAdcGain_t gain)
{
    uint8_t code = 0U;
    uint16_t value = (uint16_t)gain;
    while (value > 1U)
    {
        value >>= 1U;
        code++;
    }
    return code;
}

static HalAdcStatus_t ProductAdcConfigure(
    void *driver_context, const HalAdcDeviceConfig_t *config)
{
    ProductAdcDriverContext_t *context =
        (ProductAdcDriverContext_t *)driver_context;
    adi_ad7124_setup_config_t setups[HAL_ADC_SETUP_COUNT];
    adi_ad7124_channel_config_t channels[HAL_ADC_CHANNELS_PER_DEVICE];
    uint8_t index;

    if ((context == NULL) || (config == NULL))
    {
        return HAL_ADC_STATUS_INVALID_ARGUMENT;
    }
    for (index = 0U; index < config->setup_count; index++)
    {
        setups[index].setup = index;
        setups[index].reference = (uint8_t)config->setups[index].reference;
        setups[index].gain = MapGain(config->setups[index].gain);
        setups[index].filter =
            (config->setups[index].filter == HAL_ADC_FILTER_SINC3) ? 2U :
            (config->setups[index].filter == HAL_ADC_FILTER_FAST_SINC4) ? 4U :
            (config->setups[index].filter == HAL_ADC_FILTER_POST) ? 6U : 0U;
        setups[index].filterWord = config->setups[index].filter_word;
        setups[index].bipolar = config->setups[index].bipolar;
        setups[index].inputBufferEnabled =
            config->setups[index].input_buffer_enabled;
        setups[index].referenceBufferEnabled =
            config->setups[index].reference_buffer_enabled;
        setups[index].reject60Hz = (PRODUCT_ADC_REJECT_60_HZ != 0U);
    }
    for (index = 0U; index < config->channel_count; index++)
    {
        channels[index].channel = config->channels[index].channel;
        channels[index].setup = config->channels[index].setup;
        channels[index].positiveInput = config->channels[index].positive_input;
        channels[index].negativeInput = config->channels[index].negative_input;
        channels[index].enabled = config->channels[index].enabled;
    }
    {
        HalAdcStatus_t status = MapDriverStatus(ADI_AD7124_Configure(
            ProductAd7124_GetDevice(context->device_index), setups,
            config->setup_count, channels, config->channel_count));
        if (status == HAL_ADC_STATUS_OK)
        {
            context->discard_next_sample = true;
        }
        return status;
    }
}

static HalAdcStatus_t ProductAdcTryRead(
    void *driver_context,
    HalAdcSample_t *sample)
{
    ProductAdcDriverContext_t *context =
        (ProductAdcDriverContext_t *)driver_context;
    adi_ad7124_device_t *device;
    adi_ad7124_status_t status;
    const HalAdcChannelConfig_t *channel_config = NULL;
    const HalAdcSetupConfig_t *setup;
    uint32_t reference_uv;
    uint8_t index;

    if ((context == NULL) || (sample == NULL))
    {
        return HAL_ADC_STATUS_INVALID_ARGUMENT;
    }

    device = ProductAd7124_GetDevice(context->device_index);
    if ((device == NULL) || !device->initialized)
    {
        return HAL_ADC_STATUS_NOT_INITIALIZED;
    }

    status = ADI_AD7124_TryReadData(
        device, &sample->raw_code, &sample->channel);
    if (status != kAdiAd7124_Ok)
    {
        return MapDriverStatus(status);
    }
    if (context->discard_next_sample)
    {
        context->discard_next_sample = false;
        return HAL_ADC_STATUS_NOT_READY;
    }
    for (index = 0U;
         index < g_device_config[context->device_index].channel_count;
         index++)
    {
        if (g_device_config[context->device_index].channels[index].channel ==
            sample->channel)
        {
            channel_config =
                &g_device_config[context->device_index].channels[index];
            break;
        }
    }
    if (channel_config == NULL)
    {
        return HAL_ADC_STATUS_DEVICE_ERROR;
    }
    setup = &g_setups[channel_config->setup];
    reference_uv =
        (setup->reference == HAL_ADC_REFERENCE_INTERNAL) ?
            PRODUCT_ADC_INTERNAL_REFERENCE_UV :
        (setup->reference == HAL_ADC_REFERENCE_EXTERNAL_1) ?
            PRODUCT_ADC_EXTERNAL_REFERENCE1_UV :
        (setup->reference == HAL_ADC_REFERENCE_EXTERNAL_2) ?
            PRODUCT_ADC_EXTERNAL_REFERENCE2_UV :
            PRODUCT_ADC_SUPPLY_REFERENCE_UV;
    if (!HalAdcMeasurement_CodeToMicrovolts(
            sample->raw_code, reference_uv, (uint16_t)setup->gain,
            setup->bipolar, &sample->microvolts))
    {
        return HAL_ADC_STATUS_DEVICE_ERROR;
    }
    return HAL_ADC_STATUS_OK;
}

bool ProductAdcDriver_Init(void)
{
    static const HalAdcDriverOps_t ops =
    {
        ProductAdcInitialize,
        ProductAdcConfigure,
        ProductAdcTryRead
    };
    uint8_t device;

    for (device = 0U; device < HAL_ADC_DEVICE_COUNT; device++)
    {
        if (HalAdc_RegisterDriver(device, &ops,
                                  &g_adc_context[device]) !=
            HAL_ADC_STATUS_OK)
        {
            return false;
        }
    }
    return true;
}

const HalAdcDeviceConfig_t *ProductAdcDriver_GetDeviceConfig(uint8_t device)
{
    return (device < HAL_ADC_DEVICE_COUNT) ? &g_device_config[device] : NULL;
}

const AnalogInputRoute_t *ProductAdcDriver_GetRoutes(uint8_t *route_count)
{
    if (route_count != NULL)
    {
        *route_count = (uint8_t)(sizeof(g_routes) / sizeof(g_routes[0]));
    }
    return g_routes;
}
