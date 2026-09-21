#ifndef PRODUCT_ADC_DRIVER_H
#define PRODUCT_ADC_DRIVER_H

#include <stdbool.h>

#include "AnalogInputService.h"
#include "HalAdc.h"

/* Register all product ADC devices with the shared generic HAL. */
bool ProductAdcDriver_Init(void);
const HalAdcDeviceConfig_t *ProductAdcDriver_GetDeviceConfig(uint8_t device);
const AnalogInputRoute_t *ProductAdcDriver_GetRoutes(uint8_t *route_count);

#endif
