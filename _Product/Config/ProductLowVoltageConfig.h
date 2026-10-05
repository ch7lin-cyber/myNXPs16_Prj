#ifndef PRODUCT_LOW_VOLTAGE_CONFIG_H
#define PRODUCT_LOW_VOLTAGE_CONFIG_H

/* Existing PINT0 rising-edge routing indicates that LV asserts high. */
#define PRODUCT_LOW_VOLTAGE_ACTIVE_HIGH        (1U)
#define PRODUCT_LOW_VOLTAGE_ASSERT_SAMPLES     (3U)
#define PRODUCT_LOW_VOLTAGE_RELEASE_SAMPLES    (100U)
#define PRODUCT_LOW_VOLTAGE_SAMPLE_PERIOD_MS   (1U)

#if (PRODUCT_LOW_VOLTAGE_ASSERT_SAMPLES == 0U)
#error "Low-voltage assert sample count must be nonzero"
#endif
#if (PRODUCT_LOW_VOLTAGE_RELEASE_SAMPLES == 0U)
#error "Low-voltage release sample count must be nonzero"
#endif
#if (PRODUCT_LOW_VOLTAGE_SAMPLE_PERIOD_MS == 0U)
#error "Low-voltage sample period must be nonzero"
#endif

#endif
