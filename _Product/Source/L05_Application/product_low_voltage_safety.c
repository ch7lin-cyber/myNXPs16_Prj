#include "product_low_voltage_safety.h"

#include <stddef.h>

#include "FaultService.h"
#include "ProductLowVoltageConfig.h"
#include "ProductSafetyTestConfig.h"
#include "SystemFaultService.h"
#include "product_low_voltage_driver.h"
#include "product_modbus_register_adapter.h"
#include "product_safety_test.h"

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
#if !PRODUCT_SAFETY_TEST_EVK
    ProductLowVoltageDriver_Initialize();
#endif
    g_snapshot.interrupt_count = 0U;
    g_snapshot.revision = 0U;
    g_snapshot.raw_active = false;
    g_snapshot.confirmed_active = false;
    g_snapshot.fault_active = false;
    g_assert_samples = 0U;
    g_release_samples = 0U;
    PublishMonitor();
}

void ProductLowVoltageSafety_Process(uint32_t timestamp_ms)
{
    bool previous_confirmed = g_snapshot.confirmed_active;

#if PRODUCT_SAFETY_TEST_EVK
    /* EVK has no product voltage detector. Keep the real safety logic. */
    g_snapshot.raw_active = false;
    g_snapshot.interrupt_count = 0U;
#else
    g_snapshot.raw_active = ProductLowVoltageDriver_IsActive();
    g_snapshot.interrupt_count =
        ProductLowVoltageDriver_GetInterruptCount();
#endif

#ifdef PRODUCT_SAFETY_COMMAND_ENABLED
    (void)ProductSafetyTest_LowVoltageInput(&g_snapshot.raw_active);
#endif

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
        int32_t values[SNAPSHOT_SERVICE_VALUE_COUNT] = {0};

        values[0] = g_snapshot.raw_active ? 1L : 0L;
        values[1] = (int32_t)g_snapshot.interrupt_count;
        values[2] = (int32_t)g_snapshot.revision;
        (void)SystemFaultService_Raise(
            FAULT_CODE_LOW_VOLTAGE, 1U,
            g_snapshot.revision, 0U, timestamp_ms, values);
    }
    g_snapshot.fault_active =
        FaultService_IsActive(FAULT_CODE_LOW_VOLTAGE);
#ifdef PRODUCT_SAFETY_COMMAND_ENABLED
    ProductSafetyTest_LowVoltageSample(g_assert_samples, g_release_samples,
                                      g_snapshot.raw_active,
                                      g_snapshot.confirmed_active);
#endif
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
