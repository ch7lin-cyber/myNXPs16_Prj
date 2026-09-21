#ifndef PRODUCT_PWM_DRIVER_H
#define PRODUCT_PWM_DRIVER_H

#include <stdbool.h>

/* Register the LPC55S16 OUT1 CTIMER adapter with the shared HAL. */
bool ProductPwmDriver_Init(void);

#endif
