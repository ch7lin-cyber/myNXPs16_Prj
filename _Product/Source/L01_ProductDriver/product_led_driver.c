#include "product_led_driver.h"

#include <stdint.h>

#include "fsl_gpio.h"
#include "pin_mux.h"

typedef struct
{
    GPIO_Type *gpio;
    uint32_t port;
    uint32_t pin;
} ProductLedPin_t;

static const ProductLedPin_t g_led_pins[PRODUCT_LED_COUNT] =
{
    {
        BOARD_INITLEDSPINS_LED_RUN_GPIO,
        BOARD_INITLEDSPINS_LED_RUN_PORT,
        BOARD_INITLEDSPINS_LED_RUN_PIN
    },
    {
        BOARD_INITLEDSPINS_LED_COM_GPIO,
        BOARD_INITLEDSPINS_LED_COM_PORT,
        BOARD_INITLEDSPINS_LED_COM_PIN
    },
    {
        BOARD_INITLEDSPINS_LED_ERR_GPIO,
        BOARD_INITLEDSPINS_LED_ERR_PORT,
        BOARD_INITLEDSPINS_LED_ERR_PIN
    }
};

bool ProductLedDriver_Initialize(void)
{
    uint32_t led;

    /* BOARD_InitBootPins() owns pin mux and GPIO direction initialization. */
    for (led = 0U; led < (uint32_t)PRODUCT_LED_COUNT; led++)
    {
        GPIO_PinWrite(g_led_pins[led].gpio,
                      g_led_pins[led].port,
                      g_led_pins[led].pin,
                      0U);
    }
    return true;
}

bool ProductLedDriver_Write(ProductLed_t led, bool on)
{
    const ProductLedPin_t *pin;

    if ((uint32_t)led >= (uint32_t)PRODUCT_LED_COUNT)
    {
        return false;
    }
    pin = &g_led_pins[(uint32_t)led];
    GPIO_PinWrite(pin->gpio, pin->port, pin->pin, on ? 1U : 0U);
    return true;
}
