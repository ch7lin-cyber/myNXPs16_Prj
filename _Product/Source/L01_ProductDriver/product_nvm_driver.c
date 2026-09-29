#include "product_nvm_driver.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "HalNvm.h"
#include "fsl_clock.h"
#include "fsl_iap.h"
#include "fsl_power.h"

#define PRODUCT_NVM_SLOT_SIZE       (32768UL)
#define PRODUCT_NVM_SLOT0_ADDRESS   (0x00028000UL)
#define PRODUCT_NVM_SLOT1_ADDRESS   (0x00030000UL)
#define PRODUCT_NVM_IAP_CLOCK_HZ    (96000000UL)
#define PRODUCT_NVM_RUN_CLOCK_HZ    (150000000UL)
#define PRODUCT_NVM_SECURE_PRIVILEGED_RULE (3UL)
#define PRODUCT_NVM_ALL_REGION_RULES       (0x33333333UL)

typedef struct
{
    flash_config_t flash_config;
} ProductNvmDriverContext_t;

static ProductNvmDriverContext_t g_nvm_context;
static uint8_t g_nvm_program_page[HAL_NVM_PAGE_SIZE]
    __attribute__((aligned(HAL_NVM_PAGE_SIZE)));

typedef struct
{
    uint32_t primask;
    bool restore_run_clock;
} ProductNvmIapGuard_t;

static bool ProductNvmConfigureIapSecurity(void)
{
    uint32_t value;
    uint32_t index;

    /*
     * This image executes in the Secure state.  The LPC55S16 ROM flash API
     * is reached through the Secure ROM alias (0x1301FE00), so the Boot ROM,
     * application flash and the controllers used by the ROM routine must be
     * Secure/Privileged before dereferencing the ROM API table.
     */
    value = AHB_SECURE_CTRL->SEC_CTRL_FLASH_ROM[0].SLAVE_RULE;
    value &= ~(AHB_SECURE_CTRL_SEC_CTRL_FLASH_ROM_SLAVE_RULE_FLASH_RULE_MASK |
               AHB_SECURE_CTRL_SEC_CTRL_FLASH_ROM_SLAVE_RULE_ROM_RULE_MASK);
    value |= AHB_SECURE_CTRL_SEC_CTRL_FLASH_ROM_SLAVE_RULE_FLASH_RULE(
                 PRODUCT_NVM_SECURE_PRIVILEGED_RULE) |
             AHB_SECURE_CTRL_SEC_CTRL_FLASH_ROM_SLAVE_RULE_ROM_RULE(
                 PRODUCT_NVM_SECURE_PRIVILEGED_RULE);
    AHB_SECURE_CTRL->SEC_CTRL_FLASH_ROM[0].SLAVE_RULE = value;
    AHB_SECURE_CTRL->SEC_CTRL_FLASH_ROM[0].SEC_CTRL_FLASH_MEM_RULE[0] =
        PRODUCT_NVM_ALL_REGION_RULES;
    for (index = 0U; index < 4U; index++)
    {
        AHB_SECURE_CTRL->SEC_CTRL_FLASH_ROM[0].SEC_CTRL_ROM_MEM_RULE[index] =
            PRODUCT_NVM_ALL_REGION_RULES;
    }

    value = AHB_SECURE_CTRL->SEC_CTRL_APB_BRIDGE[0].SLAVE_RULE;
    value &= ~(AHB_SECURE_CTRL_SEC_CTRL_APB_BRIDGE_SLAVE_RULE_APBBRIDGE0_RULE_MASK |
               AHB_SECURE_CTRL_SEC_CTRL_APB_BRIDGE_SLAVE_RULE_APBBRIDGE1_RULE_MASK);
    value |= AHB_SECURE_CTRL_SEC_CTRL_APB_BRIDGE_SLAVE_RULE_APBBRIDGE0_RULE(
                 PRODUCT_NVM_SECURE_PRIVILEGED_RULE) |
             AHB_SECURE_CTRL_SEC_CTRL_APB_BRIDGE_SLAVE_RULE_APBBRIDGE1_RULE(
                 PRODUCT_NVM_SECURE_PRIVILEGED_RULE);
    AHB_SECURE_CTRL->SEC_CTRL_APB_BRIDGE[0].SLAVE_RULE = value;

    value = AHB_SECURE_CTRL->SEC_CTRL_APB_BRIDGE[0].SEC_CTRL_APB_BRIDGE0_MEM_CTRL0;
    value &= ~AHB_SECURE_CTRL_SEC_CTRL_APB_BRIDGE0_MEM_CTRL0_SYSCON_RULE_MASK;
    value |= AHB_SECURE_CTRL_SEC_CTRL_APB_BRIDGE0_MEM_CTRL0_SYSCON_RULE(
        PRODUCT_NVM_SECURE_PRIVILEGED_RULE);
    AHB_SECURE_CTRL->SEC_CTRL_APB_BRIDGE[0].SEC_CTRL_APB_BRIDGE0_MEM_CTRL0 = value;

    value = AHB_SECURE_CTRL->SEC_CTRL_APB_BRIDGE[0].SEC_CTRL_APB_BRIDGE1_MEM_CTRL0;
    value &= ~AHB_SECURE_CTRL_SEC_CTRL_APB_BRIDGE1_MEM_CTRL0_SYSCTRL_RULE_MASK;
    value |= AHB_SECURE_CTRL_SEC_CTRL_APB_BRIDGE1_MEM_CTRL0_SYSCTRL_RULE(
        PRODUCT_NVM_SECURE_PRIVILEGED_RULE);
    AHB_SECURE_CTRL->SEC_CTRL_APB_BRIDGE[0].SEC_CTRL_APB_BRIDGE1_MEM_CTRL0 = value;

    value = AHB_SECURE_CTRL->SEC_CTRL_APB_BRIDGE[0].SEC_CTRL_APB_BRIDGE1_MEM_CTRL2;
    value &= ~AHB_SECURE_CTRL_SEC_CTRL_APB_BRIDGE1_MEM_CTRL2_FLASH_CTRL_RULE_MASK;
    value |= AHB_SECURE_CTRL_SEC_CTRL_APB_BRIDGE1_MEM_CTRL2_FLASH_CTRL_RULE(
        PRODUCT_NVM_SECURE_PRIVILEGED_RULE);
    AHB_SECURE_CTRL->SEC_CTRL_APB_BRIDGE[0].SEC_CTRL_APB_BRIDGE1_MEM_CTRL2 = value;

    __DSB();
    __ISB();
    POWER_DisablePD(kPDRUNCFG_PD_ROM);

    value = AHB_SECURE_CTRL->SEC_CTRL_FLASH_ROM[0].SLAVE_RULE;
    if (((value &
          (AHB_SECURE_CTRL_SEC_CTRL_FLASH_ROM_SLAVE_RULE_FLASH_RULE_MASK |
           AHB_SECURE_CTRL_SEC_CTRL_FLASH_ROM_SLAVE_RULE_ROM_RULE_MASK)) !=
         (AHB_SECURE_CTRL_SEC_CTRL_FLASH_ROM_SLAVE_RULE_FLASH_RULE(
              PRODUCT_NVM_SECURE_PRIVILEGED_RULE) |
          AHB_SECURE_CTRL_SEC_CTRL_FLASH_ROM_SLAVE_RULE_ROM_RULE(
              PRODUCT_NVM_SECURE_PRIVILEGED_RULE))) ||
        (AHB_SECURE_CTRL->SEC_CTRL_FLASH_ROM[0].SEC_CTRL_FLASH_MEM_RULE[0] !=
         PRODUCT_NVM_ALL_REGION_RULES))
    {
        return false;
    }
    for (index = 0U; index < 4U; index++)
    {
        if (AHB_SECURE_CTRL->SEC_CTRL_FLASH_ROM[0].SEC_CTRL_ROM_MEM_RULE[index] !=
            PRODUCT_NVM_ALL_REGION_RULES)
        {
            return false;
        }
    }
    return true;
}

static ProductNvmIapGuard_t ProductNvmEnterIap(void)
{
    ProductNvmIapGuard_t guard;

    guard.primask = __get_PRIMASK();
    __disable_irq();
    guard.restore_run_clock = (SystemCoreClock != PRODUCT_NVM_IAP_CLOCK_HZ);
    if (guard.restore_run_clock)
    {
        CLOCK_AttachClk(kFRO_HF_to_MAIN_CLK);
        SystemCoreClock = PRODUCT_NVM_IAP_CLOCK_HZ;
        CLOCK_SetFLASHAccessCyclesForFreq(PRODUCT_NVM_IAP_CLOCK_HZ);
    }
    return guard;
}

static void ProductNvmLeaveIap(ProductNvmIapGuard_t guard)
{
    if (guard.restore_run_clock)
    {
        CLOCK_SetFLASHAccessCyclesForFreq(PRODUCT_NVM_RUN_CLOCK_HZ);
        CLOCK_AttachClk(kPLL0_to_MAIN_CLK);
        SystemCoreClock = PRODUCT_NVM_RUN_CLOCK_HZ;
    }
    if (guard.primask == 0U)
    {
        __enable_irq();
    }
}

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
    status_t status;

    if (driver == NULL)
    {
        return HAL_NVM_STATUS_INVALID_ARGUMENT;
    }
    if (!ProductNvmConfigureIapSecurity())
    {
        return HAL_NVM_STATUS_IO_ERROR;
    }
    ProductNvmIapGuard_t guard = ProductNvmEnterIap();
    status = FLASH_Init(&driver->flash_config);
    ProductNvmLeaveIap(guard);

    if ((status != kStatus_FLASH_Success) ||
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
    ProductNvmIapGuard_t guard = ProductNvmEnterIap();
    status_t status = FLASH_Read(&driver->flash_config,
                                 SlotAddress(slot) + offset, data, length);
    ProductNvmLeaveIap(guard);
    return (status == kStatus_FLASH_Success) ? HAL_NVM_STATUS_OK :
                                              HAL_NVM_STATUS_IO_ERROR;
}

static HalNvmStatus_t ProductNvmErase(void *context, uint8_t slot)
{
    ProductNvmDriverContext_t *driver =
        (ProductNvmDriverContext_t *)context;
    if (driver == NULL)
    {
        return HAL_NVM_STATUS_INVALID_ARGUMENT;
    }
    ProductNvmIapGuard_t guard = ProductNvmEnterIap();
    status_t status = FLASH_Erase(&driver->flash_config, SlotAddress(slot),
                                  PRODUCT_NVM_SLOT_SIZE,
                                  kFLASH_ApiEraseKey);
    ProductNvmLeaveIap(guard);
    return (status == kStatus_FLASH_Success) ? HAL_NVM_STATUS_OK :
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
    (void)memcpy(g_nvm_program_page, data, HAL_NVM_PAGE_SIZE);
    ProductNvmIapGuard_t guard = ProductNvmEnterIap();
    status_t status = FLASH_Program(&driver->flash_config, address,
                                    g_nvm_program_page,
                                    HAL_NVM_PAGE_SIZE);
    ProductNvmLeaveIap(guard);
    return (status == kStatus_FLASH_Success) ? HAL_NVM_STATUS_OK :
                                              HAL_NVM_STATUS_IO_ERROR;
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
