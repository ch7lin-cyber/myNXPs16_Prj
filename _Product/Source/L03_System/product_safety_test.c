#include "product_safety_test.h"
#ifdef PRODUCT_SAFETY_COMMAND_ENABLED
#include "product_internal_adc_driver.h"
#include "ProductLowVoltageConfig.h"
#include <stddef.h>
/* Managed linker/startup includes this zero-initialized state in .bss_RAM3. */
static struct {
    bool addressed, pending;
    uint16_t command[3], applied[3], status;
    bool lv_pending;
    uint16_t lv_command[3], lv_applied[3], lv_status, lv_remaining;
    uint16_t lv_processed, lv_peak_assert, lv_peak_release, lv_flags;
    uint32_t lv_sequence;
} state
#if defined(__GNUC__)
    __attribute__((section(".bss.$SRAMX")))
#endif
    ;
void ProductSafetyTest_SetAddressed(bool value) { state.addressed = value; }
uint8_t ProductSafetyTest_Write(uint16_t address, const uint16_t *v, uint16_t count)
{
    if (!state.addressed) return 3;
    if (address == 0x4970U && count == 4U) {
        if (!v || v[0] > 1U || v[1] > 1U || v[3] != 0x5AFEU) return 3;
        if (state.lv_pending) return 6;
        for (uint16_t i=0;i<3U;i++) state.lv_command[i]=v[i];
        state.lv_status=1U; state.lv_pending=true;
        return 0;
    }
    if (address != 0x4960U || count != 4U) return 2;
    if (v == NULL || v[0] > 1U || v[1] > 1U || v[3] != 0x5AFEU) return 3;
    if (state.pending) return 6;
    state.command[0]=v[0]; state.command[1]=v[1]; state.command[2]=v[2];
    state.status=1; state.pending=true;
    return 0;
}
void ProductSafetyTest_Process(void)
{
    if (state.lv_pending) {
        for (uint16_t i=0;i<3U;i++) state.lv_applied[i]=state.lv_command[i];
        state.lv_remaining=state.lv_command[2];
        state.lv_processed=0U; state.lv_peak_assert=0U;
        state.lv_peak_release=0U; state.lv_flags=0U;
        state.lv_status=2U; state.lv_pending=false;
    }
    if (!state.pending) return;
    if (ProductInternalAdcDriver_SetMcuTemperatureTestOverride(
        state.command[0] != 0U, state.command[1] != 0U, (int16_t)state.command[2])) {
        state.applied[0]=state.command[0]; state.applied[1]=state.command[1]; state.applied[2]=state.command[2];
        state.status=2;
    } else state.status=3;
    state.pending=false;
}

bool ProductSafetyTest_LowVoltageInput(bool *active)
{
    if (!state.lv_applied[0] || !active) return false;
    *active = state.lv_applied[1] != 0U &&
        (state.lv_applied[2] == 0U || state.lv_remaining > 0U);
    if (*active) {
        if (state.lv_applied[2] != 0U) state.lv_remaining--;
        if (state.lv_processed < UINT16_MAX) state.lv_processed++;
    }
    return true;
}

void ProductSafetyTest_LowVoltageSample(uint16_t assert_count,
                                      uint16_t release_count,
                                      bool raw, bool confirmed)
{
    state.lv_sequence++;
    if (!state.lv_applied[0]) return;
    if (assert_count > state.lv_peak_assert) state.lv_peak_assert=assert_count;
    if (release_count > state.lv_peak_release) state.lv_peak_release=release_count;
    state.lv_flags = (state.lv_flags & 2U) | (confirmed ? 3U : 0U) |
        (raw ? 4U : 0U) |
        ((state.lv_applied[2] != 0U && state.lv_remaining == 0U) ? 8U : 0U);
}

uint8_t ProductSafetyTest_Read(uint16_t address, uint16_t count, uint16_t *values)
{
    ProductInternalAdcSnapshot_t snapshot;
    uint16_t data[12];
    if (values && count && address >= 0x4974U && address <= 0x497FU &&
        count <= 0x4980U-address) {
        data[0]=state.lv_status; data[1]=state.lv_applied[0];
        data[2]=state.lv_applied[1]; data[3]=state.lv_applied[2];
        data[4]=state.lv_processed; data[5]=state.lv_peak_assert;
        data[6]=state.lv_peak_release; data[7]=state.lv_flags;
        data[8]=(uint16_t)(state.lv_sequence >> 16); data[9]=(uint16_t)state.lv_sequence;
        data[10]=PRODUCT_LOW_VOLTAGE_ASSERT_SAMPLES;
        data[11]=PRODUCT_LOW_VOLTAGE_RELEASE_SAMPLES;
        for (uint16_t i=0;i<count;i++) values[i]=data[address-0x4974U+i];
        return 0;
    }
    if (!values || !count || address < 0x4968U || address > 0x496FU ||
        count > 0x4970U-address) return 2;
    if (!ProductInternalAdcDriver_GetSnapshot(&snapshot)) return 4;
    data[0]=state.status; data[1]=state.applied[0]; data[2]=state.applied[1]; data[3]=state.applied[2];
    data[4]=snapshot.mcu_temperature.overtemperature_count;
    data[5]=snapshot.mcu_temperature.overtemperature;
    data[6]=(uint16_t)(snapshot.mcu_temperature.sequence >> 16);
    data[7]=(uint16_t)snapshot.mcu_temperature.sequence;
    for (uint16_t i=0;i<count;i++) values[i]=data[address-0x4968U+i];
    return 0;
}
#endif
