#include "product_nvm_driver.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "HalNvm.h"
#include "ProductNvmConfig.h"
#include "peripherals.h"
#include "fsl_spi.h"

/* RAMXEED MB85RS2MTY: 2 Mbit (256 KiB) SPI FeRAM. */
#define PRODUCT_NVM_OPCODE_WREN               (0x06U)
#define PRODUCT_NVM_OPCODE_RDSR               (0x05U)
#define PRODUCT_NVM_OPCODE_WRSR               (0x01U)
#define PRODUCT_NVM_OPCODE_READ               (0x03U)
#define PRODUCT_NVM_OPCODE_WRITE              (0x02U)
#define PRODUCT_NVM_OPCODE_RDID               (0x9FU)

#define PRODUCT_NVM_STATUS_BP_MASK            (0x0CU)
#define PRODUCT_NVM_STATUS_WEL_MASK           (0x02U)

#define PRODUCT_NVM_DEVICE_ID_LENGTH          (4U)
#define PRODUCT_NVM_COMMAND_HEADER_SIZE       (4U)
#define PRODUCT_NVM_MAX_PAYLOAD_SIZE          (HAL_NVM_PAGE_SIZE)
#define PRODUCT_NVM_TRANSFER_BUFFER_SIZE      \
    (PRODUCT_NVM_COMMAND_HEADER_SIZE + PRODUCT_NVM_MAX_PAYLOAD_SIZE)

typedef struct
{
    uint8_t device_id[PRODUCT_NVM_DEVICE_ID_LENGTH];
} ProductNvmDriverContext_t;

static ProductNvmDriverContext_t g_nvm_context;
static uint8_t g_nvm_tx_buffer[PRODUCT_NVM_TRANSFER_BUFFER_SIZE];
static uint8_t g_nvm_rx_buffer[PRODUCT_NVM_TRANSFER_BUFFER_SIZE];

static uint32_t ProductNvmSlotAddress(uint8_t slot)
{
    return (slot == 0U) ? PRODUCT_NVM_SLOT0_ADDRESS :
                          PRODUCT_NVM_SLOT1_ADDRESS;
}

static bool ProductNvmTransfer(uint8_t *tx_data, uint8_t *rx_data,
                               uint32_t length)
{
    spi_transfer_t transfer;

    if ((length == 0U) || ((tx_data == NULL) && (rx_data == NULL)))
    {
        return false;
    }

    transfer.txData = tx_data;
    transfer.rxData = rx_data;
    transfer.dataSize = length;
    transfer.configFlags = (uint32_t)kSPI_FrameAssert;
    return SPI_MasterTransferBlocking(MEM_FC7_PERIPHERAL, &transfer) ==
           kStatus_Success;
}

static bool ProductNvmWriteEnable(void)
{
    uint8_t command = PRODUCT_NVM_OPCODE_WREN;
    return ProductNvmTransfer(&command, NULL, 1U);
}

static bool ProductNvmReadStatus(uint8_t *status_register)
{
    uint8_t tx_data[2] = {PRODUCT_NVM_OPCODE_RDSR, 0xFFU};
    uint8_t rx_data[2] = {0U, 0U};

    if ((status_register == NULL) ||
        !ProductNvmTransfer(tx_data, rx_data, sizeof(tx_data)))
    {
        return false;
    }
    *status_register = rx_data[1];
    return true;
}

static bool ProductNvmWriteStatus(uint8_t status_register)
{
    uint8_t command[2] = {PRODUCT_NVM_OPCODE_WRSR, status_register};

    return ProductNvmWriteEnable() &&
           ProductNvmTransfer(command, NULL, sizeof(command));
}

static bool ProductNvmReadDeviceId(uint8_t *device_id)
{
    uint8_t tx_data[1U + PRODUCT_NVM_DEVICE_ID_LENGTH];
    uint8_t rx_data[1U + PRODUCT_NVM_DEVICE_ID_LENGTH];

    if (device_id == NULL)
    {
        return false;
    }
    (void)memset(tx_data, 0xFF, sizeof(tx_data));
    (void)memset(rx_data, 0, sizeof(rx_data));
    tx_data[0] = PRODUCT_NVM_OPCODE_RDID;
    if (!ProductNvmTransfer(tx_data, rx_data, sizeof(tx_data)))
    {
        return false;
    }
    (void)memcpy(device_id, &rx_data[1], PRODUCT_NVM_DEVICE_ID_LENGTH);
    return true;
}

static bool ProductNvmDeviceIdIsValid(const uint8_t *device_id)
{
    return (device_id != NULL) &&
           (device_id[0] == PRODUCT_NVM_MANUFACTURER_ID) &&
           (device_id[1] == PRODUCT_NVM_CONTINUATION_CODE) &&
           (device_id[2] == PRODUCT_NVM_PRODUCT_ID_1) &&
           (device_id[3] == PRODUCT_NVM_PRODUCT_ID_2);
}

static void ProductNvmBuildAddressCommand(uint8_t opcode, uint32_t address)
{
    address &= PRODUCT_NVM_ADDRESS_MASK;
    g_nvm_tx_buffer[0] = opcode;
    g_nvm_tx_buffer[1] = (uint8_t)(address >> 16U);
    g_nvm_tx_buffer[2] = (uint8_t)(address >> 8U);
    g_nvm_tx_buffer[3] = (uint8_t)address;
}

static bool ProductNvmReadBytes(uint32_t address, uint8_t *data,
                                uint32_t length)
{
    uint32_t chunk;

    if ((data == NULL) || (length == 0U) ||
        (address >= PRODUCT_NVM_FRAM_CAPACITY_BYTES) ||
        (length > (PRODUCT_NVM_FRAM_CAPACITY_BYTES - address)))
    {
        return false;
    }

    while (length != 0U)
    {
        chunk = (length > PRODUCT_NVM_MAX_PAYLOAD_SIZE) ?
                    PRODUCT_NVM_MAX_PAYLOAD_SIZE : length;
        ProductNvmBuildAddressCommand(PRODUCT_NVM_OPCODE_READ, address);
        (void)memset(&g_nvm_tx_buffer[PRODUCT_NVM_COMMAND_HEADER_SIZE],
                     0xFF, chunk);
        (void)memset(g_nvm_rx_buffer, 0,
                     PRODUCT_NVM_COMMAND_HEADER_SIZE + chunk);
        if (!ProductNvmTransfer(g_nvm_tx_buffer, g_nvm_rx_buffer,
                               PRODUCT_NVM_COMMAND_HEADER_SIZE + chunk))
        {
            return false;
        }
        (void)memcpy(data,
                     &g_nvm_rx_buffer[PRODUCT_NVM_COMMAND_HEADER_SIZE], chunk);
        address += chunk;
        data += chunk;
        length -= chunk;
    }
    return true;
}

static bool ProductNvmWriteBytes(uint32_t address, const uint8_t *data,
                                 uint32_t length)
{
    uint32_t chunk;
    uint8_t status_register;

    if ((data == NULL) || (length == 0U) ||
        (address >= PRODUCT_NVM_FRAM_CAPACITY_BYTES) ||
        (length > (PRODUCT_NVM_FRAM_CAPACITY_BYTES - address)))
    {
        return false;
    }

    while (length != 0U)
    {
        chunk = (length > PRODUCT_NVM_MAX_PAYLOAD_SIZE) ?
                    PRODUCT_NVM_MAX_PAYLOAD_SIZE : length;
        if (!ProductNvmWriteEnable() ||
            !ProductNvmReadStatus(&status_register) ||
            ((status_register & PRODUCT_NVM_STATUS_WEL_MASK) == 0U))
        {
            return false;
        }
        ProductNvmBuildAddressCommand(PRODUCT_NVM_OPCODE_WRITE, address);
        (void)memcpy(&g_nvm_tx_buffer[PRODUCT_NVM_COMMAND_HEADER_SIZE], data,
                     chunk);
        if (!ProductNvmTransfer(g_nvm_tx_buffer, NULL,
                               PRODUCT_NVM_COMMAND_HEADER_SIZE + chunk))
        {
            return false;
        }
        address += chunk;
        data += chunk;
        length -= chunk;
    }
    return true;
}

static HalNvmStatus_t ProductNvmInitialize(void *context)
{
    ProductNvmDriverContext_t *driver =
        (ProductNvmDriverContext_t *)context;
    uint8_t status_register;

    if (driver == NULL)
    {
        return HAL_NVM_STATUS_INVALID_ARGUMENT;
    }
    if (!ProductNvmReadDeviceId(driver->device_id) ||
        !ProductNvmDeviceIdIsValid(driver->device_id) ||
        !ProductNvmReadStatus(&status_register))
    {
        return HAL_NVM_STATUS_IO_ERROR;
    }

    /* Clear any persisted block-protect bits before using the data array. */
    if ((status_register & PRODUCT_NVM_STATUS_BP_MASK) != 0U)
    {
        status_register &= (uint8_t)~PRODUCT_NVM_STATUS_BP_MASK;
        if (!ProductNvmWriteStatus(status_register) ||
            !ProductNvmReadStatus(&status_register) ||
            ((status_register & PRODUCT_NVM_STATUS_BP_MASK) != 0U))
        {
            return HAL_NVM_STATUS_IO_ERROR;
        }
    }
    return HAL_NVM_STATUS_OK;
}

static HalNvmStatus_t ProductNvmRead(void *context, uint8_t slot,
                                     uint32_t offset, uint8_t *data,
                                     uint32_t length)
{
    if ((context == NULL) || (slot >= HAL_NVM_SLOT_COUNT) ||
        (data == NULL) || (length == 0U) ||
        (offset >= PRODUCT_NVM_LOGICAL_SLOT_SIZE) ||
        (length > (PRODUCT_NVM_LOGICAL_SLOT_SIZE - offset)))
    {
        return HAL_NVM_STATUS_INVALID_ARGUMENT;
    }
    return ProductNvmReadBytes(ProductNvmSlotAddress(slot) + offset,
                               data, length) ? HAL_NVM_STATUS_OK :
                                               HAL_NVM_STATUS_IO_ERROR;
}

static HalNvmStatus_t ProductNvmErase(void *context, uint8_t slot)
{
    static const uint8_t invalid_commit_magic[4] = {0U, 0U, 0U, 0U};
    uint32_t commit_address;

    if ((context == NULL) || (slot >= HAL_NVM_SLOT_COUNT))
    {
        return HAL_NVM_STATUS_INVALID_ARGUMENT;
    }

    /*
     * FeRAM has no erase operation. Invalidate the commit record first;
     * NvmService subsequently overwrites both complete 512-byte pages.
     */
    commit_address = ProductNvmSlotAddress(slot) + HAL_NVM_PAGE_SIZE;
    return ProductNvmWriteBytes(commit_address, invalid_commit_magic,
                                sizeof(invalid_commit_magic)) ?
               HAL_NVM_STATUS_OK : HAL_NVM_STATUS_IO_ERROR;
}

static HalNvmStatus_t ProductNvmProgramPage(void *context, uint8_t slot,
                                            uint8_t page,
                                            const uint8_t *data)
{
    uint32_t address;

    if ((context == NULL) || (slot >= HAL_NVM_SLOT_COUNT) ||
        (page > 1U) || (data == NULL))
    {
        return HAL_NVM_STATUS_INVALID_ARGUMENT;
    }
    address = ProductNvmSlotAddress(slot) +
              ((uint32_t)page * HAL_NVM_PAGE_SIZE);
    return ProductNvmWriteBytes(address, data, HAL_NVM_PAGE_SIZE) ?
               HAL_NVM_STATUS_OK : HAL_NVM_STATUS_IO_ERROR;
}

bool ProductNvmDriver_Init(void)
{
    static const HalNvmDriverOps_t ops =
    {
        ProductNvmInitialize,
        ProductNvmRead,
        ProductNvmErase,
        ProductNvmProgramPage
    };
    return HalNvm_RegisterDriver(&ops, &g_nvm_context) == HAL_NVM_STATUS_OK;
}
