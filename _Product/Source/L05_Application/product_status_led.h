#ifndef PRODUCT_STATUS_LED_H_
#define PRODUCT_STATUS_LED_H_

#include <stdbool.h>

bool ProductStatusLed_Initialize(void);
void ProductStatusLed_Tick1ms(void);
void ProductStatusLed_Process(void);

#endif /* PRODUCT_STATUS_LED_H_ */
