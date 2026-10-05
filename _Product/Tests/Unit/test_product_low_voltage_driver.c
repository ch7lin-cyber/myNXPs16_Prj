#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "fsl_gpio.h"
#include "fsl_pint.h"
#include "product_low_voltage_driver.h"

GPIO_Type g_mock_gpio;
PINT_Type g_mock_pint;
static bool g_pin_high;
static pint_cb_t g_callback;
static bool g_callback_enabled;

uint32_t GPIO_PinRead(GPIO_Type *base, uint32_t port, uint32_t pin)
{
    assert(base == &g_mock_gpio);
    assert(port == 1U);
    assert(pin == 31U);
    return g_pin_high ? 1U : 0U;
}

void PINT_PinInterruptConfig(PINT_Type *base, pint_pin_int_t intr,
                             pint_pin_enable_t enable, pint_cb_t callback)
{
    assert(base == &g_mock_pint);
    assert(intr == kPINT_PinInt0);
    assert(enable == kPINT_PinIntEnableRiseEdge);
    assert(callback != NULL);
    g_callback = callback;
}

void PINT_EnableCallbackByIndex(PINT_Type *base, pint_pin_int_t intr)
{
    assert(base == &g_mock_pint);
    assert(intr == kPINT_PinInt0);
    g_callback_enabled = true;
}

void drv_level_detect_callback(pint_pin_int_t pintr, uint32_t pmatch_status)
{
    (void)pintr;
    (void)pmatch_status;
    ProductLowVoltageDriver_NotifyInterrupt();
}

int main(void)
{
    ProductLowVoltageDriver_Initialize();
    assert(g_callback == drv_level_detect_callback);
    assert(g_callback_enabled);
    assert(!ProductLowVoltageDriver_IsActive());

    g_pin_high = true;
    assert(ProductLowVoltageDriver_IsActive());
    g_callback(kPINT_PinInt0, 0U);
    g_callback(kPINT_PinInt0, 0U);
    assert(ProductLowVoltageDriver_GetInterruptCount() == 2U);
    return 0;
}
