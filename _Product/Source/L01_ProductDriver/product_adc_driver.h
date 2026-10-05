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
    uint32_t initial_error_register;
    uint32_t first_error_register;
    uint32_t last_error_register;
    uint32_t latched_error_register;
    uint32_t error_register_reads;
    uint32_t error_register_read_failures;
    uint32_t communication_faults;
    uint32_t integrity_faults;
    uint32_t reference_faults;
    uint32_t conversion_faults;
    uint32_t input_voltage_faults;
    uint32_t internal_faults;
    uint32_t unexpected_por_faults;
    int32_t last_driver_status;
    uint16_t active_fault_categories;
    uint16_t last_fault_categories;
    uint8_t last_status_register;
    uint8_t consecutive_transaction_errors;
    uint8_t consecutive_clean_samples;
    uint8_t device_id;
    bool initialized;
} ProductAdcDriverDiagnostics_t;

typedef enum
{
    PRODUCT_ADC_FAULT_NONE = 0U,
    PRODUCT_ADC_FAULT_COMMUNICATION = (1U << 0U),
    PRODUCT_ADC_FAULT_INTEGRITY = (1U << 1U),
    PRODUCT_ADC_FAULT_REFERENCE = (1U << 2U),
    PRODUCT_ADC_FAULT_CONVERSION = (1U << 3U),
    PRODUCT_ADC_FAULT_INPUT_VOLTAGE = (1U << 4U),
    PRODUCT_ADC_FAULT_INTERNAL = (1U << 5U),
    PRODUCT_ADC_FAULT_STALE_OFFLINE = (1U << 6U)
} ProductAdcFaultCategory_t;

bool ProductAdcDriver_Init(void);
const HalAdcDeviceConfig_t *ProductAdcDriver_GetDeviceConfig(uint8_t device);
const AnalogInputRoute_t *ProductAdcDriver_GetRoutes(uint8_t *route_count);
bool ProductAdcDriver_GetDiagnostics(
    uint8_t device, ProductAdcDriverDiagnostics_t *diagnostics);
bool ProductAdcDriver_AuditNextDevice(void);

#endif
