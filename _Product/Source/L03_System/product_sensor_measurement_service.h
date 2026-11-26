#ifndef PRODUCT_SENSOR_MEASUREMENT_SERVICE_H
#define PRODUCT_SENSOR_MEASUREMENT_SERVICE_H

#include <stdbool.h>
#include <stdint.h>

#include "FactoryCalibrationService.h"
#include "SensorConversionService.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PRODUCT_INPUT_ERROR_NONE (61U)
#define PRODUCT_INPUT_ERROR_STALE (144U)
#define PRODUCT_INPUT_ERROR_RECOVERING (145U)
#define PRODUCT_INPUT_ERROR_SOURCE_FAULT (146U)

/* Flags describe data quality, not a guaranteed physical open-wire detector. */
#define PRODUCT_SENSOR_QUALITY_NO_SAMPLE    (1U << 0U)
#define PRODUCT_SENSOR_QUALITY_SOURCE_FAULT (1U << 1U)
#define PRODUCT_SENSOR_QUALITY_STALE        (1U << 2U)
#define PRODUCT_SENSOR_QUALITY_CONVERSION   (1U << 3U)
#define PRODUCT_SENSOR_QUALITY_RECOVERING   (1U << 4U)
#define PRODUCT_SENSOR_QUALITY_DISABLED     (1U << 5U)
#define PRODUCT_SENSOR_QUALITY_SOURCE_UNKNOWN (1U << 6U)

/* Modbus Temperature Input Error values, shared by all sensor types. */
#define PRODUCT_INPUT_ERROR_OUT_OF_SPEC  (142U)
#define PRODUCT_INPUT_ERROR_OUT_OF_RANGE (143U)

typedef struct
{
    uint32_t raw_code;
    int32_t uncalibrated_uv;
    int32_t calibrated_uv;
    int32_t cjc_temperature_centi_c;
    int32_t engineering_value;
    uint32_t conversion_flags;
    uint32_t sample_sequence;
    uint32_t processed_timestamp_ms; /* first processing of this sequence, not ADC capture */
    bool processed_timestamp_valid;
    uint16_t sensor_type;
    uint16_t input_error;
    bool valid;
    uint16_t quality_flags;
    uint16_t recovery_good_samples;
    uint32_t quality_revision; /* increments when old history is invalidated */
    uint32_t sample_age_ms;    /* UINT32_MAX when no timed acquisition observed */
} ProductSensorMeasurementSnapshot_t;

void ProductSensorMeasurementService_Initialize(void);
/* ProductApplication publishes ADC online/acquisition health before ProcessAt.
 * Missing status is fail-closed. This does not clear Fault or Safety latches. */
bool ProductSensorMeasurementService_SetSourceHealth(
    uint8_t channel, bool online, bool sample_fault);
bool ProductSensorMeasurementService_Process(uint8_t channel);
/* Common product monotonic clock; repeated sequence does not refresh time. */
bool ProductSensorMeasurementService_ProcessAt(uint8_t channel, uint32_t now_ms);
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
