#include "product_safety_test.h"
#ifdef PRODUCT_SAFETY_COMMAND_ENABLED
#include "product_internal_adc_driver.h"
#include <stddef.h>
static bool addressed, pending;
static uint16_t command[3], applied[3], status;
void ProductSafetyTest_SetAddressed(bool value) { addressed = value; }
uint8_t ProductSafetyTest_Write(uint16_t address, const uint16_t *v, uint16_t count)
{
    if (!addressed) return 3;
    if (address != 0x4960U || count != 4U) return 2;
    if (v == NULL || v[0] > 1U || v[1] > 1U || v[3] != 0x5AFEU) return 3;
    if (pending) return 6;
    command[0]=v[0]; command[1]=v[1]; command[2]=v[2];
    status=1; pending=true;
    return 0;
}
void ProductSafetyTest_Process(void)
{
    if (!pending) return;
    if (ProductInternalAdcDriver_SetMcuTemperatureTestOverride(
        command[0] != 0U, command[1] != 0U, (int16_t)command[2])) {
        applied[0]=command[0]; applied[1]=command[1]; applied[2]=command[2];
        status=2;
    } else status=3;
    pending=false;
}
uint8_t ProductSafetyTest_Read(uint16_t address, uint16_t count, uint16_t *values)
{
    ProductInternalAdcSnapshot_t snapshot;
    uint16_t data[8];
    if (!values || !count || address < 0x4968U || address > 0x496FU ||
        count > 0x4970U-address) return 2;
    if (!ProductInternalAdcDriver_GetSnapshot(&snapshot)) return 4;
    data[0]=status; data[1]=applied[0]; data[2]=applied[1]; data[3]=applied[2];
    data[4]=snapshot.mcu_temperature.overtemperature_count;
    data[5]=snapshot.mcu_temperature.overtemperature;
    data[6]=(uint16_t)(snapshot.mcu_temperature.sequence >> 16);
    data[7]=(uint16_t)snapshot.mcu_temperature.sequence;
    for (uint16_t i=0;i<count;i++) values[i]=data[address-0x4968U+i];
    return 0;
}
#endif
