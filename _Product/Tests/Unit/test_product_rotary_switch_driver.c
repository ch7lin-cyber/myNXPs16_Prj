#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include "fsl_gpio.h"
#include "product_rotary_switch_driver.h"

GPIO_Type g_mock_gpio;
static uint8_t g_raw_value;
static unsigned g_reads[2];
uint32_t GPIO_PinRead(GPIO_Type *base, uint32_t port, uint32_t pin)
{
    assert(base == &g_mock_gpio);
    if ((port == 1U) && (pin == 27U))
    {
        ++g_reads[0];
        return g_raw_value & 1U;
    }
    /* Any access to one of the old four rotary pins fails this test. */
    assert((port == 0U) && (pin == 4U));
    ++g_reads[1];
    return (g_raw_value >> 1U) & 1U;
}
int main(void)
{
    ProductRotarySwitchSnapshot_t snapshot;
    const uint8_t raw[] = {0U, 2U, 1U, 3U, 0U};
    const uint8_t station[] = {1U, 1U, 2U, 2U, 1U};
    const uint16_t revision[] = {1U, 1U, 2U, 2U, 3U};
    ProductRotarySwitchDriver_Initialize();
    assert(ProductRotarySwitchDriver_GetSnapshot(&snapshot));
    assert(snapshot.status == PRODUCT_ROTARY_SWITCH_STATUS_NOT_INITIALIZED);
    assert(snapshot.revision == 0U);
    assert(!ProductRotarySwitchDriver_GetSnapshot(NULL));
    for (unsigned i = 0U; i < sizeof(raw); ++i)
    {
        g_raw_value = raw[i];
        ProductRotarySwitchDriver_Process();
        assert(ProductRotarySwitchDriver_GetSnapshot(&snapshot));
        assert(snapshot.raw_value == raw[i]);
        assert(snapshot.position == station[i]);
        assert(snapshot.revision == revision[i]);
        assert(snapshot.status == PRODUCT_ROTARY_SWITCH_STATUS_READY);
    }
    assert(g_reads[0] == 5U && g_reads[1] == 5U);
    return 0;
}
