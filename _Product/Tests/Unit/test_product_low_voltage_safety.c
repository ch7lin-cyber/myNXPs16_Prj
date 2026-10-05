#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "EventService.h"
#include "FaultService.h"
#include "ProductLowVoltageConfig.h"
#include "ProductSafetyConfig.h"
#include "SafetyConfigurationEventConsumer.h"
#include "product_low_voltage_safety.h"
#include "product_modbus_register_adapter.h"

static bool g_low_voltage_active;
static uint32_t g_interrupt_count;
static product_low_voltage_monitor_t g_monitor;

void ProductLowVoltageDriver_Initialize(void)
{
    g_interrupt_count = 0U;
}

bool ProductLowVoltageDriver_IsActive(void)
{
    return g_low_voltage_active;
}

uint32_t ProductLowVoltageDriver_GetInterruptCount(void)
{
    return g_interrupt_count;
}

void ProductModbusRegisterAdapter_SetLowVoltageMonitor(
    const product_low_voltage_monitor_t *monitor)
{
    assert(monitor != NULL);
    g_monitor = *monitor;
}

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
    ProductLowVoltageSnapshot_t snapshot;
    uint16_t sample;

    FaultService_Initialize();
    assert(SafetyConfigurationEventConsumer_Initialize(ResolveRange, NULL));
    ProductLowVoltageSafety_Initialize();
    assert(!g_monitor.rawActive);

    g_low_voltage_active = true;
    g_interrupt_count = 1U;
    for (sample = 1U; sample < PRODUCT_LOW_VOLTAGE_ASSERT_SAMPLES; sample++)
    {
        ProductLowVoltageSafety_Process();
    }
    assert(ProductLowVoltageSafety_GetSnapshot(&snapshot));
    assert(!snapshot.confirmed_active);
    ProductLowVoltageSafety_Process();
    assert(ProductLowVoltageSafety_GetSnapshot(&snapshot));
    assert(snapshot.raw_active);
    assert(snapshot.confirmed_active);
    assert(snapshot.fault_active);
    assert(snapshot.revision == 1U);
    assert(snapshot.interrupt_count == 1U);
    assert(FaultService_IsActive(FAULT_CODE_LOW_VOLTAGE));
    assert((SafetyConfigurationEventConsumer_GetGlobalOutputInhibitMask() &
            PRODUCT_SAFETY_INHIBIT_LOW_VOLTAGE) != 0U);

    g_low_voltage_active = false;
    for (sample = 1U; sample < PRODUCT_LOW_VOLTAGE_RELEASE_SAMPLES; sample++)
    {
        ProductLowVoltageSafety_Process();
    }
    assert(ProductLowVoltageSafety_GetSnapshot(&snapshot));
    assert(snapshot.confirmed_active);
    ProductLowVoltageSafety_Process();
    assert(ProductLowVoltageSafety_GetSnapshot(&snapshot));
    assert(!snapshot.confirmed_active);
    assert(snapshot.fault_active);
    assert(snapshot.revision == 2U);
    assert(SafetyConfigurationEventConsumer_IsGlobalOutputInhibited());

    assert(FaultService_Clear(FAULT_CODE_LOW_VOLTAGE));
    ProductLowVoltageSafety_Process();
    assert(ProductLowVoltageSafety_GetSnapshot(&snapshot));
    assert(!snapshot.fault_active);
    assert(!SafetyConfigurationEventConsumer_IsGlobalOutputInhibited());
    assert(g_monitor.status == PRODUCT_MODBUS_LOW_VOLTAGE_STATUS_READY);
    return 0;
}
