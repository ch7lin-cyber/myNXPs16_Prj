#include "product_rotary_switch_driver.h"

#include <stddef.h>

#include "ProductRotarySwitchConfig.h"
#include "fsl_gpio.h"
#include "pin_mux.h"

static ProductRotarySwitchSnapshot_t g_snapshot;
static bool g_has_valid_sample;

static uint8_t ReadRawValue(void)
{
    uint8_t raw_value = 0U;

    if (GPIO_PinRead(BOARD_INITROTARYSWPINS_RR_SW_1_GPIO,
                     BOARD_INITROTARYSWPINS_RR_SW_1_PORT,
                     BOARD_INITROTARYSWPINS_RR_SW_1_PIN) != 0U)
    {
        raw_value |= 0x01U;
    }
    if (GPIO_PinRead(BOARD_INITROTARYSWPINS_RR_SW_2_GPIO,
                     BOARD_INITROTARYSWPINS_RR_SW_2_PORT,
                     BOARD_INITROTARYSWPINS_RR_SW_2_PIN) != 0U)
    {
        raw_value |= 0x02U;
    }
    if (GPIO_PinRead(BOARD_INITROTARYSWPINS_RR_SW_4_GPIO,
                     BOARD_INITROTARYSWPINS_RR_SW_4_PORT,
                     BOARD_INITROTARYSWPINS_RR_SW_4_PIN) != 0U)
    {
        raw_value |= 0x04U;
    }
    if (GPIO_PinRead(BOARD_INITROTARYSWPINS_RR_SW_8_GPIO,
                     BOARD_INITROTARYSWPINS_RR_SW_8_PORT,
                     BOARD_INITROTARYSWPINS_RR_SW_8_PIN) != 0U)
    {
        raw_value |= 0x08U;
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
    uint8_t position = (uint8_t)(
        (raw_value ^ PRODUCT_ROTARY_SWITCH_ACTIVE_LOW_MASK) & 0x0FU);

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
