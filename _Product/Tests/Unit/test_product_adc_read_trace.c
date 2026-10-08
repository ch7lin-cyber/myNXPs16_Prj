#include <assert.h>
#include <string.h>

/* Include the driver to inject invalid runtime configurations deliberately. */
#include "../../Source/L01_ProductDriver/product_adc_driver.c"

GPIO_Type g_mock_gpio;
static adi_ad7124_device_t g_device;
static uint32_t g_raw = 0x009C0DD9UL;
static uint8_t g_channel;
static adi_ad7124_status_t g_read_status = kAdiAd7124_Ok;
static uint32_t g_error_enable;

void GPIO_PinWrite(GPIO_Type *base, uint32_t port, uint32_t pin, uint8_t output)
{
    (void)base; (void)port; (void)pin; (void)output;
}
adi_ad7124_device_t *ProductAd7124_GetDevice(uint8_t device)
{
    return (device < HAL_ADC_DEVICE_COUNT) ? &g_device : NULL;
}
adi_ad7124_status_t ProductAd7124_InitDevice(uint8_t device)
{
    (void)device;
    g_device.initialized = true;
    return kAdiAd7124_Ok;
}
adi_ad7124_status_t ADI_AD7124_Configure(adi_ad7124_device_t *device,
    const adi_ad7124_setup_config_t *setups, uint8_t setup_count,
    const adi_ad7124_channel_config_t *channels, uint8_t channel_count,
    const adi_ad7124_io_config_t *io)
{
    (void)device; (void)setups; (void)setup_count; (void)channels;
    (void)channel_count;
    assert(io->referenceOutputRequired);
    return kAdiAd7124_Ok;
}
adi_ad7124_status_t ADI_AD7124_WriteRegister(adi_ad7124_device_t *device,
    uint8_t address, uint32_t value)
{
    (void)device;
    if (address == ADI_AD7124_ERROR_ENABLE_REG) g_error_enable = value;
    return kAdiAd7124_Ok;
}
adi_ad7124_status_t ADI_AD7124_ReadRegister(adi_ad7124_device_t *device,
    uint8_t address, uint32_t *value)
{
    (void)device;
    *value = (address == ADI_AD7124_ERROR_ENABLE_REG) ? g_error_enable : 0U;
    return kAdiAd7124_Ok;
}
adi_ad7124_status_t ADI_AD7124_TryReadDataDiagnostic(adi_ad7124_device_t *device,
    uint32_t *raw, uint8_t *channel, uint8_t *status,
    uint32_t *error, bool *error_read)
{
    (void)device;
    *status = 0U; *error = 0U; *error_read = false;
    if (g_read_status == kAdiAd7124_Ok) { *raw = g_raw; *channel = g_channel; }
    return g_read_status;
}

int main(void)
{
    ProductAdcDriverContext_t *context = &g_adc_context[0];
    HalAdcSample_t sample;
    ProductAdcDriverDiagnostics_t diagnostics;
    HalAdcSetupConfig_t setup = {HAL_ADC_REFERENCE_INTERNAL, HAL_ADC_FILTER_SINC4,
        HAL_ADC_GAIN_32, 384U, true, true, false};
    HalAdcChannelConfig_t channel = {0U, 0U, 3U, 2U, true};
    HalAdcDeviceConfig_t config = {&setup, 1U, &channel, 1U,
        HAL_ADC_INPUT_MODE_VOLTAGE, 0U, 0U, 7U};
    assert(ProductAdcDriver_Init());
    assert(ProductAdcInitialize(context) == HAL_ADC_STATUS_OK);
    ProductAdcDriver_SetConfigureSource(0U, PRODUCT_ADC_CONFIG_SOURCE_SENSOR_EVENT);
    assert(ProductAdcConfigure(context, &config) == HAL_ADC_STATUS_OK);
    assert(ProductAdcTryRead(context, &sample) == HAL_ADC_STATUS_NOT_READY);
#if PRODUCT_ADC_DEBUG_ENABLE
    assert(context->diagnostics.first_sample_discards == 1U);
#endif
#if PRODUCT_ADC_DEBUG_ENABLE
    assert(context->diagnostics.read_failures == 0U);
#endif
    assert(ProductAdcTryRead(context, &sample) == HAL_ADC_STATUS_OK);
    assert(sample.microvolts > 17000 && sample.microvolts < 17200);
    assert(context->diagnostics.successful_samples == 1U);

    g_channel = 3U;
    assert(ProductAdcTryRead(context, &sample) == HAL_ADC_STATUS_DEVICE_ERROR);
#if PRODUCT_ADC_DEBUG_ENABLE
    assert(context->diagnostics.last_read_failure_stage == PRODUCT_ADC_READ_STAGE_CHANNEL);
#endif
#if PRODUCT_ADC_DEBUG_ENABLE
    assert(context->diagnostics.failure_channel == 3U);
#endif
    g_channel = 0U;
    channel.setup = 1U;
    assert(ProductAdcTryRead(context, &sample) == HAL_ADC_STATUS_DEVICE_ERROR);
#if PRODUCT_ADC_DEBUG_ENABLE
    assert(context->diagnostics.last_read_failure_stage == PRODUCT_ADC_READ_STAGE_SETUP);
#endif
    channel.setup = 0U;
    setup.gain = (HalAdcGain_t)0;
    assert(ProductAdcTryRead(context, &sample) == HAL_ADC_STATUS_DEVICE_ERROR);
#if PRODUCT_ADC_DEBUG_ENABLE
    assert(context->diagnostics.last_read_failure_stage == PRODUCT_ADC_READ_STAGE_CONVERT);
#endif
#if PRODUCT_ADC_DEBUG_ENABLE
    assert(context->diagnostics.failure_gain == 0U);
#endif
#if PRODUCT_ADC_DEBUG_ENABLE
    assert(context->diagnostics.conversion_diagnostics.result == HAL_ADC_CONVERSION_GAIN_ZERO);
#endif
#if PRODUCT_ADC_DEBUG_ENABLE
    assert(context->diagnostics.failure_reference_uv == 2500000UL);
#endif
#if PRODUCT_ADC_DEBUG_ENABLE
    assert(context->diagnostics.failure_raw_code == g_raw);
#endif
#if PRODUCT_ADC_DEBUG_ENABLE
    assert(context->diagnostics.failure_driver_status == 0);
#endif
    setup.gain = HAL_ADC_GAIN_32;

    /* A successful audit and recovery must not replace the failure snapshot. */
    assert(ProductAdcDriver_AuditNextDevice());
    assert(ProductAdcInitialize(context) == HAL_ADC_STATUS_OK);
    assert(ProductAdcConfigure(context, &config) == HAL_ADC_STATUS_OK);
#if PRODUCT_ADC_DEBUG_ENABLE
    assert(context->diagnostics.last_read_failure_stage == PRODUCT_ADC_READ_STAGE_CONVERT);
#endif
    assert(ProductAdcTryRead(context, &sample) == HAL_ADC_STATUS_NOT_READY);
#if PRODUCT_ADC_DEBUG_ENABLE
    assert(context->diagnostics.read_failures == 3U);
#endif
    assert(ProductAdcTryRead(context, &sample) == HAL_ADC_STATUS_OK);

    context->active_config = NULL;
    assert(ProductAdcTryRead(context, &sample) == HAL_ADC_STATUS_NOT_INITIALIZED);
#if PRODUCT_ADC_DEBUG_ENABLE
    assert(context->diagnostics.last_read_failure_stage == PRODUCT_ADC_READ_STAGE_CONFIG);
#endif
#if PRODUCT_ADC_DEBUG_ENABLE
    assert(!context->diagnostics.failure_config_valid);
#endif
    context->active_config = &config;
    g_read_status = kAdiAd7124_TransportError;
    (void)memset(&sample, 0xFF, sizeof(sample));
    assert(ProductAdcTryRead(context, &sample) == HAL_ADC_STATUS_IO_ERROR);
#if PRODUCT_ADC_DEBUG_ENABLE
    assert(context->diagnostics.last_read_failure_stage == PRODUCT_ADC_READ_STAGE_TRANSFER);
#endif
#if PRODUCT_ADC_DEBUG_ENABLE
    assert(context->diagnostics.failure_driver_status == kAdiAd7124_TransportError);
#endif
#if PRODUCT_ADC_DEBUG_ENABLE
    assert(context->diagnostics.failure_raw_code == 0U);
#endif
#if PRODUCT_ADC_DEBUG_ENABLE
    assert(context->diagnostics.failure_channel == UINT8_MAX);
#endif
    assert(ProductAdcDriver_GetDiagnostics(0U, &diagnostics));
#if PRODUCT_ADC_DEBUG_ENABLE
    assert(diagnostics.read_failures == 5U);
#endif
    /* Product board must request REFOUT even for an external-reference RTD. */
    setup.reference = HAL_ADC_REFERENCE_EXTERNAL_1;
    setup.gain = HAL_ADC_GAIN_8;
    assert(ProductAdcConfigure(context, &config) == HAL_ADC_STATUS_OK);
    return 0;
}
