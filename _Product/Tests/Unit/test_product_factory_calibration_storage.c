#include <assert.h>
#include <string.h>
#include "FactoryCalibrationService.h"
#include "HalNvm.h"
#include "ProductNvmConfig.h"
#include "product_factory_calibration_storage.h"

static uint8_t g_memory[PRODUCT_NVM_FRAM_CAPACITY_BYTES];
static unsigned int g_write_count, g_fail_write;
static bool g_busy, g_read_fail;
bool ProductFramBankTest_IsBusy(void) { return g_busy; }
HalNvmStatus_t HalNvm_GetCapacity(uint32_t *capacity)
{ *capacity = sizeof(g_memory); return HAL_NVM_STATUS_OK; }
HalNvmStatus_t HalNvm_ReadRaw(uint32_t address, uint8_t *data, uint32_t size)
{
    if (g_read_fail || address + size > sizeof(g_memory)) return HAL_NVM_STATUS_IO_ERROR;
    memcpy(data, g_memory + address, size); return HAL_NVM_STATUS_OK;
}
HalNvmStatus_t HalNvm_WriteRaw(uint32_t address, const uint8_t *data, uint32_t size)
{
    assert(address >= PRODUCT_NVM_CALIBRATION_BASE_ADDRESS);
    assert(address + size <= PRODUCT_NVM_CALIBRATION_END_ADDRESS);
    g_write_count++;
    if (g_write_count == g_fail_write)
    {
        /* Simulate a torn transfer before power is lost. */
        memcpy(g_memory + address, data, size / 2U);
        return HAL_NVM_STATUS_IO_ERROR;
    }
    memcpy(g_memory + address, data, size); return HAL_NVM_STATUS_OK;
}
static void Boot(void)
{
    FactoryModeService_Initialize();
    FactoryCalibrationService_Initialize();
    assert(ProductFactoryCalibrationStorage_Initialize());
    FactoryCalibrationService_SetUnlockKey1(FACTORY_CALIBRATION_UNLOCK_KEY);
    FactoryCalibrationService_SetUnlockKey2(FACTORY_CALIBRATION_UNLOCK_KEY);
}
static bool Capture(uint8_t ch, uint8_t p, int32_t zero, int32_t span)
{
    assert(FactoryCalibrationService_Select(ch, (FactoryCalibrationProfile_t)p));
    FactoryCalibrationService_UpdateLiveMicrovolts(ch, zero);
    assert(FactoryCalibrationService_CaptureZero());
    FactoryCalibrationService_UpdateLiveMicrovolts(ch, span);
    assert(FactoryCalibrationService_CaptureSpan());
    return FactoryCalibrationService_Apply();
}
int main(void)
{
    FactoryCalibrationTargets_t target;
    HalAdcFactoryCalibration_t record;
    ProductFactoryCalibrationStorageStatus_t status;
    int32_t value;
    memset(g_memory, 0xFF, sizeof(g_memory));
    Boot();
    for (uint8_t ch = 0U; ch < 4U; ch++)
    for (uint8_t p = 0U; p < 10U; p++)
    {
        assert(FactoryCalibrationService_GetTargets((FactoryCalibrationProfile_t)p, &target));
        assert(!FactoryCalibrationService_IsCalibrated(ch, (FactoryCalibrationProfile_t)p));
        assert(FactoryCalibrationService_Convert(ch, (FactoryCalibrationProfile_t)p,
            1234L, &value) && value == 1234L); /* Identity before calibration. */
        assert(Capture(ch, p, target.zero_uv - 100L - ch - p,
                                  target.span_uv - 200L - ch - p));
    }
    assert(FactoryCalibrationService_GetTargets(FACTORY_CAL_PROFILE_VOLTAGE_GAIN32, &target));
    assert(target.zero_uv == 6042L && target.span_uv == 54375L);
    assert(FactoryCalibrationService_GetTargets(FACTORY_CAL_PROFILE_CURRENT_GAIN64, &target));
    assert(target.zero_uv == 3475L && target.span_uv == 31277L);
    assert(FactoryCalibrationService_GetTargets(
        FACTORY_CAL_PROFILE_TC_GAIN32, &target));
    assert(target.zero_uv == 5000L && target.span_uv == 45000L);
    assert(FactoryCalibrationService_GetTargets(
        FACTORY_CAL_PROFILE_TC_GAIN64, &target));
    assert(target.zero_uv == 3000L && target.span_uv == 27000L);
    assert(FactoryCalibrationService_GetTargets(
        FACTORY_CAL_PROFILE_TC_GAIN16, &target));
    assert(target.zero_uv == 10000L && target.span_uv == 90000L);
    assert(FactoryCalibrationService_GetTargets(
        FACTORY_CAL_PROFILE_TC_GAIN128, &target));
    assert(target.zero_uv == 1500L && target.span_uv == 13500L);
    assert(FactoryCalibrationService_GetTargets(
        FACTORY_CAL_PROFILE_RTD_GAIN8, &target));
    assert(target.zero_uv == 27858L && target.span_uv == 176643L);
    assert(FactoryCalibrationService_GetTargets(
        FACTORY_CAL_PROFILE_RTD_GAIN1, &target));
    assert(target.zero_uv == 126176L && target.span_uv == 819359L);
    assert(FactoryCalibrationService_GetTargets(
        FACTORY_CAL_PROFILE_RTD_GAIN32, &target));
    assert(target.zero_uv == 21790L && target.span_uv == 38910L);
    Boot();
    ProductFactoryCalibrationStorage_GetStatus(&status);
    assert(status.ready && status.loaded_records == 40U);
    for (uint8_t ch = 0U; ch < 4U; ch++)
    for (uint8_t p = 0U; p < 10U; p++)
    {
        assert(FactoryCalibrationService_GetTargets((FactoryCalibrationProfile_t)p, &target));
        assert(FactoryCalibrationService_GetCalibration(ch, (FactoryCalibrationProfile_t)p, &record));
        assert(record.measured_zero_uv == target.zero_uv - 100L - ch - p);
        assert(record.measured_span_uv == target.span_uv - 200L - ch - p);
        assert(FactoryCalibrationService_Convert(ch, (FactoryCalibrationProfile_t)p,
            record.measured_zero_uv, &value) && value == target.zero_uv);
        assert(FactoryCalibrationService_Convert(ch, (FactoryCalibrationProfile_t)p,
            record.measured_span_uv, &value) && value == target.span_uv);
    }
    /* Each torn write must preserve the previous committed copy on reboot. */
    for (unsigned int stage = 1U; stage <= 3U; stage++)
    {
        g_write_count = 0U; g_fail_write = stage;
        assert(!Capture(0U, 0U, 10L, 49000L));
        assert(FactoryCalibrationService_GetCalibration(0U, FACTORY_CAL_PROFILE_TC_GAIN32, &record));
        assert(record.measured_zero_uv == 4900L);
        g_fail_write = 0U;
        Boot();
        assert(FactoryCalibrationService_GetCalibration(0U, FACTORY_CAL_PROFILE_TC_GAIN32, &record));
        assert(record.measured_zero_uv == 4900L);
    }
    assert(Capture(0U, 0U, 10L, 49000L));
    Boot();
    assert(FactoryCalibrationService_GetCalibration(0U, FACTORY_CAL_PROFILE_TC_GAIN32, &record));
    assert(record.measured_zero_uv == 10L);
    /* Newest B copy corrupted: fall back to A, preserving valid other slots. */
    g_memory[PRODUCT_NVM_CALIBRATION_BASE_ADDRESS + 64U + 12U] ^= 1U;
    Boot();
    assert(FactoryCalibrationService_GetCalibration(0U, FACTORY_CAL_PROFILE_TC_GAIN32, &record));
    assert(record.measured_zero_uv == 4900L);
    ProductFactoryCalibrationStorage_GetStatus(&status);
    assert(status.corrupt_records == 1U && status.loaded_records == 40U);
    /* Unsupported format with no valid alternative uses identity defaults. */
    g_memory[PRODUCT_NVM_CALIBRATION_BASE_ADDRESS + (10U + 4U) * 128U + 4U] = 2U;
    Boot();
    assert(!FactoryCalibrationService_IsCalibrated(1U, FACTORY_CAL_PROFILE_VOLTAGE_GAIN32));
    assert(FactoryCalibrationService_Convert(1U, FACTORY_CAL_PROFILE_VOLTAGE_GAIN32,
        12345L, &value) && value == 12345L);
    ProductFactoryCalibrationStorage_GetStatus(&status);
    assert(status.loaded_records == 39U);
    g_busy = true;
    assert(!Capture(0U, 0U, 10L, 49000L));
    g_busy = false;
    g_read_fail = true;
    FactoryCalibrationService_Initialize();
    assert(!ProductFactoryCalibrationStorage_Initialize());
    assert(!Capture(0U, 0U, 10L, 49000L));
    return 0;
}
