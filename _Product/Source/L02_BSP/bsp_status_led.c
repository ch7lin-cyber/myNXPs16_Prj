#include "bsp_status_led.h"

#include <stdint.h>

#include "product_led_driver.h"

bool BspStatusLed_Initialize(void)
{
    return ProductLedDriver_Initialize();
}

bool BspStatusLed_Write(BspStatusLed_t led, bool on)
{
    if ((uint32_t)led >= (uint32_t)BSP_STATUS_LED_COUNT)
    {
        return false;
    }
    return ProductLedDriver_Write((ProductLed_t)led, on);
}
