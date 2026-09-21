#include "product_adc_driver.h"

#include <stddef.h>
#include <stdint.h>

#include "HalAdc.h"
#include "adi_ad7124_driver.h"
#include "product_ad7124_driver.h"

typedef struct
{
    uint8_t device_index;
} ProductAdcDriverContext_t;

static ProductAdcDriverContext_t g_adc_context[HAL_ADC_DEVICE_COUNT] =
{
    {0U}, {1U}, {2U}, {3U}
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

static HalAdcStatus_t ProductAdcTryRead(
    void *driver_context,
    HalAdcSample_t *sample)
{
    ProductAdcDriverContext_t *context =
        (ProductAdcDriverContext_t *)driver_context;
    adi_ad7124_device_t *device;
    adi_ad7124_status_t status;

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
    return MapDriverStatus(status);
}

bool ProductAdcDriver_Init(void)
{
    static const HalAdcDriverOps_t ops =
    {
        ProductAdcInitialize,
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
