#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#include "HalDac.h"
#include "bsp_analog_output.h"
#include "fsl_gpio.h"
#include "fsl_spi.h"
#include "product_dac8562_driver.h"

#define MOCK_MAX_FRAMES (16U)

typedef struct
{
    uint8_t device;
    uint8_t data[3];
} MockFrame_t;

SPI_Type g_mock_spi;
GPIO_Type g_mock_gpio;
static MockFrame_t g_frames[MOCK_MAX_FRAMES];
static size_t g_frame_count;
static int g_selected_device = -1;

void GPIO_PinWrite(GPIO_Type *base, uint32_t port, uint32_t pin,
                   uint8_t output)
{
    assert(base == &g_mock_gpio);
    if ((port == 0U) && (pin == 27U))
    {
        if (output == 0U)
        {
            g_selected_device = 0;
        }
        else if (g_selected_device == 0)
        {
            g_selected_device = -1;
        }
    }
    else if ((port == 1U) && (pin == 15U))
    {
        if (output == 0U)
        {
            g_selected_device = 1;
        }
        else if (g_selected_device == 1)
        {
            g_selected_device = -1;
        }
    }
    else
    {
        assert((port == 1U) && (pin == 16U) && (output == 1U));
    }
}

status_t SPI_MasterTransferBlocking(SPI_Type *base, spi_transfer_t *transfer)
{
    size_t index;

    assert(base == &g_mock_spi);
    assert(transfer != NULL);
    assert(transfer->txData != NULL);
    assert(transfer->dataSize == 3U);
    assert(g_selected_device >= 0);
    assert(g_frame_count < MOCK_MAX_FRAMES);
    g_frames[g_frame_count].device = (uint8_t)g_selected_device;
    for (index = 0U; index < 3U; index++)
    {
        g_frames[g_frame_count].data[index] = transfer->txData[index];
    }
    g_frame_count++;
    return kStatus_Success;
}

static void AssertFrame(size_t index, uint8_t device, uint8_t command_address,
                        uint16_t data)
{
    assert(index < g_frame_count);
    assert(g_frames[index].device == device);
    assert(g_frames[index].data[0] == command_address);
    assert(g_frames[index].data[1] == (uint8_t)(data >> 8U));
    assert(g_frames[index].data[2] == (uint8_t)data);
}

int main(void)
{
    uint16_t code;

    assert(ProductDac8562Driver_Init());
    assert(BspAnalogOutput_Initialize());
    assert(g_frame_count == 10U);

    AssertFrame(0U, 0U, 0x2FU, 0x0001U);
    AssertFrame(1U, 0U, 0x3FU, 0x0001U);
    AssertFrame(2U, 0U, 0x02U, 0x0003U);
    AssertFrame(3U, 0U, 0x27U, 0x0003U);
    AssertFrame(4U, 0U, 0x17U, 0x0000U);
    AssertFrame(5U, 1U, 0x2FU, 0x0001U);
    AssertFrame(9U, 1U, 0x17U, 0x0000U);

    assert(BspAnalogOutput_WriteCode(BSP_ANALOG_OUTPUT_0, 0xA55AU));
    AssertFrame(10U, 0U, 0x18U, 0xA55AU);
    assert(BspAnalogOutput_GetLastCode(BSP_ANALOG_OUTPUT_0, &code));
    assert(code == 0xA55AU);

    assert(BspAnalogOutput_WriteDacMicrovolts(BSP_ANALOG_OUTPUT_3,
                                               1250000U));
    AssertFrame(11U, 1U, 0x19U, 0x8000U);
    assert(!BspAnalogOutput_WriteDacMicrovolts(BSP_ANALOG_OUTPUT_3,
                                                2500001U));
    assert(!BspAnalogOutput_WriteCode((BspAnalogOutput_t)4U, 0U));
    assert(!BspAnalogOutput_GetLastCode(BSP_ANALOG_OUTPUT_0, NULL));
    return 0;
}
