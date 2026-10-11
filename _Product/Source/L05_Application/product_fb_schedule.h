#ifndef PRODUCT_FB_SCHEDULE_H
#define PRODUCT_FB_SCHEDULE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* L05 callback adapter will supply inputs and call an implemented L04 FB.
 * elapsed_ms is actual time since the previous invocation (or initialization).
 * No hardware access or L04 invocation is implemented by this scheduler. */
typedef void (*ProductFbScheduleCallback_t)(void *context, uint32_t elapsed_ms);

typedef struct
{
    bool enabled;
    uint32_t period_ms;
    ProductFbScheduleCallback_t callback;
    void *context;
} ProductFbScheduleTask_t;

typedef struct
{
    uint32_t last_release_ms;
    uint32_t last_call_ms;
} ProductFbScheduleTaskState_t;

typedef struct
{
    const ProductFbScheduleTask_t *tasks;
    ProductFbScheduleTaskState_t *states;
    size_t count;
} ProductFbSchedule_t;

/* Caller owns immutable task table and state storage for the entire lifetime.
 * Disabled placeholders may have NULL callbacks. Enabled tasks may not.
 * Reinitialize the whole schedule to change configuration or enabled state. */
bool ProductFbSchedule_Init(ProductFbSchedule_t *schedule,
                            const ProductFbScheduleTask_t *tasks,
                            ProductFbScheduleTaskState_t *states,
                            size_t count, uint32_t now_ms);

/* Main-loop only, non-reentrant; never execute callbacks inside a tick ISR.
 * Use a monotonic uint32 millisecond clock, call at least once per 2^31 ms.
 * Late releases coalesce into one call, preserving phase; no catch-up burst.
 * Table order is execution order. Callbacks must not block or mutate the table. */
void ProductFbSchedule_Process(ProductFbSchedule_t *schedule, uint32_t now_ms);

#ifdef __cplusplus
}
#endif

#endif /* PRODUCT_FB_SCHEDULE_H */
