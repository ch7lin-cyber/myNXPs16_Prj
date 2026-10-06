#include "product_low_voltage_safety.h"

#include <stddef.h>

#include "FaultService.h"
#include "ProductLowVoltageConfig.h"
#include "product_low_voltage_driver.h"
#include "product_modbus_register_adapter.h"

static ProductLowVoltageSnapshot_t g_snapshot;
static uint16_t g_assert_samples;
static uint16_t g_release_samples;

static void PublishMonitor(void)
{
    product_low_voltage_monitor_t monitor;

    monitor.rawActive = g_snapshot.raw_active;
    monitor.confirmedActive = g_snapshot.confirmed_active;
    monitor.faultActive = g_snapshot.fault_active;
    monitor.revision = g_snapshot.revision;
    monitor.interruptCount = g_snapshot.interrupt_count;
    monitor.status = PRODUCT_MODBUS_LOW_VOLTAGE_STATUS_READY;
    ProductModbusRegisterAdapter_SetLowVoltageMonitor(&monitor);
}

void ProductLowVoltageSafety_Initialize(void)
{
    ProductLowVoltageDriver_Initialize();
    g_snapshot.interrupt_count = 0U;
    g_snapshot.revision = 0U;
    g_snapshot.raw_active = false;
    g_snapshot.confirmed_active = false;
    g_snapshot.fault_active = false;
    g_assert_samples = 0U;
    g_release_samples = 0U;
    PublishMonitor();
}

void ProductLowVoltageSafety_Process(void)
{
    bool previous_confirmed = g_snapshot.confirmed_active;

    g_snapshot.raw_active = ProductLowVoltageDriver_IsActive();
    g_snapshot.interrupt_count =
        ProductLowVoltageDriver_GetInterruptCount();

    if (g_snapshot.raw_active)
    {
        g_release_samples = 0U;
        if (g_assert_samples < PRODUCT_LOW_VOLTAGE_ASSERT_SAMPLES)
        {
            g_assert_samples++;
        }
        if (g_assert_samples >= PRODUCT_LOW_VOLTAGE_ASSERT_SAMPLES)
        {
            g_snapshot.confirmed_active = true;
        }
    }
    else
    {
        g_assert_samples = 0U;
        if (g_snapshot.confirmed_active)
        {
            if (g_release_samples < PRODUCT_LOW_VOLTAGE_RELEASE_SAMPLES)
            {
                g_release_samples++;
            }
            if (g_release_samples >= PRODUCT_LOW_VOLTAGE_RELEASE_SAMPLES)
            {
                g_snapshot.confirmed_active = false;
            }
        }
        else
        {
            g_release_samples = 0U;
        }
    }

    if (g_snapshot.confirmed_active != previous_confirmed)
    {
        g_snapshot.revision++;
    }
    if (g_snapshot.confirmed_active &&
        !FaultService_IsActive(FAULT_CODE_LOW_VOLTAGE))
    {
        (void)FaultService_Raise(FAULT_CODE_LOW_VOLTAGE, 1U,
                                 g_snapshot.revision, 0U);
    }
    g_snapshot.fault_active =
        FaultService_IsActive(FAULT_CODE_LOW_VOLTAGE);
    PublishMonitor();
}

bool ProductLowVoltageSafety_GetSnapshot(ProductLowVoltageSnapshot_t *snapshot)
{
    if (snapshot == NULL)
    {
        return false;
    }
    *snapshot = g_snapshot;
    return true;
}
