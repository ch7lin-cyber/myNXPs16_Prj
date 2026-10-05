#include "product_adc_driver.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "HalAdc.h"
#include "HalAdcMeasurement.h"
#include "ProductAdcConfig.h"
#include "adi_ad7124_driver.h"
#include "product_ad7124_driver.h"
#include "fsl_gpio.h"
#include "pin_mux.h"

typedef struct
{
    uint8_t device_index;
    bool discard_next_sample;
    const HalAdcDeviceConfig_t *active_config;
    uint8_t category_streak[6U];
    ProductAdcDriverDiagnostics_t diagnostics;
} ProductAdcDriverContext_t;

static ProductAdcDriverContext_t g_adc_context[HAL_ADC_DEVICE_COUNT] =
{
    {.device_index = 0U}, {.device_index = 1U},
    {.device_index = 2U}, {.device_index = 3U}
};
static uint8_t g_next_audit_device;

#define PRODUCT_ADC_ERROR_COMM_MASK \
    (ADI_AD7124_ERROR_SPI_IGNORE_MASK | ADI_AD7124_ERROR_SPI_SCLK_MASK | \
     ADI_AD7124_ERROR_SPI_READ_MASK | ADI_AD7124_ERROR_SPI_WRITE_MASK)
#define PRODUCT_ADC_ERROR_INTEGRITY_MASK \
    (ADI_AD7124_ERROR_SPI_CRC_MASK | ADI_AD7124_ERROR_MM_CRC_MASK | \
     ADI_AD7124_ERROR_ROM_CRC_MASK)
#define PRODUCT_ADC_ERROR_ENABLE_COMMON_MASK \
    (ADI_AD7124_ERROR_ADC_CAL_MASK | ADI_AD7124_ERROR_ADC_CONV_MASK | \
     ADI_AD7124_ERROR_ADC_SAT_MASK | ADI_AD7124_ERROR_AINP_OV_MASK | \
     ADI_AD7124_ERROR_AINP_UV_MASK | ADI_AD7124_ERROR_AINM_OV_MASK | \
     ADI_AD7124_ERROR_AINM_UV_MASK | ADI_AD7124_ERROR_DLDO_PSM_MASK | \
     ADI_AD7124_ERROR_ALDO_PSM_MASK | ADI_AD7124_ERROR_SPI_IGNORE_MASK | \
     ADI_AD7124_ERROR_MM_CRC_MASK | \
     ADI_AD7124_ERROR_ROM_CRC_MASK)
#define PRODUCT_ADC_ERROR_CONVERSION_MASK \
    (ADI_AD7124_ERROR_ADC_CAL_MASK | ADI_AD7124_ERROR_ADC_CONV_MASK | \
     ADI_AD7124_ERROR_ADC_SAT_MASK)
#define PRODUCT_ADC_ERROR_INPUT_MASK \
    (ADI_AD7124_ERROR_AINP_OV_MASK | ADI_AD7124_ERROR_AINP_UV_MASK | \
     ADI_AD7124_ERROR_AINM_OV_MASK | ADI_AD7124_ERROR_AINM_UV_MASK)
#define PRODUCT_ADC_ERROR_INTERNAL_MASK \
    (ADI_AD7124_ERROR_LDO_CAP_MASK | ADI_AD7124_ERROR_DLDO_PSM_MASK | \
     ADI_AD7124_ERROR_ALDO_PSM_MASK)

static uint16_t ClassifyErrorRegister(uint32_t error)
{
    uint16_t categories = PRODUCT_ADC_FAULT_NONE;
    if ((error & PRODUCT_ADC_ERROR_COMM_MASK) != 0U)
    {
        categories |= PRODUCT_ADC_FAULT_COMMUNICATION;
    }
    if ((error & PRODUCT_ADC_ERROR_INTEGRITY_MASK) != 0U)
    {
        categories |= PRODUCT_ADC_FAULT_INTEGRITY;
    }
    if ((error & ADI_AD7124_ERROR_REF_DET_MASK) != 0U)
    {
        categories |= PRODUCT_ADC_FAULT_REFERENCE;
    }
    if ((error & PRODUCT_ADC_ERROR_CONVERSION_MASK) != 0U)
    {
        categories |= PRODUCT_ADC_FAULT_CONVERSION;
    }
    if ((error & PRODUCT_ADC_ERROR_INPUT_MASK) != 0U)
    {
        categories |= PRODUCT_ADC_FAULT_INPUT_VOLTAGE;
    }
    if ((error & PRODUCT_ADC_ERROR_INTERNAL_MASK) != 0U)
    {
        categories |= PRODUCT_ADC_FAULT_INTERNAL;
    }
    return categories;
}

static void ActivateCategory(ProductAdcDriverContext_t *context,
                             uint16_t category, uint8_t streak_index,
                             uint8_t threshold)
{
    if (context->category_streak[streak_index] < UINT8_MAX)
    {
        context->category_streak[streak_index]++;
    }
    if (context->category_streak[streak_index] >= threshold)
    {
        context->diagnostics.active_fault_categories |= category;
    }
}

static uint16_t RecordErrorRegister(ProductAdcDriverContext_t *context,
                                    uint32_t error)
{
    uint16_t categories = ClassifyErrorRegister(error);
    ProductAdcDriverDiagnostics_t *diagnostics = &context->diagnostics;

    diagnostics->error_register_reads++;
    diagnostics->last_error_register = error;
    diagnostics->latched_error_register |= error;
    diagnostics->last_fault_categories = categories;
    if ((error != 0U) && (diagnostics->first_error_register == 0U))
    {
        diagnostics->first_error_register = error;
    }
    if ((categories & PRODUCT_ADC_FAULT_COMMUNICATION) != 0U)
    {
        diagnostics->communication_faults++;
        ActivateCategory(context, PRODUCT_ADC_FAULT_COMMUNICATION, 0U,
                         PRODUCT_ADC_COMM_FAULT_COUNT);
    }
    if ((categories & PRODUCT_ADC_FAULT_INTEGRITY) != 0U)
    {
        diagnostics->integrity_faults++;
        ActivateCategory(context, PRODUCT_ADC_FAULT_INTEGRITY, 1U,
                         PRODUCT_ADC_COMM_FAULT_COUNT);
    }
    if ((categories & PRODUCT_ADC_FAULT_REFERENCE) != 0U)
    {
        diagnostics->reference_faults++;
        ActivateCategory(context, PRODUCT_ADC_FAULT_REFERENCE, 2U,
                         PRODUCT_ADC_REFERENCE_FAULT_COUNT);
    }
    if ((categories & PRODUCT_ADC_FAULT_CONVERSION) != 0U)
    {
        diagnostics->conversion_faults++;
        ActivateCategory(context, PRODUCT_ADC_FAULT_CONVERSION, 3U,
                         PRODUCT_ADC_CONVERSION_FAULT_COUNT);
    }
    if ((categories & PRODUCT_ADC_FAULT_INPUT_VOLTAGE) != 0U)
    {
        diagnostics->input_voltage_faults++;
        ActivateCategory(context, PRODUCT_ADC_FAULT_INPUT_VOLTAGE, 4U,
                         PRODUCT_ADC_CONVERSION_FAULT_COUNT);
    }
    if ((categories & PRODUCT_ADC_FAULT_INTERNAL) != 0U)
    {
        diagnostics->internal_faults++;
        ActivateCategory(context, PRODUCT_ADC_FAULT_INTERNAL, 5U, 1U);
    }
    return categories;
}

static void RecordCleanSample(ProductAdcDriverContext_t *context)
{
    uint8_t index;
    context->diagnostics.consecutive_transaction_errors = 0U;
    if (context->diagnostics.consecutive_clean_samples < UINT8_MAX)
    {
        context->diagnostics.consecutive_clean_samples++;
    }
    for (index = 0U; index < 5U; index++)
    {
        context->category_streak[index] = 0U;
    }
    if (context->diagnostics.consecutive_clean_samples >=
        PRODUCT_ADC_CLEAR_GOOD_SAMPLE_COUNT)
    {
        context->diagnostics.active_fault_categories &=
            PRODUCT_ADC_FAULT_INTERNAL;
    }
}

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
    {g_setups, 1U, g_channels[0], PRODUCT_ADC_CHANNELS_PER_DEVICE,
     HAL_ADC_INPUT_MODE_VOLTAGE, 0U, PRODUCT_ADC_IEX1_AIN,
     PRODUCT_ADC_IEX2_AIN},
    {g_setups, 1U, g_channels[1], PRODUCT_ADC_CHANNELS_PER_DEVICE,
     HAL_ADC_INPUT_MODE_VOLTAGE, 0U, PRODUCT_ADC_IEX1_AIN,
     PRODUCT_ADC_IEX2_AIN},
    {g_setups, 1U, g_channels[2], PRODUCT_ADC_CHANNELS_PER_DEVICE,
     HAL_ADC_INPUT_MODE_VOLTAGE, 0U, PRODUCT_ADC_IEX1_AIN,
     PRODUCT_ADC_IEX2_AIN},
    {g_setups, 1U, g_channels[3], PRODUCT_ADC_CHANNELS_PER_DEVICE,
     HAL_ADC_INPUT_MODE_VOLTAGE, 0U, PRODUCT_ADC_IEX1_AIN,
     PRODUCT_ADC_IEX2_AIN}
};

typedef struct
{
    uint8_t port;
    uint8_t pin;
} ProductAdcCvSelect_t;

static const ProductAdcCvSelect_t g_cv_select[HAL_ADC_DEVICE_COUNT] =
{
    {BOARD_INITEXTADCPINS_CV_SEL_0_PORT,
     BOARD_INITEXTADCPINS_CV_SEL_0_PIN},
    {BOARD_INITEXTADCPINS_CV_SEL_1_PORT,
     BOARD_INITEXTADCPINS_CV_SEL_1_PIN},
    {BOARD_INITEXTADCPINS_CV_SEL_2_PORT,
     BOARD_INITEXTADCPINS_CV_SEL_2_PIN},
    {BOARD_INITEXTADCPINS_CV_SEL_3_PORT,
     BOARD_INITEXTADCPINS_CV_SEL_3_PIN}
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

static void RecordDriverStatus(
    ProductAdcDriverContext_t *context, adi_ad7124_status_t status)
{
    context->diagnostics.last_driver_status = (int32_t)status;
    if (status == kAdiAd7124_NotReady)
    {
        context->diagnostics.not_ready_polls++;
    }
    else if (status == kAdiAd7124_CrcError)
    {
        context->diagnostics.crc_errors++;
        context->diagnostics.integrity_faults++;
        if (context->diagnostics.consecutive_transaction_errors < UINT8_MAX)
        {
            context->diagnostics.consecutive_transaction_errors++;
        }
        if (context->diagnostics.consecutive_transaction_errors >=
            PRODUCT_ADC_COMM_FAULT_COUNT)
        {
            context->diagnostics.active_fault_categories |=
                PRODUCT_ADC_FAULT_INTEGRITY;
        }
    }
    else if ((status == kAdiAd7124_TransportError) ||
             (status == kAdiAd7124_Timeout))
    {
        context->diagnostics.transport_errors++;
        if (context->diagnostics.consecutive_transaction_errors < UINT8_MAX)
        {
            context->diagnostics.consecutive_transaction_errors++;
        }
        if (context->diagnostics.consecutive_transaction_errors >=
            PRODUCT_ADC_COMM_FAULT_COUNT)
        {
            context->diagnostics.active_fault_categories |=
                PRODUCT_ADC_FAULT_COMMUNICATION;
        }
    }
    else if (status != kAdiAd7124_Ok)
    {
        context->diagnostics.device_errors++;
    }
}

static HalAdcStatus_t ProductAdcInitialize(void *driver_context)
{
    ProductAdcDriverContext_t *context =
        (ProductAdcDriverContext_t *)driver_context;
    adi_ad7124_status_t status;
    adi_ad7124_device_t *device;

    if (context == NULL)
    {
        return HAL_ADC_STATUS_INVALID_ARGUMENT;
    }
    context->diagnostics.initialization_attempts++;
    status = ProductAd7124_InitDevice(context->device_index);
    RecordDriverStatus(context, status);
    device = ProductAd7124_GetDevice(context->device_index);
    context->diagnostics.initialized = (status == kAdiAd7124_Ok);
    context->diagnostics.device_id =
        (device != NULL) ? device->deviceId : 0U;
    context->diagnostics.initial_error_register =
        (device != NULL) ? device->initialError : 0U;
    if (status == kAdiAd7124_Ok)
    {
        (void)memset(context->category_streak, 0,
                     sizeof(context->category_streak));
        context->diagnostics.active_fault_categories = 0U;
        context->diagnostics.consecutive_transaction_errors = 0U;
        context->diagnostics.consecutive_clean_samples = 0U;
        (void)RecordErrorRegister(context, device->initialError);
    }
    return MapDriverStatus(status);
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
    adi_ad7124_io_config_t ioConfig;
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
    ioConfig.excitationCurrentUa = config->excitation_current_ua;
    ioConfig.excitationOutput0 = config->excitation_output0;
    ioConfig.excitationOutput1 = config->excitation_output1;
    {
        adi_ad7124_status_t driverStatus = ADI_AD7124_Configure(
            ProductAd7124_GetDevice(context->device_index), setups,
            config->setup_count, channels, config->channel_count, &ioConfig);
        HalAdcStatus_t status = MapDriverStatus(driverStatus);
        RecordDriverStatus(context, driverStatus);
        if (status == HAL_ADC_STATUS_OK)
        {
            uint32_t error_enable = PRODUCT_ADC_ERROR_ENABLE_COMMON_MASK;
            uint32_t error_enable_verify = 0U;
            bool external_reference = false;
            for (index = 0U; index < config->setup_count; index++)
            {
                if ((config->setups[index].reference ==
                     HAL_ADC_REFERENCE_EXTERNAL_1) ||
                    (config->setups[index].reference ==
                     HAL_ADC_REFERENCE_EXTERNAL_2))
                {
                    external_reference = true;
                    break;
                }
            }
            if (external_reference)
            {
                error_enable |= ADI_AD7124_ERROR_REF_DET_MASK;
            }
#if PRODUCT_ADC_SPI_CRC_ENABLED
            error_enable |= ADI_AD7124_ERROR_SPI_CRC_MASK;
#endif
            driverStatus = ADI_AD7124_WriteRegister(
                ProductAd7124_GetDevice(context->device_index),
                ADI_AD7124_ERROR_ENABLE_REG, error_enable);
            RecordDriverStatus(context, driverStatus);
            status = MapDriverStatus(driverStatus);
            if (status == HAL_ADC_STATUS_OK)
            {
                driverStatus = ADI_AD7124_ReadRegister(
                    ProductAd7124_GetDevice(context->device_index),
                    ADI_AD7124_ERROR_ENABLE_REG, &error_enable_verify);
                RecordDriverStatus(context, driverStatus);
                status = MapDriverStatus(driverStatus);
                if ((status == HAL_ADC_STATUS_OK) &&
                    ((error_enable_verify & error_enable) != error_enable))
                {
                    status = HAL_ADC_STATUS_DEVICE_ERROR;
                }
            }
        }
        if (status == HAL_ADC_STATUS_OK)
        {
            GPIO_PinWrite(GPIO,
                          g_cv_select[context->device_index].port,
                          g_cv_select[context->device_index].pin,
                          (config->input_mode == HAL_ADC_INPUT_MODE_CURRENT) ?
                              PRODUCT_ADC_CV_SELECT_CURRENT :
                              PRODUCT_ADC_CV_SELECT_VOLTAGE);
            context->discard_next_sample = true;
            context->active_config = config;
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
    uint32_t error_register = 0U;
    uint8_t status_register = 0U;
    bool error_register_read = false;
    uint16_t error_categories = PRODUCT_ADC_FAULT_NONE;
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

    status = ADI_AD7124_TryReadDataDiagnostic(
        device, &sample->raw_code, &sample->channel, &status_register,
        &error_register, &error_register_read);
    context->diagnostics.last_status_register = status_register;
    RecordDriverStatus(context, status);
    if ((status_register & ADI_AD7124_STATUS_POR_MASK) != 0U)
    {
        context->diagnostics.unexpected_por_faults++;
        context->diagnostics.active_fault_categories |=
            PRODUCT_ADC_FAULT_INTERNAL;
        error_categories |= PRODUCT_ADC_FAULT_INTERNAL;
    }
    if (error_register_read)
    {
        error_categories |= RecordErrorRegister(context, error_register);
    }
    if (status != kAdiAd7124_Ok)
    {
        return MapDriverStatus(status);
    }
    if (!error_register_read &&
        ((sample->raw_code == 0U) ||
         (sample->raw_code == 0x00FFFFFFUL)))
    {
        status = ADI_AD7124_ReadRegister(
            device, ADI_AD7124_ERROR_REG, &error_register);
        RecordDriverStatus(context, status);
        if (status == kAdiAd7124_Ok)
        {
            error_categories |= RecordErrorRegister(context, error_register);
        }
        else
        {
            context->diagnostics.error_register_read_failures++;
        }
    }
    if (error_categories != PRODUCT_ADC_FAULT_NONE)
    {
        context->diagnostics.consecutive_clean_samples = 0U;
        context->diagnostics.discarded_samples++;
        return HAL_ADC_STATUS_NOT_READY;
    }
    if (context->discard_next_sample)
    {
        context->discard_next_sample = false;
        context->diagnostics.discarded_samples++;
        return HAL_ADC_STATUS_NOT_READY;
    }
    if (context->active_config == NULL)
    {
        return HAL_ADC_STATUS_NOT_INITIALIZED;
    }
    for (index = 0U;
         index < context->active_config->channel_count;
         index++)
    {
        if (context->active_config->channels[index].channel ==
            sample->channel)
        {
            channel_config =
                &context->active_config->channels[index];
            break;
        }
    }
    if (channel_config == NULL)
    {
        return HAL_ADC_STATUS_DEVICE_ERROR;
    }
    setup = &context->active_config->setups[channel_config->setup];
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
    context->diagnostics.successful_samples++;
    RecordCleanSample(context);
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

    g_next_audit_device = 0U;
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

bool ProductAdcDriver_AuditNextDevice(void)
{
    ProductAdcDriverContext_t *context;
    adi_ad7124_device_t *device;
    adi_ad7124_status_t status;
    uint32_t error_register;

    context = &g_adc_context[g_next_audit_device];
    device = ProductAd7124_GetDevice(g_next_audit_device);
    g_next_audit_device =
        (uint8_t)((g_next_audit_device + 1U) % HAL_ADC_DEVICE_COUNT);
    if ((device == NULL) || !device->initialized)
    {
        return false;
    }
    status = ADI_AD7124_ReadRegister(
        device, ADI_AD7124_ERROR_REG, &error_register);
    RecordDriverStatus(context, status);
    if (status != kAdiAd7124_Ok)
    {
        context->diagnostics.error_register_read_failures++;
        return false;
    }
    (void)RecordErrorRegister(context, error_register);
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

bool ProductAdcDriver_GetDiagnostics(
    uint8_t device, ProductAdcDriverDiagnostics_t *diagnostics)
{
    if ((device >= HAL_ADC_DEVICE_COUNT) || (diagnostics == NULL))
    {
        return false;
    }
    *diagnostics = g_adc_context[device].diagnostics;
    return true;
}
