#include "product_dac8562_driver.h"

#include <stddef.h>
#include <stdint.h>

#include "HalDac.h"
#include "ProductDacConfig.h"
#include "fsl_gpio.h"
#include "fsl_spi.h"
#include "peripherals.h"
#include "pin_mux.h"

#define DAC8562_COMMAND_WRITE_INPUT              (0U)
#define DAC8562_COMMAND_WRITE_INPUT_UPDATE_ALL   (2U)
#define DAC8562_COMMAND_WRITE_INPUT_UPDATE       (3U)
#define DAC8562_COMMAND_POWER_MODE               (4U)
#define DAC8562_COMMAND_RESET                    (5U)
#define DAC8562_COMMAND_INTERNAL_REFERENCE       (7U)

#define DAC8562_ADDRESS_DAC_A                    (0U)
#define DAC8562_ADDRESS_DAC_B                    (1U)
#define DAC8562_ADDRESS_GAIN                     (2U)
#define DAC8562_ADDRESS_BOTH                     (7U)

#define DAC8562_COMMAND_SHIFT                    (3U)
#define DAC8562_GAIN_ONE_BOTH                    (3U)
#define DAC8562_POWER_UP_BOTH                    (3U)
#define DAC8562_RESET_ALL_REGISTERS              (1U)
#define DAC8562_INTERNAL_REFERENCE_ENABLE        (1U)

typedef struct
{
    uint8_t device;
    uint8_t output;
} ProductDacChannelContext_t;

static ProductDacChannelContext_t g_dac_channels[PRODUCT_DAC_CHANNEL_COUNT] =
{
    {0U, DAC8562_ADDRESS_DAC_A},
    {0U, DAC8562_ADDRESS_DAC_B},
    {1U, DAC8562_ADDRESS_DAC_A},
    {1U, DAC8562_ADDRESS_DAC_B}
};
static bool g_dac_device_initialized[PRODUCT_DAC_DEVICE_COUNT];

static void ProductDacSetSync(uint8_t device, uint8_t level)
{
    if (device == 0U)
    {
        GPIO_PinWrite(BOARD_INITEXTDACPINS_SYNC_BAR_0_GPIO,
                      BOARD_INITEXTDACPINS_SYNC_BAR_0_PORT,
                      BOARD_INITEXTDACPINS_SYNC_BAR_0_PIN, level);
    }
    else
    {
        GPIO_PinWrite(BOARD_INITEXTDACPINS_SYNC_BAR_1_GPIO,
                      BOARD_INITEXTDACPINS_SYNC_BAR_1_PORT,
                      BOARD_INITEXTDACPINS_SYNC_BAR_1_PIN, level);
    }
}

static HalDacStatus_t ProductDacTransfer(uint8_t device,
                                         uint8_t command,
                                         uint8_t address,
                                         uint16_t data)
{
    uint8_t tx_data[3];
    spi_transfer_t transfer;
    status_t status;

    if (device >= PRODUCT_DAC_DEVICE_COUNT)
    {
        return HAL_DAC_STATUS_INVALID_ARGUMENT;
    }

    tx_data[0] = (uint8_t)((command << DAC8562_COMMAND_SHIFT) | address);
    tx_data[1] = (uint8_t)(data >> 8U);
    tx_data[2] = (uint8_t)data;
    transfer.txData = tx_data;
    transfer.rxData = NULL;
    transfer.dataSize = sizeof(tx_data);
    transfer.configFlags = kSPI_FrameAssert;

    /* Both devices are deselected before selecting exactly one device. */
    ProductDacSetSync(0U, 1U);
    ProductDacSetSync(1U, 1U);
    ProductDacSetSync(device, 0U);
    status = SPI_MasterTransferBlocking(DAC_FC5_PERIPHERAL, &transfer);
    ProductDacSetSync(device, 1U);

    return (status == kStatus_Success) ?
        HAL_DAC_STATUS_OK : HAL_DAC_STATUS_IO_ERROR;
}

static HalDacStatus_t ProductDacInitializeDevice(uint8_t device)
{
    HalDacStatus_t status;

    if (device >= PRODUCT_DAC_DEVICE_COUNT)
    {
        return HAL_DAC_STATUS_INVALID_ARGUMENT;
    }
    if (g_dac_device_initialized[device])
    {
        return HAL_DAC_STATUS_OK;
    }

    GPIO_PinWrite(BOARD_INITEXTDACPINS_CLR_BAR_GPIO,
                  BOARD_INITEXTDACPINS_CLR_BAR_PORT,
                  BOARD_INITEXTDACPINS_CLR_BAR_PIN, 1U);

    status = ProductDacTransfer(device, DAC8562_COMMAND_RESET,
                                DAC8562_ADDRESS_BOTH,
                                DAC8562_RESET_ALL_REGISTERS);
    if (status != HAL_DAC_STATUS_OK)
    {
        return status;
    }
    status = ProductDacTransfer(device, DAC8562_COMMAND_INTERNAL_REFERENCE,
                                DAC8562_ADDRESS_BOTH,
                                DAC8562_INTERNAL_REFERENCE_ENABLE);
    if (status != HAL_DAC_STATUS_OK)
    {
        return status;
    }
    /* Enabling the internal reference selects gain 2; force gain 1. */
    status = ProductDacTransfer(device, DAC8562_COMMAND_WRITE_INPUT,
                                DAC8562_ADDRESS_GAIN,
                                DAC8562_GAIN_ONE_BOTH);
    if (status != HAL_DAC_STATUS_OK)
    {
        return status;
    }
    status = ProductDacTransfer(device, DAC8562_COMMAND_POWER_MODE,
                                DAC8562_ADDRESS_BOTH,
                                DAC8562_POWER_UP_BOTH);
    if (status != HAL_DAC_STATUS_OK)
    {
        return status;
    }
    status = ProductDacTransfer(device,
                                DAC8562_COMMAND_WRITE_INPUT_UPDATE_ALL,
                                DAC8562_ADDRESS_BOTH,
                                PRODUCT_DAC_INITIAL_CODE);
    if (status == HAL_DAC_STATUS_OK)
    {
        g_dac_device_initialized[device] = true;
    }
    return status;
}

static HalDacStatus_t ProductDacInitialize(void *driver_context)
{
    const ProductDacChannelContext_t *context =
        (const ProductDacChannelContext_t *)driver_context;

    return (context == NULL) ? HAL_DAC_STATUS_INVALID_ARGUMENT :
        ProductDacInitializeDevice(context->device);
}

static HalDacStatus_t ProductDacWriteCode(void *driver_context, uint16_t code)
{
    const ProductDacChannelContext_t *context =
        (const ProductDacChannelContext_t *)driver_context;

    if ((context == NULL) ||
        (context->device >= PRODUCT_DAC_DEVICE_COUNT) ||
        (context->output > DAC8562_ADDRESS_DAC_B))
    {
        return HAL_DAC_STATUS_INVALID_ARGUMENT;
    }
    return ProductDacTransfer(context->device,
                              DAC8562_COMMAND_WRITE_INPUT_UPDATE,
                              context->output, code);
}

bool ProductDac8562Driver_Init(void)
{
    static const HalDacDriverOps_t ops =
    {
        ProductDacInitialize,
        ProductDacWriteCode
    };
    uint8_t channel;

    for (channel = 0U; channel < PRODUCT_DAC_DEVICE_COUNT; channel++)
    {
        g_dac_device_initialized[channel] = false;
    }
    for (channel = 0U; channel < PRODUCT_DAC_CHANNEL_COUNT; channel++)
    {
        if (HalDac_RegisterDriver(channel, &ops,
                                  &g_dac_channels[channel]) !=
            HAL_DAC_STATUS_OK)
        {
            return false;
        }
    }
    return true;
}
