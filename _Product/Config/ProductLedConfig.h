#ifndef PRODUCT_LED_CONFIG_H_
#define PRODUCT_LED_CONFIG_H_

/* Status LED timing is driven by the existing 1 ms product tick. */
#define PRODUCT_LED_RUN_TOGGLE_PERIOD_MS   (500U)
#define PRODUCT_LED_COMM_PULSE_PERIOD_MS   (100U)
#define PRODUCT_LED_ERROR_TOGGLE_PERIOD_MS (250U)

#if (PRODUCT_LED_RUN_TOGGLE_PERIOD_MS == 0U)
#error "PRODUCT_LED_RUN_TOGGLE_PERIOD_MS must be greater than zero"
#endif

#if (PRODUCT_LED_COMM_PULSE_PERIOD_MS == 0U)
#error "PRODUCT_LED_COMM_PULSE_PERIOD_MS must be greater than zero"
#endif

#if (PRODUCT_LED_ERROR_TOGGLE_PERIOD_MS == 0U)
#error "PRODUCT_LED_ERROR_TOGGLE_PERIOD_MS must be greater than zero"
#endif

#endif /* PRODUCT_LED_CONFIG_H_ */
