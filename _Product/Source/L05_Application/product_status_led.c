#include "product_status_led.h"

#include <stdint.h>

#include "FaultService.h"
#include "ProductLedConfig.h"
#include "bsp_status_led.h"
#include "l03_product_modbus.h"
#include "l03_product_modbus_master.h"

static uint16_t g_run_elapsed_ms;
static uint16_t g_error_elapsed_ms;
static uint16_t g_comm_remaining_ms;
static uint32_t g_last_slave_activity;
static ProductModbusMasterState_t g_last_master_state;
static bool g_run_on;
static bool g_error_on;
static bool g_fault_active;

static void WriteAllOutputs(void)
{
    (void)BspStatusLed_Write(BSP_STATUS_LED_RUN, g_run_on);
    (void)BspStatusLed_Write(BSP_STATUS_LED_COMMUNICATION,
                             g_comm_remaining_ms != 0U);
    (void)BspStatusLed_Write(BSP_STATUS_LED_ERROR,
                             g_fault_active && g_error_on);
}

bool ProductStatusLed_Initialize(void)
{
    g_run_elapsed_ms = 0U;
    g_error_elapsed_ms = 0U;
    g_comm_remaining_ms = 0U;
    g_last_slave_activity = 0U;
    g_last_master_state = PRODUCT_MODBUS_MASTER_IDLE;
    g_run_on = true;
    g_error_on = false;
    g_fault_active = false;

    if (!BspStatusLed_Initialize())
    {
        return false;
    }
    WriteAllOutputs();
    return true;
}

void ProductStatusLed_Tick1ms(void)
{
    if (++g_run_elapsed_ms >= PRODUCT_LED_RUN_TOGGLE_PERIOD_MS)
    {
        g_run_elapsed_ms = 0U;
        g_run_on = !g_run_on;
    }

    if (g_comm_remaining_ms != 0U)
    {
        g_comm_remaining_ms--;
    }

    if (!g_fault_active)
    {
        g_error_elapsed_ms = 0U;
        g_error_on = false;
    }
    else if (++g_error_elapsed_ms >= PRODUCT_LED_ERROR_TOGGLE_PERIOD_MS)
    {
        g_error_elapsed_ms = 0U;
        g_error_on = !g_error_on;
    }
    WriteAllOutputs();
}

void ProductStatusLed_Process(void)
{
    l03_product_modbus_statistics_t statistics;
    ProductModbusMasterResult_t master_result;
    bool fault_active = (FaultService_GetActiveCount() != 0U);

    if (fault_active && !g_fault_active)
    {
        /* Make a newly raised fault visible on the next 1 ms update. */
        g_error_elapsed_ms = 0U;
        g_error_on = true;
    }
    g_fault_active = fault_active;

    if (L03_ProductModbus_GetStatistics(&statistics))
    {
        uint32_t activity = statistics.receivedFrames +
                            statistics.transmittedResponses;
        if (activity != g_last_slave_activity)
        {
            g_last_slave_activity = activity;
            g_comm_remaining_ms = PRODUCT_LED_COMM_PULSE_PERIOD_MS;
        }
    }

    if (L03_ProductModbusMaster_GetResult(&master_result) &&
        (master_result.state != g_last_master_state))
    {
        if ((master_result.state == PRODUCT_MODBUS_MASTER_BUSY) ||
            (master_result.state == PRODUCT_MODBUS_MASTER_COMPLETE) ||
            (master_result.state == PRODUCT_MODBUS_MASTER_ERROR))
        {
            g_comm_remaining_ms = PRODUCT_LED_COMM_PULSE_PERIOD_MS;
        }
        g_last_master_state = master_result.state;
    }
}
