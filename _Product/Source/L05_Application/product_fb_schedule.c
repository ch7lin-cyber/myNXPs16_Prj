#include "product_fb_schedule.h"

bool ProductFbSchedule_Init(ProductFbSchedule_t *schedule,
                            const ProductFbScheduleTask_t *tasks,
                            ProductFbScheduleTaskState_t *states,
                            size_t count, uint32_t now_ms)
{
    size_t i;

    if (schedule == NULL)
    {
        return false;
    }
    schedule->tasks = NULL;
    schedule->states = NULL;
    schedule->count = 0U;
    if ((count != 0U) && ((tasks == NULL) || (states == NULL)))
    {
        return false;
    }
    for (i = 0U; i < count; i++)
    {
        if ((tasks[i].period_ms == 0U) ||
            (tasks[i].period_ms > UINT32_C(0x7FFFFFFF)) ||
            (tasks[i].enabled && (tasks[i].callback == NULL)))
        {
            return false;
        }
    }
    for (i = 0U; i < count; i++)
    {
        states[i].last_release_ms = now_ms;
        states[i].last_call_ms = now_ms;
    }
    schedule->tasks = tasks;
    schedule->states = states;
    schedule->count = count;
    return true;
}

void ProductFbSchedule_Process(ProductFbSchedule_t *schedule, uint32_t now_ms)
{
    size_t i;

    if (schedule == NULL)
    {
        return;
    }
    for (i = 0U; i < schedule->count; i++)
    {
        const ProductFbScheduleTask_t *task = &schedule->tasks[i];
        ProductFbScheduleTaskState_t *state = &schedule->states[i];
        uint32_t elapsed;
        uint32_t dt;

        if (!task->enabled)
        {
            continue;
        }
        elapsed = now_ms - state->last_release_ms;
        if (elapsed < task->period_ms)
        {
            continue;
        }
        dt = now_ms - state->last_call_ms;
        state->last_release_ms = now_ms - (elapsed % task->period_ms);
        state->last_call_ms = now_ms;
        task->callback(task->context, dt);
    }
}
