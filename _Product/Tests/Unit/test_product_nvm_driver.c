#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "HalNvm.h"
#include "fsl_spi.h"
#include "product_nvm_driver.h"

#define MOCK_FRAM_SIZE      (262144U)
#define MOCK_SLOT1_ADDRESS  (0x8000U)
#define MOCK_WEL_MASK       (0x02U)

SPI_Type g_mock_spi;
static uint8_t g_mock_fram[MOCK_FRAM_SIZE];
static uint8_t g_mock_status;
static bool g_mock_bad_device_id;

static uint32_t GetAddress(const uint8_t *data)
{
    return ((uint32_t)data[1] << 16U) |
           ((uint32_t)data[2] << 8U) |
           (uint32_t)data[3];
}

status_t SPI_MasterTransferBlocking(SPI_Type *base, spi_transfer_t *transfer)
{
    uint32_t address;
    uint32_t length;

    assert(base == &g_mock_spi);
    assert(transfer != NULL);
    assert(transfer->txData != NULL);
    if (transfer->rxData != NULL)
    {
        (void)memset(transfer->rxData, 0, transfer->dataSize);
    }

    switch (transfer->txData[0])
    {
        case 0x06U:
            g_mock_status |= MOCK_WEL_MASK;
            break;
        case 0x05U:
            assert(transfer->dataSize == 2U);
            transfer->rxData[1] = g_mock_status;
            break;
        case 0x01U:
            assert((g_mock_status & MOCK_WEL_MASK) != 0U);
            g_mock_status = (uint8_t)(transfer->txData[1] | MOCK_WEL_MASK);
            break;
        case 0x9FU:
            assert(transfer->dataSize == 5U);
            transfer->rxData[1] = g_mock_bad_device_id ? 0xFFU : 0x04U;
            transfer->rxData[2] = 0x7FU;
            transfer->rxData[3] = 0x48U;
            transfer->rxData[4] = 0x0AU;
            break;
        case 0x03U:
            address = GetAddress(transfer->txData);
            length = (uint32_t)transfer->dataSize - 4U;
            assert((address + length) <= MOCK_FRAM_SIZE);
            (void)memcpy(&transfer->rxData[4], &g_mock_fram[address], length);
            break;
        case 0x02U:
            assert((g_mock_status & MOCK_WEL_MASK) != 0U);
            address = GetAddress(transfer->txData);
            length = (uint32_t)transfer->dataSize - 4U;
            assert((address + length) <= MOCK_FRAM_SIZE);
            (void)memcpy(&g_mock_fram[address], &transfer->txData[4], length);
            break;
        default:
            assert(false);
            break;
    }
    return kStatus_Success;
}

static void ResetMock(void)
{
    (void)memset(g_mock_fram, 0xFF, sizeof(g_mock_fram));
    g_mock_status = 0U;
    g_mock_bad_device_id = false;
}

int main(void)
{
    uint8_t page[HAL_NVM_PAGE_SIZE];
    uint8_t readback[HAL_NVM_PAGE_SIZE];
    uint32_t index;

    ResetMock();
    assert(ProductNvmDriver_Init());
    assert(HalNvm_Initialize() == HAL_NVM_STATUS_OK);

    for (index = 0U; index < sizeof(page); index++)
    {
        page[index] = (uint8_t)index;
    }
    assert(HalNvm_ProgramPage(1U, 1U, page) == HAL_NVM_STATUS_OK);
    assert(HalNvm_Read(1U, HAL_NVM_PAGE_SIZE, readback,
                      sizeof(readback)) == HAL_NVM_STATUS_OK);
    assert(memcmp(page, readback, sizeof(page)) == 0);

    assert(HalNvm_EraseSlot(1U) == HAL_NVM_STATUS_OK);
    assert(g_mock_fram[MOCK_SLOT1_ADDRESS + HAL_NVM_PAGE_SIZE] == 0U);
    assert(g_mock_fram[MOCK_SLOT1_ADDRESS + HAL_NVM_PAGE_SIZE + 4U] == 4U);

    g_mock_bad_device_id = true;
    assert(ProductNvmDriver_Init());
    assert(HalNvm_Initialize() == HAL_NVM_STATUS_IO_ERROR);
    return 0;
}
