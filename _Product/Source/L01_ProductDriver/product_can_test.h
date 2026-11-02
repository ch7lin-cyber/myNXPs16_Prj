#ifndef PRODUCT_CAN_TEST_H
#define PRODUCT_CAN_TEST_H
#include "product_can_driver.h"
#if PRODUCT_CAN_TEST_BUILD
/* Dedicated bench firmware owns the CAN transport queues. Debugger writes
 * requested_frames first, then command last, only while stopped.
 * Commands: 1=loopback, 2=requester, 3=responder, 4=request bus-off recovery,
 * 5=stop. State: 0=idle, 1=running, 2=pass, 3=fail. Reset before each run. */
typedef struct {
    uint32_t command;
    uint32_t requested_frames; /* 1..100000, default 1 when zero. */
    uint32_t state;
    uint32_t role;
    uint32_t matched;
    uint32_t submitted;
    uint32_t unexpected;
    uint32_t timeouts;
    uint32_t recovery_requests;
    ProductCanDiagnostics_t driver;
} ProductCanTestWatch_t;
extern volatile ProductCanTestWatch_t g_can_test_watch;
/* Call after driver Process(), using monotonic milliseconds, from foreground. */
void ProductCanTest_Process(uint32_t now_ms);
#endif
#endif
