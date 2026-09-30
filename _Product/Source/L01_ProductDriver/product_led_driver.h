#ifndef PRODUCT_LED_DRIVER_H_
#define PRODUCT_LED_DRIVER_H_

#include <stdbool.h>

typedef enum
{
    PRODUCT_LED_RUN = 0,
    PRODUCT_LED_COMMUNICATION,
    PRODUCT_LED_ERROR,
    PRODUCT_LED_COUNT
} ProductLed_t;

bool ProductLedDriver_Initialize(void);
bool ProductLedDriver_Write(ProductLed_t led, bool on);

#endif /* PRODUCT_LED_DRIVER_H_ */
