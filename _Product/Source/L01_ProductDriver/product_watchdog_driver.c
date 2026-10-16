#include "product_watchdog_driver.h"

#include "fsl_clock.h"
#include "fsl_wwdt.h"

#define PRODUCT_WATCHDOG_FEED_INTERVAL_100US (1000U) /* 100 ms */
#define PRODUCT_WATCHDOG_RUNTIME_SECONDS      (2U)

static bool s_running;
static uint32_t s_last_feed_tick;
volatile uint32_t g_productWatchdogFeedCount;

bool ProductWatchdogDriver_StartRuntime(uint32_t tick100us)
{
    uint32_t timeout_count;

    s_running = false;
    g_productWatchdogFeedCount = 0U;

    /* LPC55S16 WWDT counts at one quarter of the WDT input clock. Do not
     * reinitialize an already enabled watchdog or change its clock here. */
    timeout_count =
        (CLOCK_GetWdtClkFreq() / 4U) * PRODUCT_WATCHDOG_RUNTIME_SECONDS;
    if ((timeout_count < 0xFFU) ||
        (timeout_count > WWDT_TC_COUNT_MASK) ||
        ((WWDT->MOD & (WWDT_MOD_WDEN_MASK | WWDT_MOD_WDRESET_MASK)) !=
         (WWDT_MOD_WDEN_MASK | WWDT_MOD_WDRESET_MASK)) ||
        ((WWDT->MOD & WWDT_MOD_WDPROTECT_MASK) != 0U) ||
        (WWDT->WINDOW != WWDT_WINDOW_WINDOW_MASK))
    {
        return false;
    }

    WWDT_SetTimeoutValue(WWDT, timeout_count);
    WWDT_Refresh(WWDT);
    s_last_feed_tick = tick100us;
    s_running = true;
    return true;
}

void ProductWatchdogDriver_Process(uint32_t tick100us)
{
    if (s_running &&
        ((uint32_t)(tick100us - s_last_feed_tick) >=
         PRODUCT_WATCHDOG_FEED_INTERVAL_100US))
    {
        WWDT_Refresh(WWDT);
        s_last_feed_tick = tick100us;
        g_productWatchdogFeedCount++;
    }
}
