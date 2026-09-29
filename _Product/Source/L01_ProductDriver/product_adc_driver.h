#ifndef PRODUCT_ADC_DRIVER_H
#define PRODUCT_ADC_DRIVER_H

#include <stdbool.h>
#include <stdint.h>

#include "AnalogInputService.h"
#include "HalAdc.h"

/* Register all product ADC devices with the shared generic HAL. */
typedef struct
{
    uint32_t initialization_attempts;
    uint32_t successful_samples;
    uint32_t discarded_samples;
    uint32_t not_ready_polls;
    uint32_t crc_errors;
    uint32_t transport_errors;
    uint32_t device_errors;
    int32_t last_driver_status;
    uint8_t device_id;
    bool initialized;
} ProductAdcDriverDiagnostics_t;

bool ProductAdcDriver_Init(void);
const HalAdcDeviceConfig_t *ProductAdcDriver_GetDeviceConfig(uint8_t device);
const AnalogInputRoute_t *ProductAdcDriver_GetRoutes(uint8_t *route_count);
bool ProductAdcDriver_GetDiagnostics(
    uint8_t device, ProductAdcDriverDiagnostics_t *diagnostics);

#endif
