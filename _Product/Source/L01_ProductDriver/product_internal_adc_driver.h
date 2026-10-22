#ifndef PRODUCT_INTERNAL_ADC_DRIVER_H
#define PRODUCT_INTERNAL_ADC_DRIVER_H

#include <stdbool.h>
#include <stdint.h>

#if defined(PRODUCT_SAFETY_TEST_BUILD) && \
    !defined(PRODUCT_HOST_TEST) && !defined(PRODUCT_SAFETY_TEST_TARGET)
#error "Safety test API requires a Host test or dedicated target test build"
#endif

#define PRODUCT_INTERNAL_ADC_CJC_COUNT (2U)

typedef struct
{
    uint16_t raw_code;
    uint32_t microvolts;
    int32_t temperature_centi_c;
    uint32_t sequence;
    bool valid;
    bool out_of_range;
} ProductCjcSample_t;

typedef struct
{
    uint16_t raw_vbe1;
    uint16_t raw_vbe8;
    int32_t temperature_centi_c;
    uint32_t sequence;
    uint8_t overtemperature_count;
    bool valid;
    bool overtemperature;
} ProductMcuTemperatureSample_t;

typedef struct
{
    ProductCjcSample_t cjc[PRODUCT_INTERNAL_ADC_CJC_COUNT];
    ProductMcuTemperatureSample_t mcu_temperature;
    uint32_t fifo0_overflows;
    uint32_t fifo1_overflows;
    uint32_t unexpected_results;
} ProductInternalAdcSnapshot_t;

/* BOARD_InitBootPeripherals() owns LPADC initialization and configuration. */
bool ProductInternalAdcDriver_Init(void);
bool ProductInternalAdcDriver_RequestCjcSamples(void);
bool ProductInternalAdcDriver_RequestMcuTemperature(void);
void ProductInternalAdcDriver_Process(void);
bool ProductInternalAdcDriver_GetSnapshot(
    ProductInternalAdcSnapshot_t *snapshot);

#if defined(PRODUCT_SAFETY_TEST_BUILD)
/* Test-build only: the normal 1 s request cadence consumes this value. */
bool ProductInternalAdcDriver_SetMcuTemperatureTestOverride(
    bool enabled,
    bool sample_valid,
    int32_t temperature_centi_c);
#endif

#endif /* PRODUCT_INTERNAL_ADC_DRIVER_H */
