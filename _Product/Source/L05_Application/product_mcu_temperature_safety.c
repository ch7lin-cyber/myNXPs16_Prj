#include "product_mcu_temperature_safety.h"

#include <stdint.h>

#include "FaultService.h"

static uint16_t TemperatureToFaultDetail(int32_t temperature_centi_c)
{
    if (temperature_centi_c <= 0)
    {
        return 0U;
    }
    if (temperature_centi_c >= 65535)
    {
        return UINT16_MAX;
    }
    return (uint16_t)temperature_centi_c;
}

void ProductMcuTemperatureSafety_Initialize(void)
{
    /* FaultService and SafetyService are initialized by SystemRoutine. */
}

void ProductMcuTemperatureSafety_Process(
    bool sample_valid,
    bool overtemperature,
    int32_t temperature_centi_c)
{
    if (!sample_valid)
    {
        return;
    }

    if (overtemperature &&
        !FaultService_IsActive(FAULT_CODE_MCU_OVERTEMPERATURE))
    {
        (void)FaultService_Raise(
            FAULT_CODE_MCU_OVERTEMPERATURE,
            TemperatureToFaultDetail(temperature_centi_c),
            0U,
            0U);
    }
}
