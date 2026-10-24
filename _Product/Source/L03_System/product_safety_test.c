#include "product_safety_test.h"
#ifdef PRODUCT_SAFETY_COMMAND_ENABLED
#include "product_internal_adc_driver.h"
#include <stddef.h>
/* Managed linker/startup includes this zero-initialized state in .bss_RAM3. */
static struct {
    bool addressed, pending;
    uint16_t command[3], applied[3], status;
} state
#if defined(__GNUC__)
    __attribute__((section(".bss.$SRAMX")))
#endif
    ;
void ProductSafetyTest_SetAddressed(bool value) { state.addressed = value; }
uint8_t ProductSafetyTest_Write(uint16_t address, const uint16_t *v, uint16_t count)
{
    if (!state.addressed) return 3;
    if (address != 0x4960U || count != 4U) return 2;
    if (v == NULL || v[0] > 1U || v[1] > 1U || v[3] != 0x5AFEU) return 3;
    if (state.pending) return 6;
    state.command[0]=v[0]; state.command[1]=v[1]; state.command[2]=v[2];
    state.status=1; state.pending=true;
    return 0;
}
void ProductSafetyTest_Process(void)
{
    if (!state.pending) return;
    if (ProductInternalAdcDriver_SetMcuTemperatureTestOverride(
        state.command[0] != 0U, state.command[1] != 0U, (int16_t)state.command[2])) {
        state.applied[0]=state.command[0]; state.applied[1]=state.command[1]; state.applied[2]=state.command[2];
        state.status=2;
    } else state.status=3;
    state.pending=false;
}
uint8_t ProductSafetyTest_Read(uint16_t address, uint16_t count, uint16_t *values)
{
    ProductInternalAdcSnapshot_t snapshot;
    uint16_t data[8];
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
