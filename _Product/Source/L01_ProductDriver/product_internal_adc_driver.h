#ifndef PRODUCT_INTERNAL_ADC_DRIVER_H
#define PRODUCT_INTERNAL_ADC_DRIVER_H

#include <stdbool.h>
#include <stdint.h>

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

#endif /* PRODUCT_INTERNAL_ADC_DRIVER_H */
