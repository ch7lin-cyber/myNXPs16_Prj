#include <assert.h>
#include <stdbool.h>
#include <stddef.h>

#include "FaultService.h"
#include "product_mcu_temperature_safety.h"

int main(void)
{
    FaultRecord_t record;

    FaultService_Initialize();
    ProductMcuTemperatureSafety_Initialize();

    ProductMcuTemperatureSafety_Process(true, false, 8400);
    assert(!FaultService_IsActive(FAULT_CODE_MCU_OVERTEMPERATURE));

    ProductMcuTemperatureSafety_Process(true, true, 8512);
    assert(FaultService_IsActive(FAULT_CODE_MCU_OVERTEMPERATURE));
    assert(FaultService_Get(FAULT_CODE_MCU_OVERTEMPERATURE, &record));
    assert(record.last_detail == 8512U);

    ProductMcuTemperatureSafety_Process(true, false, 8200);
    assert(FaultService_IsActive(FAULT_CODE_MCU_OVERTEMPERATURE));

    assert(FaultService_Clear(FAULT_CODE_MCU_OVERTEMPERATURE));
    ProductMcuTemperatureSafety_Process(true, false, 8200);
    assert(!FaultService_IsActive(FAULT_CODE_MCU_OVERTEMPERATURE));

    ProductMcuTemperatureSafety_Process(true, true, 8600);
    assert(FaultService_IsActive(FAULT_CODE_MCU_OVERTEMPERATURE));
    return 0;
}
