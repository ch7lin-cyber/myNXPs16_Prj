#include "bsp_internal_adc.h"

#include <stddef.h>

#include "product_internal_adc_driver.h"

bool BspInternalAdc_Initialize(void)
{
    return ProductInternalAdcDriver_Init();
}

bool BspInternalAdc_RequestCjcSamples(void)
{
    return ProductInternalAdcDriver_RequestCjcSamples();
}

bool BspInternalAdc_RequestMcuTemperature(void)
{
    return ProductInternalAdcDriver_RequestMcuTemperature();
}

void BspInternalAdc_Process(void)
{
    ProductInternalAdcDriver_Process();
}

bool BspInternalAdc_GetCjcTemperature(
    uint8_t cjc,
    int32_t *temperature_centi_c)
{
    ProductInternalAdcSnapshot_t snapshot;

    if ((cjc >= BSP_INTERNAL_ADC_CJC_COUNT) ||
        (temperature_centi_c == NULL) ||
        !ProductInternalAdcDriver_GetSnapshot(&snapshot) ||
        !snapshot.cjc[cjc].valid)
    {
        return false;
    }
    *temperature_centi_c = snapshot.cjc[cjc].temperature_centi_c;
    return true;
}

bool BspInternalAdc_GetMcuTemperature(
    BspInternalAdcMcuTemperature_t *temperature)
{
    ProductInternalAdcSnapshot_t snapshot;

    if ((temperature == NULL) ||
        !ProductInternalAdcDriver_GetSnapshot(&snapshot))
    {
        return false;
    }
    temperature->temperature_centi_c =
        snapshot.mcu_temperature.temperature_centi_c;
    temperature->valid = snapshot.mcu_temperature.valid;
    temperature->overtemperature =
        snapshot.mcu_temperature.overtemperature;
    return true;
}
