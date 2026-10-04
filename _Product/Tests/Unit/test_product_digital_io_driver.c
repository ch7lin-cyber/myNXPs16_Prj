#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "DigitalInputService.h"
#include "DigitalOutputService.h"
#include "fsl_gpio.h"
#include "product_digital_io_driver.h"

GPIO_Type g_mock_gpio;
static uint32_t g_port_state[2U];

void GPIO_PinWrite(GPIO_Type *base, uint32_t port, uint32_t pin,
                   uint8_t output)
{
    assert(base == &g_mock_gpio);
    assert(port < 2U);
    if (output != 0U)
    {
        g_port_state[port] |= (1UL << pin);
    }
    else
    {
        g_port_state[port] &= ~(1UL << pin);
    }
}

uint32_t GPIO_PinRead(GPIO_Type *base, uint32_t port, uint32_t pin)
{
    assert(base == &g_mock_gpio);
    assert(port < 2U);
    return (g_port_state[port] >> pin) & 1UL;
}

int main(void)
{
    DigitalInputSnapshot_t inputs;
    DigitalOutputState_t outputs;

    (void)memset(g_port_state, 0, sizeof(g_port_state));
    g_port_state[1] |= (1UL << 9U);
    g_port_state[0] |= (1UL << 23U);

    assert(ProductDigitalIoDriver_Init());
    assert(DigitalInputService_Initialize(PRODUCT_DIGITAL_INPUT_COUNT) ==
           DIGITAL_INPUT_STATUS_OK);
    assert(DigitalOutputService_Initialize(PRODUCT_DIGITAL_OUTPUT_COUNT) ==
           DIGITAL_OUTPUT_STATUS_OK);
    assert(DigitalInputService_GetSnapshot(&inputs));
    assert(inputs.state_mask == 0x0005U);

    assert(DigitalOutputService_SetMask(0x000AU) ==
           DIGITAL_OUTPUT_STATUS_OK);
    assert((g_port_state[0] & (1UL << 19U)) == 0U);
    assert((g_port_state[0] & (1UL << 26U)) != 0U);
    assert((g_port_state[0] & (1UL << 25U)) == 0U);
    assert((g_port_state[1] & (1UL << 25U)) != 0U);
    assert(DigitalOutputService_GetState(&outputs));
    assert(outputs.active_mask == 0x000AU);
    return 0;
}
