#ifndef PRODUCT_DIP_SWITCH_DRIVER_H
#define PRODUCT_DIP_SWITCH_DRIVER_H

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    PRODUCT_DIP_SWITCH_STATUS_READY = 0U,
    PRODUCT_DIP_SWITCH_STATUS_IO_ERROR = 1U,
    PRODUCT_DIP_SWITCH_STATUS_NOT_INITIALIZED = 2U
} ProductDipSwitchStatus_t;

typedef struct
{
    uint8_t logical_mask;
    uint8_t raw_value;
    uint16_t revision;
    ProductDipSwitchStatus_t status;
} ProductDipSwitchSnapshot_t;

/* Reset the retained sample; the first Process performs an immediate read. */
void ProductDipSwitchDriver_Initialize(void);

/* Read U5 once over FLEXCOMM8. The last valid value is retained on failure. */
bool ProductDipSwitchDriver_Process(void);

bool ProductDipSwitchDriver_GetSnapshot(ProductDipSwitchSnapshot_t *snapshot);

#endif
