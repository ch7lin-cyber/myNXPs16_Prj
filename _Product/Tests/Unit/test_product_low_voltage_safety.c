#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "FaultService.h"
#include "ProductLowVoltageConfig.h"
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

int main(void)
{
    ProductLowVoltageSnapshot_t snapshot;
    uint16_t sample;

    FaultService_Initialize();
    ProductLowVoltageSafety_Initialize();
    assert(!g_monitor.rawActive);

    g_low_voltage_active = true;
    g_interrupt_count = 1U;
    for (sample = 1U; sample < PRODUCT_LOW_VOLTAGE_ASSERT_SAMPLES; sample++)
    {
        ProductLowVoltageSafety_Process(sample);
    }
    assert(ProductLowVoltageSafety_GetSnapshot(&snapshot));
    assert(!snapshot.confirmed_active);
    ProductLowVoltageSafety_Process(sample);
    assert(ProductLowVoltageSafety_GetSnapshot(&snapshot));
    assert(snapshot.raw_active);
    assert(snapshot.confirmed_active);
    assert(snapshot.fault_active);
    assert(snapshot.revision == 1U);
    assert(snapshot.interrupt_count == 1U);
    assert(FaultService_IsActive(FAULT_CODE_LOW_VOLTAGE));

    g_low_voltage_active = false;
    for (sample = 1U; sample < PRODUCT_LOW_VOLTAGE_RELEASE_SAMPLES; sample++)
    {
        ProductLowVoltageSafety_Process(sample);
    }
    assert(ProductLowVoltageSafety_GetSnapshot(&snapshot));
    assert(snapshot.confirmed_active);
    ProductLowVoltageSafety_Process(sample);
    assert(ProductLowVoltageSafety_GetSnapshot(&snapshot));
    assert(!snapshot.confirmed_active);
    assert(snapshot.fault_active);
    assert(snapshot.revision == 2U);

    assert(FaultService_Clear(FAULT_CODE_LOW_VOLTAGE));
    ProductLowVoltageSafety_Process((uint32_t)sample + 1U);
    assert(ProductLowVoltageSafety_GetSnapshot(&snapshot));
    assert(!snapshot.fault_active);
    assert(g_monitor.status == PRODUCT_MODBUS_LOW_VOLTAGE_STATUS_READY);
    return 0;
}
