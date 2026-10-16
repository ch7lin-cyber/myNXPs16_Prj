#ifndef PRODUCT_WATCHDOG_DRIVER_H
#define PRODUCT_WATCHDOG_DRIVER_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* BOARD_InitBootPeripherals starts WWDT in reset mode for the boot phase.
 * After successful startup this changes the timeout to 2 seconds. The timebase
 * unit is 100 us. These functions must only be called from foreground code. */
bool ProductWatchdogDriver_StartRuntime(uint32_t tick100us);
void ProductWatchdogDriver_Process(uint32_t tick100us);

/* Debugger diagnostic: runtime feeds since the last successful start. */
extern volatile uint32_t g_productWatchdogFeedCount;

#ifdef __cplusplus
}
#endif

#endif /* PRODUCT_WATCHDOG_DRIVER_H */
