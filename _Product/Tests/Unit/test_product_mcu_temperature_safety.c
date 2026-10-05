#include <assert.h>
#include <stdbool.h>
#include <stddef.h>

#include "EventService.h"
#include "FaultService.h"
#include "ProductSafetyConfig.h"
#include "SafetyConfigurationEventConsumer.h"
#include "product_mcu_temperature_safety.h"

static bool ResolveRange(
    const EventTemperatureInputConfiguration_t *configuration,
    float *minimum,
    float *maximum,
    bool *input_enabled,
    void *context)
{
    (void)configuration;
    (void)context;
    *minimum = -270.0F;
    *maximum = 1372.0F;
    *input_enabled = true;
    return true;
}

int main(void)
{
    FaultRecord_t record;

    FaultService_Initialize();
    assert(SafetyConfigurationEventConsumer_Initialize(ResolveRange, NULL));
    ProductMcuTemperatureSafety_Initialize();

    assert(SafetyConfigurationEventConsumer_UpdateGlobalOutputInhibit(
        PRODUCT_SAFETY_INHIBIT_LOW_VOLTAGE, true));
    ProductMcuTemperatureSafety_Process(true, false, 8400);
    assert(SafetyConfigurationEventConsumer_IsGlobalOutputInhibited());
    assert((SafetyConfigurationEventConsumer_GetGlobalOutputInhibitMask() &
            PRODUCT_SAFETY_INHIBIT_LOW_VOLTAGE) != 0U);
    assert(SafetyConfigurationEventConsumer_UpdateGlobalOutputInhibit(
        PRODUCT_SAFETY_INHIBIT_LOW_VOLTAGE, false));

    ProductMcuTemperatureSafety_Process(true, false, 8400);
    assert(!FaultService_IsActive(FAULT_CODE_MCU_OVERTEMPERATURE));
    assert(!SafetyConfigurationEventConsumer_IsGlobalOutputInhibited());

    ProductMcuTemperatureSafety_Process(true, true, 8512);
    assert(FaultService_IsActive(FAULT_CODE_MCU_OVERTEMPERATURE));
    assert(FaultService_Get(FAULT_CODE_MCU_OVERTEMPERATURE, &record));
    assert(record.last_detail == 8512U);
    assert(SafetyConfigurationEventConsumer_IsGlobalOutputInhibited());

    ProductMcuTemperatureSafety_Process(true, false, 8200);
    assert(SafetyConfigurationEventConsumer_IsGlobalOutputInhibited());

    assert(FaultService_Clear(FAULT_CODE_MCU_OVERTEMPERATURE));
    ProductMcuTemperatureSafety_Process(true, false, 8200);
    assert(!SafetyConfigurationEventConsumer_IsGlobalOutputInhibited());

    ProductMcuTemperatureSafety_Process(true, true, 8600);
    assert(FaultService_IsActive(FAULT_CODE_MCU_OVERTEMPERATURE));
    assert(SafetyConfigurationEventConsumer_IsGlobalOutputInhibited());
    return 0;
}
