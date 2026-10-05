#ifndef PRODUCT_ROTARY_SWITCH_DRIVER_H
#define PRODUCT_ROTARY_SWITCH_DRIVER_H

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    PRODUCT_ROTARY_SWITCH_STATUS_READY = 0U,
    PRODUCT_ROTARY_SWITCH_STATUS_NOT_INITIALIZED = 2U
} ProductRotarySwitchStatus_t;

typedef struct
{
    uint8_t position;
    uint8_t raw_value;
    uint16_t revision;
    ProductRotarySwitchStatus_t status;
} ProductRotarySwitchSnapshot_t;

void ProductRotarySwitchDriver_Initialize(void);
void ProductRotarySwitchDriver_Process(void);
bool ProductRotarySwitchDriver_GetSnapshot(
    ProductRotarySwitchSnapshot_t *snapshot);

#endif
