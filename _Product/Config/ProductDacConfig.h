#ifndef PRODUCT_DAC_CONFIG_H
#define PRODUCT_DAC_CONFIG_H

#define PRODUCT_DAC_DEVICE_COUNT                 (2U)
#define PRODUCT_DAC_CHANNELS_PER_DEVICE          (2U)
#define PRODUCT_DAC_CHANNEL_COUNT                \
    (PRODUCT_DAC_DEVICE_COUNT * PRODUCT_DAC_CHANNELS_PER_DEVICE)

#define PRODUCT_DAC_INTERNAL_REFERENCE_UV        (2500000UL)
#define PRODUCT_DAC_GAIN                         (1U)
#define PRODUCT_DAC_NOMINAL_FULL_SCALE_UV        \
    (PRODUCT_DAC_INTERNAL_REFERENCE_UV * PRODUCT_DAC_GAIN)
#define PRODUCT_DAC_INITIAL_CODE                 (0U)

/* Per-channel raw DAC endpoints for the DTX MV=0..1000 mapping.
 * Adjust these after output calibration (min <= max). */
#ifndef PRODUCT_DAC_MV_MIN_CODES
#define PRODUCT_DAC_MV_MIN_CODES {0U, 0U, 0U, 0U}
#endif
#ifndef PRODUCT_DAC_MV_MAX_CODES
#define PRODUCT_DAC_MV_MAX_CODES {65535U, 65535U, 65535U, 65535U}
#endif

#endif /* PRODUCT_DAC_CONFIG_H */
