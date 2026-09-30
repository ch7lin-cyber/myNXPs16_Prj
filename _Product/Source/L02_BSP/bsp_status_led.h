#ifndef BSP_STATUS_LED_H_
#define BSP_STATUS_LED_H_

#include <stdbool.h>

typedef enum
{
    BSP_STATUS_LED_RUN = 0,
    BSP_STATUS_LED_COMMUNICATION,
    BSP_STATUS_LED_ERROR,
    BSP_STATUS_LED_COUNT
} BspStatusLed_t;

bool BspStatusLed_Initialize(void);
bool BspStatusLed_Write(BspStatusLed_t led, bool on);

#endif /* BSP_STATUS_LED_H_ */
