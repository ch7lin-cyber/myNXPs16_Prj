#ifndef PRODUCT_LOW_VOLTAGE_DRIVER_H
#define PRODUCT_LOW_VOLTAGE_DRIVER_H

#include <stdbool.h>
#include <stdint.h>

void ProductLowVoltageDriver_Initialize(void);
bool ProductLowVoltageDriver_IsActive(void);
void ProductLowVoltageDriver_NotifyInterrupt(void);
uint32_t ProductLowVoltageDriver_GetInterruptCount(void);

#endif
