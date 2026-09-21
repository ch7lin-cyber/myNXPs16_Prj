#ifndef PRODUCT_ADC_DRIVER_H
#define PRODUCT_ADC_DRIVER_H

#include <stdbool.h>

/* Register all product ADC devices with the shared generic HAL. */
bool ProductAdcDriver_Init(void);

#endif
