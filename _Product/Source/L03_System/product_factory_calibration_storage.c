#include "product_factory_calibration_storage.h"
#include <stddef.h>
#include <string.h>
#include "FactoryCalibrationService.h"
#include "HalNvm.h"
#include "ProductAdcConfig.h"
#include "ProductNvmConfig.h"
#include "product_fram_bank_test.h"

#define CAL_MAGIC (0x43414C31UL)
#define CAL_VERSION (1U)
#define CAL_COMMIT (0xA55AC33CUL)
#define CAL_SIZE (36U)
#define CAL_COPY_STRIDE (64UL)
#define CAL_RECORD_STRIDE (128UL)
#define CAL_CHANNELS (4U)

_Static_assert(FACTORY_CALIBRATION_INPUT_COUNT >= CAL_CHANNELS,
               "Calibration storage requires four input slots");
_Static_assert(PRODUCT_NVM_CALIBRATION_BASE_ADDRESS >=
               PRODUCT_NVM_SLOT1_ADDRESS + PRODUCT_NVM_LOGICAL_SLOT_SIZE,
               "Calibration region overlaps configuration slots");
_Static_assert(PRODUCT_NVM_CALIBRATION_END_ADDRESS - PRODUCT_NVM_CALIBRATION_BASE_ADDRESS ==
               CAL_CHANNELS * FACTORY_CALIBRATION_PROFILE_COUNT * CAL_RECORD_STRIDE,
               "Calibration region geometry mismatch");

/* ADC-side targets; resistor units are converted explicitly to microvolts. */
static const FactoryCalibrationTargets_t g_targets[FACTORY_CALIBRATION_PROFILE_COUNT] =
{
    {0L, 50000L}, /* TC32/mV: 0 / 50 mV */
    {0L, 30000L}, /* TC64: 0 / 30 mV */
    {100L * PRODUCT_ADC_IEX_UA_PT100, 300L * PRODUCT_ADC_IEX_UA_PT100},
    {0L, 100000L}, /* TC16: 0 / 100 mV */
    {0L, (int32_t)((10000000ULL * PRODUCT_ADC_VOLTAGE_RATIO_DENOMINATOR +
          PRODUCT_ADC_VOLTAGE_RATIO_NUMERATOR / 2UL) / PRODUCT_ADC_VOLTAGE_RATIO_NUMERATOR)},
    {100L * PRODUCT_ADC_IEX_UA_JPT100, 300L * PRODUCT_ADC_IEX_UA_JPT100},
    {0L, (int32_t)((20ULL * PRODUCT_ADC_CURRENT_SHUNT_MILLIOHM *
          PRODUCT_ADC_CURRENT_RATIO_NUMERATOR + PRODUCT_ADC_CURRENT_RATIO_DENOMINATOR / 2UL) /
          PRODUCT_ADC_CURRENT_RATIO_DENOMINATOR)},
    {0L, 15000L}, /* TC128: 0 / 15 mV */
    {1000L * PRODUCT_ADC_IEX_UA_PT1000, 3000L * PRODUCT_ADC_IEX_UA_PT1000},
    {50L * PRODUCT_ADC_IEX_UA_CU50, 150L * PRODUCT_ADC_IEX_UA_CU50}
};
static ProductFactoryCalibrationStorageStatus_t g_status;

static void Put32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8U);
    p[2] = (uint8_t)(v >> 16U); p[3] = (uint8_t)(v >> 24U);
}
static uint32_t Get32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8U) |
           ((uint32_t)p[2] << 16U) | ((uint32_t)p[3] << 24U);
}
static int32_t GetSigned32(const uint8_t *p)
{
    uint32_t v = Get32(p);
    return (v <= 2147483647UL) ? (int32_t)v :
        (int32_t)((int64_t)v - 4294967296LL);
}
static uint32_t Crc32(const uint8_t *p, uint8_t length)
{
    uint32_t crc = 0xFFFFFFFFUL;
    uint8_t i, bit;
    for (i = 0U; i < length; i++)
    {
        crc ^= p[i];
        for (bit = 0U; bit < 8U; bit++)
            crc = (crc >> 1U) ^ ((crc & 1U) ? 0xEDB88320UL : 0UL);
    }
    return ~crc;
}
static uint32_t Address(uint8_t channel, uint8_t profile, uint8_t copy)
{
    return PRODUCT_NVM_CALIBRATION_BASE_ADDRESS +
        ((uint32_t)channel * FACTORY_CALIBRATION_PROFILE_COUNT + profile) *
        CAL_RECORD_STRIDE + copy * CAL_COPY_STRIDE;
}
static bool Decode(const uint8_t *p, uint8_t channel, uint8_t profile,
                   HalAdcFactoryCalibration_t *calibration, uint32_t *sequence)
{
    if ((Get32(p) != CAL_MAGIC) || (p[4] != CAL_VERSION) || (p[5] != 0U) ||
        (p[6] != channel) || (p[7] != profile) ||
        (Get32(p + 32U) != CAL_COMMIT) || (Get32(p + 28U) != Crc32(p, 28U)) ||
        (GetSigned32(p + 20U) != g_targets[profile].zero_uv) ||
        (GetSigned32(p + 24U) != g_targets[profile].span_uv)) return false;
    calibration->measured_zero_uv = GetSigned32(p + 12U);
    calibration->measured_span_uv = GetSigned32(p + 16U);
    calibration->valid = true;
    *sequence = Get32(p + 8U);
    return HalAdcMeasurement_ValidateFactoryCalibration(calibration) &&
        (calibration->measured_span_uv > calibration->measured_zero_uv);
}
/* Unsigned modular comparison, including wraparound without signed casts. */
static bool Newer(uint32_t a, uint32_t b)
{
    return (a != b) && ((uint32_t)(a - b) < 0x80000000UL);
}
static bool ReadPair(uint8_t channel, uint8_t profile, uint8_t *best,
                     HalAdcFactoryCalibration_t *calibration, uint32_t *sequence)
{
    uint8_t copy, bytes[CAL_SIZE];
    *best = 255U; *sequence = 0U;
    for (copy = 0U; copy < 2U; copy++)
    {
        HalAdcFactoryCalibration_t candidate;
        uint32_t seq;
        if (HalNvm_ReadRaw(Address(channel, profile, copy), bytes, CAL_SIZE) != HAL_NVM_STATUS_OK)
            return false;
        if (Decode(bytes, channel, profile, &candidate, &seq))
        {
            if ((*best == 255U) || Newer(seq, *sequence))
            { *best = copy; *sequence = seq; *calibration = candidate; }
        }
        else if (Get32(bytes) == CAL_MAGIC && Get32(bytes + 32U) == CAL_COMMIT)
        {
            if (g_status.corrupt_records != UINT16_MAX) g_status.corrupt_records++;
        }
    }
    return true;
}
static bool Save(uint8_t channel, FactoryCalibrationProfile_t profile,
                 const HalAdcFactoryCalibration_t *calibration)
{
    uint8_t best, bytes[CAL_SIZE] = {0}, verify[CAL_SIZE], invalid[4U] = {0};
    uint32_t seq, address;
    HalAdcFactoryCalibration_t old;
    if (!g_status.ready) return false;
    if (ProductFramBankTest_IsBusy()) { g_status.last_error = 2U; return false; }
    if ((channel >= CAL_CHANNELS) || ((unsigned int)profile >= FACTORY_CALIBRATION_PROFILE_COUNT))
        return false;
    if (!ReadPair(channel, (uint8_t)profile, &best, &old, &seq))
    { g_status.last_error = 1U; return false; }
    address = Address(channel, (uint8_t)profile, (best == 0U) ? 1U : 0U);
    Put32(bytes, CAL_MAGIC); bytes[4] = CAL_VERSION; bytes[6] = channel; bytes[7] = (uint8_t)profile;
    Put32(bytes + 8U, seq + 1U);
    Put32(bytes + 12U, (uint32_t)calibration->measured_zero_uv);
    Put32(bytes + 16U, (uint32_t)calibration->measured_span_uv);
    Put32(bytes + 20U, (uint32_t)g_targets[profile].zero_uv);
    Put32(bytes + 24U, (uint32_t)g_targets[profile].span_uv);
    Put32(bytes + 28U, Crc32(bytes, 28U)); Put32(bytes + 32U, CAL_COMMIT);
    /* Invalidate destination first, verify body, then publish commit last.
     * The other valid copy is never overwritten by this transaction. */
    if (HalNvm_WriteRaw(address + 32U, invalid, 4U) != HAL_NVM_STATUS_OK ||
        HalNvm_WriteRaw(address, bytes, 32U) != HAL_NVM_STATUS_OK ||
        HalNvm_ReadRaw(address, verify, 32U) != HAL_NVM_STATUS_OK ||
        memcmp(bytes, verify, 32U) != 0 ||
        HalNvm_WriteRaw(address + 32U, bytes + 32U, 4U) != HAL_NVM_STATUS_OK ||
        HalNvm_ReadRaw(address, verify, CAL_SIZE) != HAL_NVM_STATUS_OK ||
        memcmp(bytes, verify, CAL_SIZE) != 0)
    { g_status.last_error = 3U; return false; }
    g_status.last_error = 0U;
    return true;
}
bool ProductFactoryCalibrationStorage_Initialize(void)
{
    uint8_t channel, profile, best;
    uint32_t capacity, seq;
    HalAdcFactoryCalibration_t calibration;
    memset(&g_status, 0, sizeof(g_status));
    /* Install save gate even on initialization failure: Apply must not report
     * success when the product cannot persist the record. */
    FactoryCalibrationService_SetSaveCallback(Save);
    if (!FactoryCalibrationService_SetTargets(g_targets, FACTORY_CALIBRATION_PROFILE_COUNT) ||
        HalNvm_GetCapacity(&capacity) != HAL_NVM_STATUS_OK ||
        capacity < PRODUCT_NVM_CALIBRATION_END_ADDRESS)
    { g_status.last_error = 1U; return false; }
    for (channel = 0U; channel < CAL_CHANNELS; channel++)
    for (profile = 0U; profile < FACTORY_CALIBRATION_PROFILE_COUNT; profile++)
    {
        if (!ReadPair(channel, profile, &best, &calibration, &seq))
        { g_status.last_error = 1U; return false; }
        if (best != 255U)
        {
            if (!FactoryCalibrationService_Restore(channel,
                    (FactoryCalibrationProfile_t)profile, &calibration))
            { g_status.last_error = 1U; return false; }
            g_status.loaded_records++;
        }
    }
    g_status.ready = true;
    return true;
}
void ProductFactoryCalibrationStorage_GetStatus(ProductFactoryCalibrationStorageStatus_t *status)
{
    if (status != NULL) *status = g_status;
}
