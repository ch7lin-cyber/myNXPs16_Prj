#ifndef PRODUCT_SENSOR_MEASUREMENT_SERVICE_H
#define PRODUCT_SENSOR_MEASUREMENT_SERVICE_H

#include <stdbool.h>
#include <stdint.h>

#include "FactoryCalibrationService.h"
#include "SensorConversionService.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    uint32_t raw_code;
    int32_t uncalibrated_uv;
    int32_t calibrated_uv;
    int32_t cjc_temperature_centi_c;
    int32_t engineering_value;
    uint32_t conversion_flags;
    uint32_t sample_sequence;
    uint16_t sensor_type;
    uint16_t input_error;
    bool valid;
} ProductSensorMeasurementSnapshot_t;

void ProductSensorMeasurementService_Initialize(void);
bool ProductSensorMeasurementService_Process(uint8_t channel);
bool ProductSensorMeasurementService_GetSnapshot(
    uint8_t channel,
    ProductSensorMeasurementSnapshot_t *snapshot);

/* Exposed for deterministic validation of the product-to-platform mapping. */
bool ProductSensorMeasurementService_ResolveConfiguration(
    uint16_t sensor_type,
    SensorConversionConfig_t *configuration,
    FactoryCalibrationProfile_t *calibration_profile);

#ifdef __cplusplus
}
#endif

#endif /* PRODUCT_SENSOR_MEASUREMENT_SERVICE_H */
