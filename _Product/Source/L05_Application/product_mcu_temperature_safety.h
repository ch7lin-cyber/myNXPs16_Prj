#ifndef PRODUCT_MCU_TEMPERATURE_SAFETY_H
#define PRODUCT_MCU_TEMPERATURE_SAFETY_H

#include <stdbool.h>
#include <stdint.h>

void ProductMcuTemperatureSafety_Initialize(void);
void ProductMcuTemperatureSafety_Process(
    bool sample_valid,
    bool overtemperature,
    int32_t temperature_centi_c);

#endif /* PRODUCT_MCU_TEMPERATURE_SAFETY_H */
