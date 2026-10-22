#include <assert.h>
#include <stdint.h>

#include "fsl_lpadc.h"
#include "product_internal_adc_driver.h"

LPADC_Type g_mock_lpadc;

uint32_t DisableGlobalIRQ(void) { return 0U; }
void EnableGlobalIRQ(uint32_t primask) { (void)primask; }
void POWER_DisablePD(uint32_t mask) { (void)mask; }
void LPADC_DoSoftwareTrigger(LPADC_Type *base, uint32_t triggerMask)
{
    (void)base;
    (void)triggerMask;
}
uint32_t LPADC_GetStatusFlags(LPADC_Type *base)
{
    (void)base;
    return 0U;
}
void LPADC_ClearStatusFlags(LPADC_Type *base, uint32_t mask)
{
    (void)base;
    (void)mask;
}
bool LPADC_GetConvResult(LPADC_Type *base, lpadc_conv_result_t *result,
                         uint32_t fifoIndex)
{
    (void)base;
    (void)result;
    (void)fifoIndex;
    return false;
}

static ProductMcuTemperatureSample_t Sample(void)
{
    ProductInternalAdcSnapshot_t snapshot;
    assert(ProductInternalAdcDriver_GetSnapshot(&snapshot));
    return snapshot.mcu_temperature;
}

static void SubmitSample(int32_t centi_c)
{
    assert(ProductInternalAdcDriver_SetMcuTemperatureTestOverride(
        true, true, centi_c));
    assert(ProductInternalAdcDriver_RequestMcuTemperature());
    ProductInternalAdcDriver_Process();
}

int main(void)
{
    uint32_t sequence_before_disable;
    assert(!ProductInternalAdcDriver_SetMcuTemperatureTestOverride(
        true, true, 8500));
    assert(ProductInternalAdcDriver_Init());

    SubmitSample(8499);
    assert(Sample().valid && Sample().overtemperature_count == 0U);
    SubmitSample(8500);
    assert(Sample().overtemperature_count == 1U);
    SubmitSample(8500);
    assert(Sample().overtemperature_count == 2U);
    assert(!Sample().overtemperature);
    SubmitSample(8500);
    assert(Sample().overtemperature_count == 3U);
    assert(Sample().overtemperature);

    SubmitSample(8499);
    assert(Sample().overtemperature_count == 0U);
    assert(!Sample().overtemperature);

    SubmitSample(8500);
    SubmitSample(8500);
    assert(Sample().overtemperature_count == 2U);
    assert(ProductInternalAdcDriver_SetMcuTemperatureTestOverride(
        true, false, 8500));
    assert(ProductInternalAdcDriver_RequestMcuTemperature());
    ProductInternalAdcDriver_Process();
    assert(!Sample().valid);
    assert(Sample().overtemperature_count == 2U);
    SubmitSample(8500);
    assert(Sample().overtemperature);

    sequence_before_disable = Sample().sequence;
    assert(ProductInternalAdcDriver_SetMcuTemperatureTestOverride(
        false, false, 0));
    assert(!Sample().valid);
    assert(Sample().overtemperature_count == 0U);
    assert(!Sample().overtemperature);
    ProductInternalAdcDriver_Process();
    assert(Sample().sequence == sequence_before_disable);
    return 0;
}
