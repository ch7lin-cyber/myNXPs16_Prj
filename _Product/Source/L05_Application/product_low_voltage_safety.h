#ifndef PRODUCT_LOW_VOLTAGE_SAFETY_H
#define PRODUCT_LOW_VOLTAGE_SAFETY_H

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    uint32_t interrupt_count;
    uint16_t revision;
    bool raw_active;
    bool confirmed_active;
    bool fault_active;
} ProductLowVoltageSnapshot_t;

void ProductLowVoltageSafety_Initialize(void);
void ProductLowVoltageSafety_Process(uint32_t timestamp_ms);
bool ProductLowVoltageSafety_GetSnapshot(ProductLowVoltageSnapshot_t *snapshot);

#endif
