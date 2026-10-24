/* Reuse board stubs while linking the real commands, debounce and fault code. */
#define main RunAdcBoundaryTests
#include "test_product_internal_adc_temperature_injection.c"
#undef main
#define main RunLowVoltageBoundaryTests
#include "test_product_low_voltage_safety.c"
#undef main
#include "SafetyService.h"

static uint16_t diagnostics[12];
static void ReadDiagnostics(void)
{
    assert(ProductSafetyTest_Read(0x4974U, 12U, diagnostics) == 0U);
}
static void Command(bool active, uint16_t limit, bool enabled)
{
    uint16_t values[4] = {enabled, active, limit, 0x5AFEU};
    assert(ProductSafetyTest_Write(0x4970U, values, 4U) == 0U);
    assert(ProductSafetyTest_Write(0x4970U, values, 4U) == 6U);
    ProductSafetyTest_Process();
    ReadDiagnostics();
    assert(diagnostics[0] == 2U && diagnostics[1] == enabled);
}
static void Step(unsigned timestamp)
{
    ProductLowVoltageSafety_Process(timestamp);
    ReadDiagnostics();
}
int main(void)
{
    uint16_t values[4] = {1U, 1U, 2U, 0x5AFEU};
    FaultService_Initialize();
    assert(SafetyService_Initialize(SAFETY_SOURCE_LOW_VOLTAGE, NULL, NULL));
    ProductLowVoltageSafety_Initialize();
    assert(ProductSafetyTest_Write(0x4970U, values, 4U) == 3U);
    ProductSafetyTest_SetAddressed(true);
    assert(ProductSafetyTest_Write(0x4971U, values, 4U) == 2U);
    assert(ProductSafetyTest_Write(0x4970U, values, 3U) == 2U);
    values[3]=0U;
    assert(ProductSafetyTest_Write(0x4970U, values, 4U) == 3U);
    values[3]=0x5AFEU; values[0]=2U;
    assert(ProductSafetyTest_Write(0x4970U, values, 4U) == 3U);
    assert(ProductSafetyTest_Read(0x497FU, 2U, diagnostics) == 2U);
    for (unsigned limit=1U;limit<=2U;limit++) {
        Command(true, limit, true);
        for (unsigned i=0U;i<limit;i++) {
            Step(i);
            assert(diagnostics[4] == i+1U && diagnostics[5] == i+1U);
            assert((diagnostics[7] & 3U) == 0U);
        }
        Step(limit);
        assert(diagnostics[7] == 8U); /* Finished, no live/ever trip or raw. */
        assert(FaultService_GetActiveCount() == 0U);
    }
    Command(true, 0U, true);
    Step(10U); Step(11U);
    assert(diagnostics[5] == 2U && (diagnostics[7] & 3U) == 0U);
    Step(12U);
    assert(diagnostics[5] == 3U && (diagnostics[7] & 3U) == 3U);
    assert(FaultService_IsActive(FAULT_CODE_LOW_VOLTAGE));
    Command(false, 0U, true);
    for (unsigned i=1U;i<100U;i++) {
        Step(12U+i);
        assert((diagnostics[7] & 1U) != 0U && diagnostics[6] == i);
    }
    Step(112U);
    assert((diagnostics[7] & 1U) == 0U && diagnostics[6] == 100U);
    assert(FaultService_IsActive(FAULT_CODE_LOW_VOLTAGE));
    assert(FaultService_Clear(FAULT_CODE_LOW_VOLTAGE));
    Command(false, 0U, false);
    g_low_voltage_active=true;
    Step(113U);
#if PRODUCT_SAFETY_TEST_EVK
    assert(!g_monitor.rawActive);
#else
    assert(g_monitor.rawActive); /* Disabled override restores actual input. */
#endif
    return 0;
}
