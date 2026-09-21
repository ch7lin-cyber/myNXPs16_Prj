#include "product_nvm_driver.h"

#include <stddef.h>
#include <stdint.h>

#include "HalNvm.h"
#include "fsl_iap.h"

#define PRODUCT_NVM_SLOT_SIZE       (32768UL)
#define PRODUCT_NVM_SLOT0_ADDRESS   (0x00028000UL)
#define PRODUCT_NVM_SLOT1_ADDRESS   (0x00030000UL)

typedef struct
{
    flash_config_t flash_config;
} ProductNvmDriverContext_t;

static ProductNvmDriverContext_t g_nvm_context;

static uint32_t SlotAddress(uint8_t slot)
{
    return (slot == 0U) ? PRODUCT_NVM_SLOT0_ADDRESS :
                          PRODUCT_NVM_SLOT1_ADDRESS;
}

static HalNvmStatus_t ProductNvmInitialize(void *context)
{
    ProductNvmDriverContext_t *driver =
        (ProductNvmDriverContext_t *)context;
    uint32_t sector_size;
    uint32_t page_size;
    uint32_t total_size;

    if ((driver == NULL) ||
        (FLASH_Init(&driver->flash_config) != kStatus_FLASH_Success) ||
        (FLASH_GetProperty(&driver->flash_config,
                           kFLASH_PropertyPflashSectorSize,
                           &sector_size) != kStatus_FLASH_Success) ||
        (FLASH_GetProperty(&driver->flash_config,
                           kFLASH_PropertyPflashPageSize,
                           &page_size) != kStatus_FLASH_Success) ||
        (FLASH_GetProperty(&driver->flash_config,
                           kFLASH_PropertyPflashTotalSize,
                           &total_size) != kStatus_FLASH_Success) ||
        (sector_size != PRODUCT_NVM_SLOT_SIZE) ||
        (page_size != HAL_NVM_PAGE_SIZE) ||
        (total_size < (PRODUCT_NVM_SLOT1_ADDRESS + PRODUCT_NVM_SLOT_SIZE)))
    {
        return HAL_NVM_STATUS_IO_ERROR;
    }
    return HAL_NVM_STATUS_OK;
}

static HalNvmStatus_t ProductNvmRead(
    void *context, uint8_t slot, uint32_t offset,
    uint8_t *data, uint32_t length)
{
    ProductNvmDriverContext_t *driver =
        (ProductNvmDriverContext_t *)context;
    if ((driver == NULL) || ((offset + length) > PRODUCT_NVM_SLOT_SIZE))
    {
        return HAL_NVM_STATUS_INVALID_ARGUMENT;
    }
    return (FLASH_Read(&driver->flash_config, SlotAddress(slot) + offset,
                       data, length) == kStatus_FLASH_Success) ?
           HAL_NVM_STATUS_OK : HAL_NVM_STATUS_IO_ERROR;
}

static HalNvmStatus_t ProductNvmErase(void *context, uint8_t slot)
{
    ProductNvmDriverContext_t *driver =
        (ProductNvmDriverContext_t *)context;
    if (driver == NULL)
    {
        return HAL_NVM_STATUS_INVALID_ARGUMENT;
    }
    return (FLASH_Erase(&driver->flash_config, SlotAddress(slot),
                        PRODUCT_NVM_SLOT_SIZE, kFLASH_ApiEraseKey) ==
            kStatus_FLASH_Success) ? HAL_NVM_STATUS_OK :
                                     HAL_NVM_STATUS_IO_ERROR;
}

static HalNvmStatus_t ProductNvmProgramPage(
    void *context, uint8_t slot, uint8_t page, const uint8_t *data)
{
    ProductNvmDriverContext_t *driver =
        (ProductNvmDriverContext_t *)context;
    uint32_t address = SlotAddress(slot) +
                       ((uint32_t)page * HAL_NVM_PAGE_SIZE);
    if ((driver == NULL) || (data == NULL))
    {
        return HAL_NVM_STATUS_INVALID_ARGUMENT;
    }
    return (FLASH_Program(&driver->flash_config, address, (uint8_t *)data,
                          HAL_NVM_PAGE_SIZE) == kStatus_FLASH_Success) ?
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
