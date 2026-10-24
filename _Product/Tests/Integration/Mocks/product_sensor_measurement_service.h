#ifndef PRODUCT_SENSOR_MEASUREMENT_SERVICE_H
#define PRODUCT_SENSOR_MEASUREMENT_SERVICE_H

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    int32_t engineering_value;
    uint32_t sample_sequence;
    uint32_t processed_timestamp_ms;
    bool processed_timestamp_valid;
    uint16_t input_error;
    uint32_t conversion_flags;
    uint16_t sensor_type;
    bool valid;
} ProductSensorMeasurementSnapshot_t;

bool ProductSensorMeasurementService_GetSnapshot(
    uint8_t channel,
    ProductSensorMeasurementSnapshot_t *snapshot);

#endif /* PRODUCT_SENSOR_MEASUREMENT_SERVICE_H */
