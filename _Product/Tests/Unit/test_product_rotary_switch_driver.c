#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "fsl_gpio.h"
#include "product_rotary_switch_driver.h"

GPIO_Type g_mock_gpio;
static uint8_t g_raw_value;

uint32_t GPIO_PinRead(GPIO_Type *base, uint32_t port, uint32_t pin)
{
    assert(base == &g_mock_gpio);
    if ((port == 1U) && (pin == 13U))
    {
        return (uint32_t)(g_raw_value & 0x01U);
    }
    if ((port == 1U) && (pin == 18U))
    {
        return (uint32_t)(g_raw_value & 0x02U);
    }
    if ((port == 0U) && (pin == 1U))
    {
        return (uint32_t)(g_raw_value & 0x04U);
    }
    assert((port == 0U) && (pin == 17U));
    return (uint32_t)(g_raw_value & 0x08U);
}

int main(void)
{
    ProductRotarySwitchSnapshot_t snapshot;

    ProductRotarySwitchDriver_Initialize();
    assert(ProductRotarySwitchDriver_GetSnapshot(&snapshot));
    assert(snapshot.status == PRODUCT_ROTARY_SWITCH_STATUS_NOT_INITIALIZED);
    assert(snapshot.revision == 0U);
    assert(!ProductRotarySwitchDriver_GetSnapshot(NULL));

    g_raw_value = 0x0AU;
    ProductRotarySwitchDriver_Process();
    assert(ProductRotarySwitchDriver_GetSnapshot(&snapshot));
    assert(snapshot.raw_value == 0x0AU);
    assert(snapshot.position == 5U);
    assert(snapshot.revision == 1U);
    assert(snapshot.status == PRODUCT_ROTARY_SWITCH_STATUS_READY);

    ProductRotarySwitchDriver_Process();
    assert(ProductRotarySwitchDriver_GetSnapshot(&snapshot));
    assert(snapshot.revision == 1U);

    g_raw_value = 0x06U;
    ProductRotarySwitchDriver_Process();
    assert(ProductRotarySwitchDriver_GetSnapshot(&snapshot));
    assert(snapshot.position == 9U);
    assert(snapshot.revision == 2U);
    return 0;
}
