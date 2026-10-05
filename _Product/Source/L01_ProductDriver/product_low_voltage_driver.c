#include "product_low_voltage_driver.h"

#include "ProductLowVoltageConfig.h"
#include "fsl_gpio.h"
#include "fsl_pint.h"
#include "peripherals.h"
#include "pin_mux.h"

static volatile uint32_t g_interrupt_count;

void ProductLowVoltageDriver_Initialize(void)
{
    g_interrupt_count = 0U;

    /* PINT_CLK shares and resets PINT during generated board init. Re-arm
     * PINT0 after every generated peripheral initializer has completed. */
    PINT_PinInterruptConfig(PINT_PERIPHERAL, PINT_INT_0,
                            kPINT_PinIntEnableRiseEdge,
                            drv_level_detect_callback);
    PINT_EnableCallbackByIndex(PINT_PERIPHERAL, kPINT_PinInt0);
}

bool ProductLowVoltageDriver_IsActive(void)
{
    bool pin_high =
        GPIO_PinRead(BOARD_INITLVPINS_LV_GPIO,
                     BOARD_INITLVPINS_LV_PORT,
                     BOARD_INITLVPINS_LV_PIN) != 0U;

#if (PRODUCT_LOW_VOLTAGE_ACTIVE_HIGH != 0U)
    return pin_high;
#else
    return !pin_high;
#endif
}

void ProductLowVoltageDriver_NotifyInterrupt(void)
{
    g_interrupt_count++;
}

uint32_t ProductLowVoltageDriver_GetInterruptCount(void)
{
    return g_interrupt_count;
}
