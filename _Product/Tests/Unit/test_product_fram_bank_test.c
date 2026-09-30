#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "FactoryModeService.h"
#include "HalNvm.h"
#include "NvmService.h"
#include "product_fram_bank_test.h"

#define TEST_FRAM_SIZE (262144U)

static uint8_t g_storage[TEST_FRAM_SIZE];
static bool g_corrupt_read;

static HalNvmStatus_t MockInitialize(void *context)
{
    (void)context;
    return HAL_NVM_STATUS_OK;
}

static HalNvmStatus_t MockSlotRead(void *context, uint8_t slot,
                                   uint32_t offset, uint8_t *data,
                                   uint32_t length)
{
    (void)context;
    (void)slot;
    (void)offset;
    (void)data;
    (void)length;
    return HAL_NVM_STATUS_OK;
}

static HalNvmStatus_t MockErase(void *context, uint8_t slot)
{
    (void)context;
    (void)slot;
    return HAL_NVM_STATUS_OK;
}

static HalNvmStatus_t MockProgram(void *context, uint8_t slot, uint8_t page,
                                  const uint8_t *data)
{
    (void)context;
    (void)slot;
    (void)page;
    (void)data;
    return HAL_NVM_STATUS_OK;
}

static HalNvmStatus_t MockGetCapacity(void *context, uint32_t *capacity)
{
    (void)context;
    *capacity = TEST_FRAM_SIZE;
    return HAL_NVM_STATUS_OK;
}

static HalNvmStatus_t MockReadRaw(void *context, uint32_t address,
                                  uint8_t *data, uint32_t length)
{
    (void)context;
    assert((address + length) <= sizeof(g_storage));
    (void)memcpy(data, &g_storage[address], length);
    if (g_corrupt_read)
    {
        data[0] ^= 1U;
        g_corrupt_read = false;
    }
    return HAL_NVM_STATUS_OK;
}

static HalNvmStatus_t MockWriteRaw(void *context, uint32_t address,
                                   const uint8_t *data, uint32_t length)
{
    (void)context;
    assert((address + length) <= sizeof(g_storage));
    (void)memcpy(&g_storage[address], data, length);
    return HAL_NVM_STATUS_OK;
}

NvmServiceState_t NvmService_GetState(void)
{
    return NVM_SERVICE_STATE_IDLE;
}

int main(void)
{
    static const HalNvmDriverOps_t ops =
    {
        MockInitialize,
        MockSlotRead,
        MockErase,
        MockProgram,
        MockGetCapacity,
        MockReadRaw,
        MockWriteRaw
    };
    ProductFramBankTestSnapshot_t snapshot;
    uint32_t process_count = 0U;

    assert(HalNvm_RegisterDriver(&ops, NULL) == HAL_NVM_STATUS_OK);
    assert(HalNvm_Initialize() == HAL_NVM_STATUS_OK);
    FactoryModeService_Initialize();
    ProductFramBankTest_Initialize();

    assert(!ProductFramBankTest_Start());
    ProductFramBankTest_GetSnapshot(&snapshot);
    assert(snapshot.error == PRODUCT_FRAM_TEST_ERROR_FACTORY_MODE_LOCKED);

    FactoryModeService_SetUnlockKey1(FACTORY_MODE_UNLOCK_KEY);
    FactoryModeService_SetUnlockKey2(FACTORY_MODE_UNLOCK_KEY);
    assert(ProductFramBankTest_Start());
    while (ProductFramBankTest_IsBusy())
    {
        ProductFramBankTest_Process();
        process_count++;
        assert(process_count <= 2048U);
    }
    ProductFramBankTest_GetSnapshot(&snapshot);
    assert(snapshot.state == PRODUCT_FRAM_TEST_STATE_PASSED);
    assert(snapshot.completed_bank_mask == 0x000FU);
    assert(snapshot.failed_bank_mask == 0U);
    assert(snapshot.progress_permille == 1000U);
    assert(g_storage[0] == 0x5AU);
    assert(g_storage[2] == 0xA5U);

    assert(ProductFramBankTest_Start());
    ProductFramBankTest_Process();
    while (snapshot.state != PRODUCT_FRAM_TEST_STATE_VERIFY_PATTERN_1)
    {
        ProductFramBankTest_GetSnapshot(&snapshot);
        if (snapshot.state != PRODUCT_FRAM_TEST_STATE_VERIFY_PATTERN_1)
        {
            ProductFramBankTest_Process();
        }
    }
    g_corrupt_read = true;
    ProductFramBankTest_Process();
    ProductFramBankTest_GetSnapshot(&snapshot);
    assert(snapshot.state == PRODUCT_FRAM_TEST_STATE_FAILED);
    assert(snapshot.error == PRODUCT_FRAM_TEST_ERROR_VERIFY);
    assert(snapshot.failed_bank_mask == 0x0001U);
    assert(snapshot.expected_value != snapshot.actual_value);
    return 0;
}
