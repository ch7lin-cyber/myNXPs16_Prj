#include "product_mcu_temperature_safety.h"

#include <stdint.h>

#include "FaultService.h"
#include "SafetyConfigurationEventConsumer.h"

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
    SafetyConfigurationEventConsumer_SetGlobalOutputInhibit(false);
}

void ProductMcuTemperatureSafety_Process(
    bool sample_valid,
    bool overtemperature,
    int32_t temperature_centi_c)
{
    bool fault_active;

    if (!sample_valid)
    {
        return;
    }

    fault_active = FaultService_IsActive(FAULT_CODE_MCU_OVERTEMPERATURE);
    if (overtemperature && !fault_active)
    {
        (void)FaultService_Raise(
            FAULT_CODE_MCU_OVERTEMPERATURE,
            TemperatureToFaultDetail(temperature_centi_c),
            0U,
            0U);
        fault_active = FaultService_IsActive(
            FAULT_CODE_MCU_OVERTEMPERATURE);
    }

    /* Cooling alone cannot restart an output: the fault is operator-latched. */
    SafetyConfigurationEventConsumer_SetGlobalOutputInhibit(
        overtemperature || fault_active);
}
