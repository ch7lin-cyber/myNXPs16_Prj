#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "FaultService.h"
#include "bsp_status_led.h"
#include "l03_product_modbus.h"
#include "l03_product_modbus_master.h"
#include "product_status_led.h"

static bool g_leds[BSP_STATUS_LED_COUNT];
static uint16_t g_fault_count;
static l03_product_modbus_statistics_t g_slave_statistics;
static ProductModbusMasterState_t g_master_state;

bool BspStatusLed_Initialize(void)
{
    (void)memset(g_leds, 0, sizeof(g_leds));
    return true;
}

bool BspStatusLed_Write(BspStatusLed_t led, bool on)
{
    assert((uint32_t)led < (uint32_t)BSP_STATUS_LED_COUNT);
    g_leds[led] = on;
    return true;
}

uint16_t FaultService_GetActiveCount(void)
{
    return g_fault_count;
}

bool L03_ProductModbus_GetStatistics(
    l03_product_modbus_statistics_t *statistics)
{
    *statistics = g_slave_statistics;
    return true;
}

bool L03_ProductModbusMaster_GetResult(ProductModbusMasterResult_t *result)
{
    (void)memset(result, 0, sizeof(*result));
    result->state = g_master_state;
    return true;
}

static void Tick(uint16_t milliseconds)
{
    uint16_t elapsed;
    for (elapsed = 0U; elapsed < milliseconds; elapsed++)
    {
        ProductStatusLed_Tick1ms();
    }
}

int main(void)
{
    assert(ProductStatusLed_Initialize());
    assert(g_leds[BSP_STATUS_LED_RUN]);
    assert(!g_leds[BSP_STATUS_LED_COMMUNICATION]);
    assert(!g_leds[BSP_STATUS_LED_ERROR]);

    Tick(499U);
    assert(g_leds[BSP_STATUS_LED_RUN]);
    Tick(1U);
    assert(!g_leds[BSP_STATUS_LED_RUN]);
    Tick(500U);
    assert(g_leds[BSP_STATUS_LED_RUN]);

    g_slave_statistics.receivedFrames++;
    ProductStatusLed_Process();
    Tick(1U);
    assert(g_leds[BSP_STATUS_LED_COMMUNICATION]);
    Tick(99U);
    assert(!g_leds[BSP_STATUS_LED_COMMUNICATION]);

    g_master_state = PRODUCT_MODBUS_MASTER_BUSY;
    ProductStatusLed_Process();
    Tick(1U);
    assert(g_leds[BSP_STATUS_LED_COMMUNICATION]);

    g_fault_count = 1U;
    ProductStatusLed_Process();
    Tick(1U);
    assert(g_leds[BSP_STATUS_LED_ERROR]);
    Tick(249U);
    assert(!g_leds[BSP_STATUS_LED_ERROR]);

    g_fault_count = 0U;
    ProductStatusLed_Process();
    Tick(1U);
    assert(!g_leds[BSP_STATUS_LED_ERROR]);
    return 0;
}
