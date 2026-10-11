#include "product_fb_schedule.h"
#include "ProductFbScheduleConfig.h"
#include <assert.h>
#include <stdio.h>

static uint32_t calls;
static uint32_t last_dt;

static void Callback(void *context, uint32_t dt)
{
    assert(context == &calls);
    calls++;
    last_dt = dt;
}

#define MAKE_TASK(name, enabled, period, callback, context) \
    {enabled, period, callback, context},
static const ProductFbScheduleTask_t defaults[] =
{
    PRODUCT_FB_SCHEDULE_TASKS(MAKE_TASK)
};
#undef MAKE_TASK

int main(void)
{
    ProductFbSchedule_t schedule = {0};
    ProductFbScheduleTaskState_t states[18];
    ProductFbScheduleTask_t task = {true, 20U, Callback, &calls};
    size_t i;

    assert(!ProductFbSchedule_Init(NULL, &task, states, 1U, 0U));
    assert(!ProductFbSchedule_Init(&schedule, NULL, states, 1U, 0U));
    assert(!ProductFbSchedule_Init(&schedule, &task, NULL, 1U, 0U));
    task.period_ms = 0U;
    assert(!ProductFbSchedule_Init(&schedule, &task, states, 1U, 0U));
    task.period_ms = UINT32_MAX;
    assert(!ProductFbSchedule_Init(&schedule, &task, states, 1U, 0U));
    task.period_ms = 20U;
    task.callback = NULL;
    assert(!ProductFbSchedule_Init(&schedule, &task, states, 1U, 0U));
    task.callback = Callback;
    assert(ProductFbSchedule_Init(&schedule, &task, states, 1U, 0U));
    ProductFbSchedule_Process(&schedule, 19U);
    assert(calls == 0U);
    ProductFbSchedule_Process(&schedule, 20U);
    assert(calls == 1U && last_dt == 20U);
    ProductFbSchedule_Process(&schedule, 20U);
    assert(calls == 1U);
    ProductFbSchedule_Process(&schedule, 85U);
    assert(calls == 2U && last_dt == 65U);
    ProductFbSchedule_Process(&schedule, 100U);
    assert(calls == 3U && last_dt == 15U);
    assert(ProductFbSchedule_Init(&schedule, &task, states, 1U, UINT32_MAX - 9U));
    ProductFbSchedule_Process(&schedule, 10U);
    assert(calls == 4U && last_dt == 20U);
    assert(ProductFbSchedule_Init(&schedule, defaults, states, 18U, 0U));
    ProductFbSchedule_Process(&schedule, 1000U);
    assert(calls == 4U);
    for (i = 0U; i < 18U; i++)
    {
        assert(!defaults[i].enabled && defaults[i].callback == NULL);
    }
    assert(ProductFbSchedule_Init(&schedule, NULL, NULL, 0U, 0U));
    ProductFbSchedule_Process(&schedule, 1000U);
    ProductFbSchedule_Process(NULL, 0U);
    puts("PASS: validation, release timing, coalescing, phase, actual dt, rollover, disabled defaults, empty schedule");
    return 0;
}
