#include "product_fram_bank_test.h"

#include <stddef.h>
#include <string.h>

#include "FactoryModeService.h"
#include "HalNvm.h"
#include "NvmService.h"
#include "ProductNvmConfig.h"

#define PRODUCT_FRAM_TEST_BANK_COUNT       (4U)
#define PRODUCT_FRAM_TEST_BANK_SIZE        (65536UL)
#define PRODUCT_FRAM_TEST_CHUNK_SIZE       (512U)
#define PRODUCT_FRAM_TEST_PATTERN_A5A5     (0xA5A5U)
#define PRODUCT_FRAM_TEST_PATTERN_5A5A     (0x5A5AU)
#define PRODUCT_FRAM_TEST_PHASE_COUNT      (4UL)

static ProductFramBankTestSnapshot_t g_snapshot;
static uint32_t g_bank_offset;
static uint8_t g_buffer[PRODUCT_FRAM_TEST_CHUNK_SIZE];

static uint8_t GetPhase(void)
{
    switch (g_snapshot.state)
    {
        case PRODUCT_FRAM_TEST_STATE_WRITE_PATTERN_1: return 0U;
        case PRODUCT_FRAM_TEST_STATE_VERIFY_PATTERN_1: return 1U;
        case PRODUCT_FRAM_TEST_STATE_WRITE_PATTERN_2: return 2U;
        case PRODUCT_FRAM_TEST_STATE_VERIFY_PATTERN_2: return 3U;
        default: return 0U;
    }
}

static uint16_t GetExpectedWord(uint32_t address, bool swapped)
{
    bool odd_word = (((address >> 1U) & 1UL) != 0UL);
    if (swapped)
    {
        odd_word = !odd_word;
    }
    return odd_word ? PRODUCT_FRAM_TEST_PATTERN_5A5A :
                      PRODUCT_FRAM_TEST_PATTERN_A5A5;
}

static void FillExpected(uint32_t address, bool swapped)
{
    uint32_t index;
    for (index = 0U; index < PRODUCT_FRAM_TEST_CHUNK_SIZE; index += 2U)
    {
        uint16_t value = GetExpectedWord(address + index, swapped);
        g_buffer[index] = (uint8_t)(value >> 8U);
        g_buffer[index + 1U] = (uint8_t)value;
    }
}

static void UpdateProgress(void)
{
    uint32_t completed_bytes =
        ((((uint32_t)g_snapshot.current_bank * PRODUCT_FRAM_TEST_PHASE_COUNT) +
          GetPhase()) * PRODUCT_FRAM_TEST_BANK_SIZE) + g_bank_offset;
    uint32_t total_bytes = PRODUCT_NVM_FRAM_CAPACITY_BYTES *
                           PRODUCT_FRAM_TEST_PHASE_COUNT;
    g_snapshot.progress_permille =
        (uint16_t)((completed_bytes * 1000UL) / total_bytes);
}

static void Fail(ProductFramBankTestError_t error, uint32_t address,
                 uint16_t expected, uint16_t actual)
{
    g_snapshot.error = error;
    g_snapshot.state = PRODUCT_FRAM_TEST_STATE_FAILED;
    g_snapshot.failed_bank_mask |=
        (uint16_t)(1UL << g_snapshot.current_bank);
    g_snapshot.failure_address = address;
    g_snapshot.expected_value = expected;
    g_snapshot.actual_value = actual;
}

static void AdvancePhase(void)
{
    g_bank_offset = 0U;
    switch (g_snapshot.state)
    {
        case PRODUCT_FRAM_TEST_STATE_WRITE_PATTERN_1:
            g_snapshot.state = PRODUCT_FRAM_TEST_STATE_VERIFY_PATTERN_1;
            break;
        case PRODUCT_FRAM_TEST_STATE_VERIFY_PATTERN_1:
            g_snapshot.state = PRODUCT_FRAM_TEST_STATE_WRITE_PATTERN_2;
            break;
        case PRODUCT_FRAM_TEST_STATE_WRITE_PATTERN_2:
            g_snapshot.state = PRODUCT_FRAM_TEST_STATE_VERIFY_PATTERN_2;
            break;
        case PRODUCT_FRAM_TEST_STATE_VERIFY_PATTERN_2:
            g_snapshot.completed_bank_mask |=
                (uint16_t)(1UL << g_snapshot.current_bank);
            g_snapshot.current_bank++;
            if (g_snapshot.current_bank >= PRODUCT_FRAM_TEST_BANK_COUNT)
            {
                g_snapshot.current_bank = PRODUCT_FRAM_TEST_BANK_COUNT - 1U;
                g_snapshot.current_address =
                    PRODUCT_NVM_FRAM_CAPACITY_BYTES - 1UL;
                g_snapshot.progress_permille = 1000U;
                g_snapshot.state = PRODUCT_FRAM_TEST_STATE_PASSED;
            }
            else
            {
                g_snapshot.state = PRODUCT_FRAM_TEST_STATE_WRITE_PATTERN_1;
            }
            break;
        default:
            break;
    }
}

void ProductFramBankTest_Initialize(void)
{
    (void)memset(&g_snapshot, 0, sizeof(g_snapshot));
    g_snapshot.state = PRODUCT_FRAM_TEST_STATE_IDLE;
    g_bank_offset = 0U;
}

bool ProductFramBankTest_Start(void)
{
    uint32_t capacity = 0U;
    NvmServiceState_t nvm_state = NvmService_GetState();

    if (ProductFramBankTest_IsBusy())
    {
        return false;
    }
    if (!FactoryModeService_IsActive())
    {
        Fail(PRODUCT_FRAM_TEST_ERROR_FACTORY_MODE_LOCKED, 0U, 0U, 0U);
        return false;
    }
    if ((nvm_state != NVM_SERVICE_STATE_IDLE) &&
        (nvm_state != NVM_SERVICE_STATE_COMPLETE))
    {
        Fail(PRODUCT_FRAM_TEST_ERROR_NVM_BUSY, 0U, 0U, 0U);
        return false;
    }
    if ((HalNvm_GetCapacity(&capacity) != HAL_NVM_STATUS_OK) ||
        (capacity != PRODUCT_NVM_FRAM_CAPACITY_BYTES) ||
        (capacity != (PRODUCT_FRAM_TEST_BANK_COUNT *
                      PRODUCT_FRAM_TEST_BANK_SIZE)))
    {
        Fail(PRODUCT_FRAM_TEST_ERROR_GEOMETRY, 0U,
             (uint16_t)(PRODUCT_NVM_FRAM_CAPACITY_BYTES >> 16U),
             (uint16_t)(capacity >> 16U));
        return false;
    }

    (void)memset(&g_snapshot, 0, sizeof(g_snapshot));
    g_snapshot.state = PRODUCT_FRAM_TEST_STATE_WRITE_PATTERN_1;
    g_bank_offset = 0U;
    return true;
}

void ProductFramBankTest_Abort(void)
{
    if (ProductFramBankTest_IsBusy())
    {
        g_snapshot.state = PRODUCT_FRAM_TEST_STATE_ABORTED;
    }
}

void ProductFramBankTest_Process(void)
{
    uint32_t address;
    uint32_t index;
    bool swapped;

    if (!ProductFramBankTest_IsBusy())
    {
        return;
    }
    address = ((uint32_t)g_snapshot.current_bank *
               PRODUCT_FRAM_TEST_BANK_SIZE) + g_bank_offset;
    g_snapshot.current_address = address;
    swapped = (g_snapshot.state == PRODUCT_FRAM_TEST_STATE_WRITE_PATTERN_2) ||
              (g_snapshot.state == PRODUCT_FRAM_TEST_STATE_VERIFY_PATTERN_2);

    if ((g_snapshot.state == PRODUCT_FRAM_TEST_STATE_WRITE_PATTERN_1) ||
        (g_snapshot.state == PRODUCT_FRAM_TEST_STATE_WRITE_PATTERN_2))
    {
        FillExpected(address, swapped);
        if (HalNvm_WriteRaw(address, g_buffer, sizeof(g_buffer)) !=
            HAL_NVM_STATUS_OK)
        {
            Fail(PRODUCT_FRAM_TEST_ERROR_WRITE, address,
                 GetExpectedWord(address, swapped), 0U);
            return;
        }
    }
    else
    {
        if (HalNvm_ReadRaw(address, g_buffer, sizeof(g_buffer)) !=
            HAL_NVM_STATUS_OK)
        {
            Fail(PRODUCT_FRAM_TEST_ERROR_READ, address,
                 GetExpectedWord(address, swapped), 0U);
            return;
        }
        for (index = 0U; index < sizeof(g_buffer); index += 2U)
        {
            uint16_t expected = GetExpectedWord(address + index, swapped);
            uint16_t actual = ((uint16_t)g_buffer[index] << 8U) |
                              g_buffer[index + 1U];
            if (actual != expected)
            {
                Fail(PRODUCT_FRAM_TEST_ERROR_VERIFY, address + index,
                     expected, actual);
                return;
            }
        }
    }

    g_bank_offset += PRODUCT_FRAM_TEST_CHUNK_SIZE;
    UpdateProgress();
    if (g_bank_offset >= PRODUCT_FRAM_TEST_BANK_SIZE)
    {
        AdvancePhase();
    }
}

bool ProductFramBankTest_IsBusy(void)
{
    return (g_snapshot.state >= PRODUCT_FRAM_TEST_STATE_WRITE_PATTERN_1) &&
           (g_snapshot.state <= PRODUCT_FRAM_TEST_STATE_VERIFY_PATTERN_2);
}

void ProductFramBankTest_GetSnapshot(ProductFramBankTestSnapshot_t *snapshot)
{
    if (snapshot != NULL)
    {
        *snapshot = g_snapshot;
    }
}
