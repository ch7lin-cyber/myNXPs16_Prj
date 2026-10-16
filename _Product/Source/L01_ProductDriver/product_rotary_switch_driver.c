#include "product_rotary_switch_driver.h"

#include <stddef.h>

#include "ProductRotarySwitchConfig.h"
#include "fsl_gpio.h"
#include "product_board_pin_mux.h"

static ProductRotarySwitchSnapshot_t g_snapshot;
static bool g_has_valid_sample;

static uint8_t ReadRawValue(void)
{
    uint8_t raw_value = 0U;

    if (GPIO_PinRead(BOARD_INITROTARYSWPINS_READ_PIN0_GPIO,
                     BOARD_INITROTARYSWPINS_READ_PIN0_PORT,
                     BOARD_INITROTARYSWPINS_READ_PIN0_PIN) != 0U)
    {
        raw_value |= 0x01U;
    }
    if (GPIO_PinRead(BOARD_INITROTARYSWPINS_READ_PIN1_GPIO,
                     BOARD_INITROTARYSWPINS_READ_PIN1_PORT,
                     BOARD_INITROTARYSWPINS_READ_PIN1_PIN) != 0U)
    {
        raw_value |= 0x02U;
    }
    return raw_value;
}

void ProductRotarySwitchDriver_Initialize(void)
{
    g_snapshot.position = 0U;
    g_snapshot.raw_value = 0U;
    g_snapshot.revision = 0U;
    g_snapshot.status = PRODUCT_ROTARY_SWITCH_STATUS_NOT_INITIALIZED;
    g_has_valid_sample = false;
}

void ProductRotarySwitchDriver_Process(void)
{
    uint8_t raw_value = ReadRawValue();
    /* DTX: PIN85 high selects station 2, low selects station 1.
     * PIN86 is reported in raw_value only; it never selects the station. */
    uint8_t position = (uint8_t)((raw_value & 0x01U) + 1U);

    if (!g_has_valid_sample || (position != g_snapshot.position))
    {
        g_snapshot.revision++;
    }
    g_snapshot.raw_value = raw_value;
    g_snapshot.position = position;
    g_snapshot.status = PRODUCT_ROTARY_SWITCH_STATUS_READY;
    g_has_valid_sample = true;
}

bool ProductRotarySwitchDriver_GetSnapshot(
    ProductRotarySwitchSnapshot_t *snapshot)
{
    if (snapshot == NULL)
    {
        return false;
    }
    *snapshot = g_snapshot;
    return true;
}
