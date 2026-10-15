#ifndef PRODUCT_WATCHDOG_DRIVER_H
#define PRODUCT_WATCHDOG_DRIVER_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* BOARD_InitBootPeripherals starts the reset watchdog with an 8 s boot budget.
 * The foreground switches to 2 s after successful startup. Tick unit: 100 us.
 * No interrupt handler may call these functions. */
bool ProductWatchdogDriver_StartRuntime(uint32_t tick100us);
void ProductWatchdogDriver_Process(uint32_t tick100us);

/* Debugger diagnostic: runtime feeds since the last successful StartRuntime. */
extern volatile uint32_t g_productWatchdogFeedCount;

#ifdef __cplusplus
}
#endif
#endif
