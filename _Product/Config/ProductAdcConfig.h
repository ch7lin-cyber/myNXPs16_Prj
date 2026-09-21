#ifndef PRODUCT_ADC_CONFIG_H
#define PRODUCT_ADC_CONFIG_H

#include "HalAdc.h"

/*
 * Board-level analog input defaults.
 *
 * Verify these AIN pairs and the RTD reference/excitation network against the
 * production schematic. Logical inputs are numbered device * 4 + channel.
 */
#define PRODUCT_ADC_CHANNELS_PER_DEVICE       (4U)

#define PRODUCT_ADC_CHANNEL0_AIN_POSITIVE     (0U)
#define PRODUCT_ADC_CHANNEL0_AIN_NEGATIVE     (1U)
#define PRODUCT_ADC_CHANNEL1_AIN_POSITIVE     (2U)
#define PRODUCT_ADC_CHANNEL1_AIN_NEGATIVE     (3U)
#define PRODUCT_ADC_CHANNEL2_AIN_POSITIVE     (4U)
#define PRODUCT_ADC_CHANNEL2_AIN_NEGATIVE     (5U)
#define PRODUCT_ADC_CHANNEL3_AIN_POSITIVE     (6U)
#define PRODUCT_ADC_CHANNEL3_AIN_NEGATIVE     (7U)

#define PRODUCT_ADC_TC_GAIN                   HAL_ADC_GAIN_32
#define PRODUCT_ADC_TC_FILTER                 HAL_ADC_FILTER_SINC4
#define PRODUCT_ADC_TC_FILTER_WORD            (384U)
#define PRODUCT_ADC_TC_REFERENCE              HAL_ADC_REFERENCE_INTERNAL

#define PRODUCT_ADC_RTD_GAIN                  HAL_ADC_GAIN_16
#define PRODUCT_ADC_RTD_FILTER                HAL_ADC_FILTER_SINC4
#define PRODUCT_ADC_RTD_FILTER_WORD           (384U)
#define PRODUCT_ADC_RTD_REFERENCE             HAL_ADC_REFERENCE_EXTERNAL_1

#endif /* PRODUCT_ADC_CONFIG_H */
