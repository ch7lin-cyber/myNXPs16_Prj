#ifndef PRODUCT_ADC_CONFIG_H
#define PRODUCT_ADC_CONFIG_H

#include "HalAdc.h"

/* Four AD7124-4 devices provide one independently configurable product input. */
#define PRODUCT_ADC_DEVICE_COUNT              (4U)
#define PRODUCT_ADC_CHANNELS_PER_DEVICE       (1U)
#define PRODUCT_ADC_ACTIVE_CHANNEL            (0U)
#define PRODUCT_ADC_DEFAULT_SETUP             (0U)

#define PRODUCT_ADC_INTERNAL_REFERENCE_UV     (2500000UL)
#define PRODUCT_ADC_EXTERNAL_REFERENCE1_UV    (2500000UL)
#define PRODUCT_ADC_EXTERNAL_REFERENCE2_UV    (2500000UL)
#define PRODUCT_ADC_SUPPLY_REFERENCE_UV       (3300000UL)

/* Production schematic AIN routing, identical on ADC0 through ADC3. */
#define PRODUCT_ADC_TC_AIN_POSITIVE           (5U)
#define PRODUCT_ADC_TC_AIN_NEGATIVE           (4U)
#define PRODUCT_ADC_RTD_AIN_POSITIVE          (6U)
#define PRODUCT_ADC_RTD_AIN_NEGATIVE          (5U)
#define PRODUCT_ADC_VOLTAGE_AIN_POSITIVE      (3U)
#define PRODUCT_ADC_VOLTAGE_AIN_NEGATIVE      (2U)
#define PRODUCT_ADC_CURRENT_AIN_POSITIVE      (1U)
#define PRODUCT_ADC_CURRENT_AIN_NEGATIVE      (2U)
#define PRODUCT_ADC_IEX1_AIN                  (0U)
#define PRODUCT_ADC_IEX2_AIN                  (7U)
/* REFOUT -> U14/U20/U25/U31 -> CHx_VS bias is required in RTD mode too. */
#define PRODUCT_ADC_REFERENCE_OUTPUT_REQUIRED (1U)

/* CV_SEL0..3 are initialized low: voltage/TC/RTD mode; high selects current. */
#define PRODUCT_ADC_CV_SELECT_VOLTAGE         (0U)
#define PRODUCT_ADC_CV_SELECT_CURRENT         (1U)

/* Factory default: K type, bipolar, internal 2.5 V reference, PGA gain 32. */
#define PRODUCT_ADC_DEFAULT_SENSOR_IS_K       (1U)
#define PRODUCT_ADC_TC_GAIN                   HAL_ADC_GAIN_32
#define PRODUCT_ADC_TC_FILTER                 HAL_ADC_FILTER_SINC4
#define PRODUCT_ADC_TC_FILTER_WORD            (384U)
#define PRODUCT_ADC_TC_REFERENCE              HAL_ADC_REFERENCE_INTERNAL
#define PRODUCT_ADC_REJECT_60_HZ              (1U)
#define PRODUCT_ADC_SPI_CRC_ENABLED           (1U)

/* Full-power 614.4 kHz modulator: FS=384 gives 50 SPS with the Sinc4 filter. */
#define PRODUCT_ADC_OUTPUT_DATA_RATE_HZ       (50U)
#define PRODUCT_ADC_POLL_PERIOD_MS            (10U)
#define PRODUCT_ADC_RECOVERY_PERIOD_MS        (1000U)
#define PRODUCT_ADC_DIAGNOSTIC_PERIOD_MS      (250U)
#define PRODUCT_ADC_COMM_FAULT_COUNT          (3U)
#define PRODUCT_ADC_REFERENCE_FAULT_COUNT     (2U)
#define PRODUCT_ADC_CONVERSION_FAULT_COUNT    (3U)
#define PRODUCT_ADC_CLEAR_GOOD_SAMPLE_COUNT   (10U)
#define PRODUCT_ADC_INTERNAL_CLOCK_HZ          (614400UL)
#define PRODUCT_ADC_SPI_BAUD_RATE_HZ           (200000UL)
#define PRODUCT_ADC_SPI_MODE                   (3U)

/* Analog front-end component values read from the production schematic. */
#define PRODUCT_ADC_RTD_REFERENCE_OHM          (6490UL)
#define PRODUCT_ADC_CURRENT_SHUNT_MILLIOHM     (49900UL)
#define PRODUCT_ADC_INPUT_SERIES_OHM           (510000UL)
#define PRODUCT_ADC_VOLTAGE_DIVIDER_OHM        (6200UL)
#define PRODUCT_ADC_CURRENT_DIVIDER_OHM        (36800UL)
/* R92 + R94 + R95 divider: ADC differential = shunt voltage * 36.8/1056.8. */
#define PRODUCT_ADC_CURRENT_RATIO_NUMERATOR    (36800UL)
#define PRODUCT_ADC_CURRENT_RATIO_DENOMINATOR  (1056800UL)
/* V+/VS network: two 510 kOhm legs and the 6.2 kOhm sense resistor. */
#define PRODUCT_ADC_VOLTAGE_RATIO_NUMERATOR    \
    ((2UL * PRODUCT_ADC_INPUT_SERIES_OHM) +    \
     PRODUCT_ADC_VOLTAGE_DIVIDER_OHM)
#define PRODUCT_ADC_VOLTAGE_RATIO_DENOMINATOR  \
    (PRODUCT_ADC_VOLTAGE_DIVIDER_OHM)

/* RTD excitation-current defaults for the later sensor Apply implementation. */
#define PRODUCT_ADC_IEX_UA_JPT100              (500U)
#define PRODUCT_ADC_IEX_UA_PT100               (500U)
#define PRODUCT_ADC_IEX_UA_NI120               (500U)
#define PRODUCT_ADC_IEX_UA_CU50                (500U)
#define PRODUCT_ADC_IEX_UA_PT1000              (250U)
#define PRODUCT_ADC_IEX_UA_DEFAULT             (0U)

/* Values retained for the later run-time sensor-type reconfiguration phase. */
#define PRODUCT_ADC_GAIN_SB                   HAL_ADC_GAIN_128
#define PRODUCT_ADC_GAIN_TRC                  HAL_ADC_GAIN_64
#define PRODUCT_ADC_GAIN_CURRENT_0_20MA       HAL_ADC_GAIN_64
#define PRODUCT_ADC_GAIN_CURRENT_4_20MA       HAL_ADC_GAIN_64
#define PRODUCT_ADC_GAIN_J_JPT100             HAL_ADC_GAIN_16
#define PRODUCT_ADC_GAIN_PT100_NI120          HAL_ADC_GAIN_8
#define PRODUCT_ADC_GAIN_PT1000               HAL_ADC_GAIN_1
#define PRODUCT_ADC_GAIN_GENERAL              HAL_ADC_GAIN_32
#define PRODUCT_ADC_RTD_FILTER                HAL_ADC_FILTER_SINC4
#define PRODUCT_ADC_RTD_FILTER_WORD           (384U)
#define PRODUCT_ADC_RTD_REFERENCE             HAL_ADC_REFERENCE_EXTERNAL_1

#if defined(__cplusplus)
static_assert(PRODUCT_ADC_DEVICE_COUNT == HAL_ADC_DEVICE_COUNT,
              "Product and platform ADC device counts must match");
static_assert(PRODUCT_ADC_GAIN_CURRENT_0_20MA == HAL_ADC_GAIN_64,
              "I_0_20 must use PGA gain 64");
static_assert(PRODUCT_ADC_GAIN_CURRENT_4_20MA == HAL_ADC_GAIN_64,
              "I_4_20 must use PGA gain 64");
static_assert(PRODUCT_ADC_CURRENT_SHUNT_MILLIOHM == 49900UL,
              "Production current shunt must be 49.9 ohm");
#else
_Static_assert(PRODUCT_ADC_DEVICE_COUNT == HAL_ADC_DEVICE_COUNT,
               "Product and platform ADC device counts must match");
_Static_assert(PRODUCT_ADC_GAIN_CURRENT_0_20MA == HAL_ADC_GAIN_64,
               "I_0_20 must use PGA gain 64");
_Static_assert(PRODUCT_ADC_GAIN_CURRENT_4_20MA == HAL_ADC_GAIN_64,
               "I_4_20 must use PGA gain 64");
_Static_assert(PRODUCT_ADC_CURRENT_SHUNT_MILLIOHM == 49900UL,
               "Production current shunt must be 49.9 ohm");
#endif

#endif /* PRODUCT_ADC_CONFIG_H */
