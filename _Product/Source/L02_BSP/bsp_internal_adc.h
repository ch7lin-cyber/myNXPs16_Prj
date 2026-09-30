#ifndef BSP_INTERNAL_ADC_H
#define BSP_INTERNAL_ADC_H

#include <stdbool.h>
#include <stdint.h>

#define BSP_INTERNAL_ADC_CJC_COUNT (2U)

typedef struct
{
    int32_t temperature_centi_c;
    bool valid;
    bool overtemperature;
} BspInternalAdcMcuTemperature_t;

bool BspInternalAdc_Initialize(void);
bool BspInternalAdc_RequestCjcSamples(void);
bool BspInternalAdc_RequestMcuTemperature(void);
void BspInternalAdc_Process(void);
bool BspInternalAdc_GetCjcTemperature(
    uint8_t cjc,
    int32_t *temperature_centi_c);
bool BspInternalAdc_GetMcuTemperature(
    BspInternalAdcMcuTemperature_t *temperature);

#endif /* BSP_INTERNAL_ADC_H */
