#include "product_internal_adc_driver.h"

#include <stddef.h>
#include <string.h>

#include "ProductInternalAdcConfig.h"
#include "fsl_common.h"
#include "fsl_lpadc.h"
#include "fsl_power.h"
#include "peripherals.h"

#define PRODUCT_INTERNAL_ADC_FIFO_CJC              (0U)
#define PRODUCT_INTERNAL_ADC_FIFO_MCU_TEMPERATURE  (1U)
#define PRODUCT_INTERNAL_ADC_TEMP_RESULT_SHIFT     (3U)
#define PRODUCT_INTERNAL_ADC_REQUEST_CJC0          (1U << 0)
#define PRODUCT_INTERNAL_ADC_REQUEST_CJC1          (1U << 1)
#define PRODUCT_INTERNAL_ADC_REQUEST_MCU_TEMP      (1U << 2)

typedef struct
{
    volatile uint16_t cjc_raw[PRODUCT_INTERNAL_ADC_CJC_COUNT];
    volatile uint8_t cjc_pending_mask;
    volatile uint16_t mcu_raw[2];
    volatile uint8_t mcu_result_count;
    volatile bool mcu_pending;
    volatile uint8_t requested_conversions;
    volatile bool conversion_active;
    ProductInternalAdcSnapshot_t snapshot;
    bool initialized;
} ProductInternalAdcContext_t;

static ProductInternalAdcContext_t g_internal_adc;

static void ProductInternalAdcStartNextConversion(void)
{
    uint32_t trigger_mask = 0U;
    uint32_t interrupt_mask = DisableGlobalIRQ();

    if (!g_internal_adc.conversion_active)
    {
        if ((g_internal_adc.requested_conversions &
             PRODUCT_INTERNAL_ADC_REQUEST_CJC0) != 0U)
        {
            g_internal_adc.requested_conversions &=
                (uint8_t)~PRODUCT_INTERNAL_ADC_REQUEST_CJC0;
            trigger_mask = 1UL << CJC_ADC0_CJC0_TRIG;
        }
        else if ((g_internal_adc.requested_conversions &
                  PRODUCT_INTERNAL_ADC_REQUEST_CJC1) != 0U)
        {
            g_internal_adc.requested_conversions &=
                (uint8_t)~PRODUCT_INTERNAL_ADC_REQUEST_CJC1;
            trigger_mask = 1UL << CJC_ADC0_CJC1_TRIG;
        }
        else if ((g_internal_adc.requested_conversions &
                  PRODUCT_INTERNAL_ADC_REQUEST_MCU_TEMP) != 0U)
        {
            g_internal_adc.requested_conversions &=
                (uint8_t)~PRODUCT_INTERNAL_ADC_REQUEST_MCU_TEMP;
            g_internal_adc.mcu_result_count = 0U;
            trigger_mask = 1UL << CJC_ADC0_CHIPTEMP_TRIG;
        }
        if (trigger_mask != 0U)
        {
            g_internal_adc.conversion_active = true;
        }
    }
    EnableGlobalIRQ(interrupt_mask);

    if (trigger_mask != 0U)
    {
        LPADC_DoSoftwareTrigger(CJC_ADC0_PERIPHERAL, trigger_mask);
    }
}

static uint32_t ProductInternalAdcCodeToMicrovolts(uint16_t raw_code)
{
    return (uint32_t)((((uint64_t)raw_code *
                        PRODUCT_INTERNAL_ADC_REFERENCE_UV) +
                       (PRODUCT_INTERNAL_ADC_FULL_SCALE_CODE / 2UL)) /
                      PRODUCT_INTERNAL_ADC_FULL_SCALE_CODE);
}

static int32_t ProductTmp20MicrovoltsToCentiC(uint32_t microvolts)
{
    int64_t numerator =
        ((int64_t)PRODUCT_TMP20_ZERO_DEGREE_UV - (int64_t)microvolts) *
        100LL;

    if (numerator >= 0LL)
    {
        numerator += PRODUCT_TMP20_SLOPE_UV_PER_DEGREE / 2L;
    }
    else
    {
        numerator -= PRODUCT_TMP20_SLOPE_UV_PER_DEGREE / 2L;
    }
    return (int32_t)(numerator / PRODUCT_TMP20_SLOPE_UV_PER_DEGREE);
}

static bool ProductInternalAdcCalculateMcuTemperature(
    uint16_t raw_vbe1, uint16_t raw_vbe8, int32_t *temperature_centi_c)
{
    float vbe1;
    float vbe8;
    float delta;
    float denominator;
    float temperature;

    if (temperature_centi_c == NULL)
    {
        return false;
    }

    vbe1 = (float)(raw_vbe1 >> PRODUCT_INTERNAL_ADC_TEMP_RESULT_SHIFT);
    vbe8 = (float)(raw_vbe8 >> PRODUCT_INTERNAL_ADC_TEMP_RESULT_SHIFT);
    delta = vbe8 - vbe1;
    denominator = vbe8 +
        (FSL_FEATURE_LPADC_TEMP_PARAMETER_ALPHA * delta);
    if ((denominator > -0.0001F) && (denominator < 0.0001F))
    {
        return false;
    }

    /* NXP formula: A*[alpha*(VBE8-VBE1)/(VBE8+alpha*delta)]-B. */
    temperature =
        FSL_FEATURE_LPADC_TEMP_PARAMETER_A *
        (FSL_FEATURE_LPADC_TEMP_PARAMETER_ALPHA * delta / denominator) -
        FSL_FEATURE_LPADC_TEMP_PARAMETER_B;
    if ((temperature < -273.15F) || (temperature > 200.0F))
    {
        return false;
    }
    *temperature_centi_c = (int32_t)(temperature * 100.0F +
        ((temperature >= 0.0F) ? 0.5F : -0.5F));
    return true;
}

bool ProductInternalAdcDriver_Init(void)
{
    uint32_t interrupt_mask = DisableGlobalIRQ();

    (void)memset(&g_internal_adc, 0, sizeof(g_internal_adc));
    g_internal_adc.initialized = true;
    EnableGlobalIRQ(interrupt_mask);

    /* Wizard initializes LPADC/commands. L01 only powers the internal sensor. */
    POWER_DisablePD(kPDRUNCFG_PD_TEMPSENS);
    return true;
}

bool ProductInternalAdcDriver_RequestCjcSamples(void)
{
    uint32_t interrupt_mask;

    if (!g_internal_adc.initialized)
    {
        return false;
    }
    interrupt_mask = DisableGlobalIRQ();
    g_internal_adc.requested_conversions |=
        PRODUCT_INTERNAL_ADC_REQUEST_CJC0 |
        PRODUCT_INTERNAL_ADC_REQUEST_CJC1;
    EnableGlobalIRQ(interrupt_mask);
    return true;
}

bool ProductInternalAdcDriver_RequestMcuTemperature(void)
{
    uint32_t interrupt_mask;

    if (!g_internal_adc.initialized)
    {
        return false;
    }
    interrupt_mask = DisableGlobalIRQ();
    g_internal_adc.requested_conversions |=
        PRODUCT_INTERNAL_ADC_REQUEST_MCU_TEMP;
    EnableGlobalIRQ(interrupt_mask);
    return true;
}

void ProductInternalAdcDriver_Process(void)
{
    uint16_t cjc_raw[PRODUCT_INTERNAL_ADC_CJC_COUNT];
    uint16_t mcu_raw[2];
    uint8_t cjc_pending_mask;
    bool mcu_pending;
    uint32_t interrupt_mask = DisableGlobalIRQ();

    cjc_raw[0] = g_internal_adc.cjc_raw[0];
    cjc_raw[1] = g_internal_adc.cjc_raw[1];
    cjc_pending_mask = g_internal_adc.cjc_pending_mask;
    g_internal_adc.cjc_pending_mask = 0U;
    mcu_raw[0] = g_internal_adc.mcu_raw[0];
    mcu_raw[1] = g_internal_adc.mcu_raw[1];
    mcu_pending = g_internal_adc.mcu_pending;
    g_internal_adc.mcu_pending = false;
    EnableGlobalIRQ(interrupt_mask);

    for (uint8_t index = 0U;
         index < PRODUCT_INTERNAL_ADC_CJC_COUNT; index++)
    {
        if ((cjc_pending_mask & (1U << index)) != 0U)
        {
            ProductCjcSample_t *sample = &g_internal_adc.snapshot.cjc[index];
            sample->raw_code = cjc_raw[index];
            sample->microvolts =
                ProductInternalAdcCodeToMicrovolts(cjc_raw[index]);
            sample->temperature_centi_c =
                ProductTmp20MicrovoltsToCentiC(sample->microvolts);
            sample->out_of_range =
                (sample->temperature_centi_c <
                    PRODUCT_TMP20_MIN_TEMPERATURE_CENTI_C) ||
                (sample->temperature_centi_c >
                    PRODUCT_TMP20_MAX_TEMPERATURE_CENTI_C);
            sample->valid = !sample->out_of_range;
            sample->sequence++;
        }
    }

    if (mcu_pending)
    {
        ProductMcuTemperatureSample_t *sample =
            &g_internal_adc.snapshot.mcu_temperature;
        int32_t temperature_centi_c;

        sample->raw_vbe1 = mcu_raw[0];
        sample->raw_vbe8 = mcu_raw[1];
        sample->valid = ProductInternalAdcCalculateMcuTemperature(
            mcu_raw[0], mcu_raw[1], &temperature_centi_c);
        if (sample->valid)
        {
            sample->temperature_centi_c = temperature_centi_c;
            sample->sequence++;
            if (temperature_centi_c >=
                PRODUCT_MCU_OVERTEMPERATURE_CENTI_C)
            {
                if (sample->overtemperature_count <
                    PRODUCT_MCU_OVERTEMPERATURE_CONFIRM_COUNT)
                {
                    sample->overtemperature_count++;
                }
            }
            else
            {
                sample->overtemperature_count = 0U;
            }
            sample->overtemperature =
                sample->overtemperature_count >=
                PRODUCT_MCU_OVERTEMPERATURE_CONFIRM_COUNT;
        }
    }
    ProductInternalAdcStartNextConversion();
}

bool ProductInternalAdcDriver_GetSnapshot(
    ProductInternalAdcSnapshot_t *snapshot)
{
    uint32_t interrupt_mask;

    if ((snapshot == NULL) || !g_internal_adc.initialized)
    {
        return false;
    }
    interrupt_mask = DisableGlobalIRQ();
    *snapshot = g_internal_adc.snapshot;
    EnableGlobalIRQ(interrupt_mask);
    return true;
}

void CJC_ADC0_IRQHANDLER(void)
{
    lpadc_conv_result_t result;
    uint32_t status = LPADC_GetStatusFlags(CJC_ADC0_PERIPHERAL);

    if ((status & kLPADC_ResultFIFO0OverflowFlag) != 0U)
    {
        g_internal_adc.snapshot.fifo0_overflows++;
        LPADC_ClearStatusFlags(
            CJC_ADC0_PERIPHERAL, kLPADC_ResultFIFO0OverflowFlag);
    }
    if ((status & kLPADC_ResultFIFO1OverflowFlag) != 0U)
    {
        g_internal_adc.snapshot.fifo1_overflows++;
        LPADC_ClearStatusFlags(
            CJC_ADC0_PERIPHERAL, kLPADC_ResultFIFO1OverflowFlag);
    }

    while (LPADC_GetConvResult(
        CJC_ADC0_PERIPHERAL, &result, PRODUCT_INTERNAL_ADC_FIFO_CJC))
    {
        if (result.commandIdSource == CJC_ADC0_CJC0_CMD)
        {
            g_internal_adc.cjc_raw[0] = result.convValue;
            g_internal_adc.cjc_pending_mask |= 1U;
            g_internal_adc.conversion_active = false;
        }
        else if (result.commandIdSource == CJC_ADC0_CJC1_CMD)
        {
            g_internal_adc.cjc_raw[1] = result.convValue;
            g_internal_adc.cjc_pending_mask |= 2U;
            g_internal_adc.conversion_active = false;
        }
        else
        {
            g_internal_adc.snapshot.unexpected_results++;
            g_internal_adc.conversion_active = false;
        }
    }

    while (LPADC_GetConvResult(
        CJC_ADC0_PERIPHERAL, &result,
        PRODUCT_INTERNAL_ADC_FIFO_MCU_TEMPERATURE))
    {
        if ((result.commandIdSource == CJC_ADC0_CHIPTEMP_CMD) &&
            (g_internal_adc.mcu_result_count < 2U))
        {
            g_internal_adc.mcu_raw[g_internal_adc.mcu_result_count] =
                result.convValue;
            g_internal_adc.mcu_result_count++;
            if (g_internal_adc.mcu_result_count == 2U)
            {
                g_internal_adc.mcu_pending = true;
                g_internal_adc.conversion_active = false;
            }
        }
        else
        {
            g_internal_adc.snapshot.unexpected_results++;
            g_internal_adc.conversion_active = false;
        }
    }
}
