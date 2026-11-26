#ifndef PRODUCT_SENSOR_MEASUREMENT_CONFIG_H
#define PRODUCT_SENSOR_MEASUREMENT_CONFIG_H

/* Product monotonic milliseconds. Repeated foreground calls do not refresh age. */
#ifndef PRODUCT_SENSOR_STALE_TIMEOUT_MS
#define PRODUCT_SENSOR_STALE_TIMEOUT_MS (500U)
#endif
#ifndef PRODUCT_SENSOR_RECOVERY_GOOD_SAMPLES
#define PRODUCT_SENSOR_RECOVERY_GOOD_SAMPLES (3U)
#endif
#if PRODUCT_SENSOR_STALE_TIMEOUT_MS == 0 || PRODUCT_SENSOR_STALE_TIMEOUT_MS > 2147483647UL
#error "Sensor stale timeout must be in 1..INT32_MAX milliseconds"
#endif
#if PRODUCT_SENSOR_RECOVERY_GOOD_SAMPLES == 0 || PRODUCT_SENSOR_RECOVERY_GOOD_SAMPLES > 65535U
#error "Sensor recovery count must be in 1..65535"
#endif
#endif
