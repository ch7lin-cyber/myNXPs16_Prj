#include "product_digital_io_driver.h"

#include <stddef.h>
#include <stdint.h>

#include "HalGpio.h"
#include "ProductDigitalIoConfig.h"
#include "fsl_gpio.h"
#include "pin_mux.h"

typedef struct
{
    GPIO_Type *gpio;
    uint32_t port;
    uint32_t pin;
    bool active_low;
} ProductDigitalIoPin_t;

static ProductDigitalIoPin_t g_input_pins[PRODUCT_DIGITAL_INPUT_COUNT] =
{
    {BOARD_INITDIOPINS_DI0_GPIO, BOARD_INITDIOPINS_DI0_PORT,
     BOARD_INITDIOPINS_DI0_PIN,
     (PRODUCT_DIGITAL_INPUT_ACTIVE_LOW_MASK & 0x0001U) != 0U},
    {BOARD_INITDIOPINS_DI1_GPIO, BOARD_INITDIOPINS_DI1_PORT,
     BOARD_INITDIOPINS_DI1_PIN,
     (PRODUCT_DIGITAL_INPUT_ACTIVE_LOW_MASK & 0x0002U) != 0U},
    {BOARD_INITDIOPINS_DI2_GPIO, BOARD_INITDIOPINS_DI2_PORT,
     BOARD_INITDIOPINS_DI2_PIN,
     (PRODUCT_DIGITAL_INPUT_ACTIVE_LOW_MASK & 0x0004U) != 0U},
    {BOARD_INITDIOPINS_DI3_GPIO, BOARD_INITDIOPINS_DI3_PORT,
     BOARD_INITDIOPINS_DI3_PIN,
     (PRODUCT_DIGITAL_INPUT_ACTIVE_LOW_MASK & 0x0008U) != 0U}
};

static ProductDigitalIoPin_t g_output_pins[PRODUCT_DIGITAL_OUTPUT_COUNT] =
{
    {BOARD_INITDIOPINS_DO0_GPIO, BOARD_INITDIOPINS_DO0_PORT,
     BOARD_INITDIOPINS_DO0_PIN,
     (PRODUCT_DIGITAL_OUTPUT_ACTIVE_LOW_MASK & 0x0001U) != 0U},
    {BOARD_INITDIOPINS_DO1_GPIO, BOARD_INITDIOPINS_DO1_PORT,
     BOARD_INITDIOPINS_DO1_PIN,
     (PRODUCT_DIGITAL_OUTPUT_ACTIVE_LOW_MASK & 0x0002U) != 0U},
    {BOARD_INITDIOPINS_DO2_GPIO, BOARD_INITDIOPINS_DO2_PORT,
     BOARD_INITDIOPINS_DO2_PIN,
     (PRODUCT_DIGITAL_OUTPUT_ACTIVE_LOW_MASK & 0x0004U) != 0U},
    {BOARD_INITDIOPINS_DO3_GPIO, BOARD_INITDIOPINS_DO3_PORT,
     BOARD_INITDIOPINS_DO3_PIN,
     (PRODUCT_DIGITAL_OUTPUT_ACTIVE_LOW_MASK & 0x0008U) != 0U}
};

static HalGpioStatus_t InitializeInput(void *driver_context)
{
    return (driver_context != NULL) ? HAL_GPIO_STATUS_OK :
                                      HAL_GPIO_STATUS_INVALID_ARGUMENT;
}

static HalGpioStatus_t ReadInput(void *driver_context, bool *active)
{
    ProductDigitalIoPin_t *pin = (ProductDigitalIoPin_t *)driver_context;
    bool pin_high;

    if ((pin == NULL) || (active == NULL))
    {
        return HAL_GPIO_STATUS_INVALID_ARGUMENT;
    }
    pin_high = GPIO_PinRead(pin->gpio, pin->port, pin->pin) != 0U;
    *active = pin->active_low ? !pin_high : pin_high;
    return HAL_GPIO_STATUS_OK;
}

static HalGpioStatus_t InitializeOutput(void *driver_context)
{
    ProductDigitalIoPin_t *pin = (ProductDigitalIoPin_t *)driver_context;

    if (pin == NULL)
    {
        return HAL_GPIO_STATUS_INVALID_ARGUMENT;
    }
    GPIO_PinWrite(pin->gpio, pin->port, pin->pin,
                  pin->active_low ? 1U : 0U);
    return HAL_GPIO_STATUS_OK;
}

static HalGpioStatus_t WriteOutput(void *driver_context, bool active)
{
    ProductDigitalIoPin_t *pin = (ProductDigitalIoPin_t *)driver_context;
    bool pin_high;

    if (pin == NULL)
    {
        return HAL_GPIO_STATUS_INVALID_ARGUMENT;
    }
    pin_high = pin->active_low ? !active : active;
    GPIO_PinWrite(pin->gpio, pin->port, pin->pin, pin_high ? 1U : 0U);
    return HAL_GPIO_STATUS_OK;
}

bool ProductDigitalIoDriver_Init(void)
{
    static const HalGpioInputDriverOps_t input_ops =
    {
        InitializeInput,
        ReadInput
    };
    static const HalGpioOutputDriverOps_t output_ops =
    {
        InitializeOutput,
        WriteOutput
    };
    uint8_t channel;

    for (channel = 0U; channel < PRODUCT_DIGITAL_INPUT_COUNT; channel++)
    {
        if (HalGpio_RegisterInputDriver(
                channel, &input_ops, &g_input_pins[channel]) !=
            HAL_GPIO_STATUS_OK)
        {
            return false;
        }
    }
    for (channel = 0U; channel < PRODUCT_DIGITAL_OUTPUT_COUNT; channel++)
    {
        if (HalGpio_RegisterOutputDriver(
                channel, &output_ops, &g_output_pins[channel]) !=
            HAL_GPIO_STATUS_OK)
        {
            return false;
        }
    }
    return true;
}
