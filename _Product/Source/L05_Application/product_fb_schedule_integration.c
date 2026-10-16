#include "product_fb_schedule_integration.h"

#include <stddef.h>

#include "product_fb_moving_average_adapter.h"
#include "product_fb_schedule.h"
#include "product_sensor_measurement_service.h"

#define PRODUCT_FB_SCHEDULE_TASK_COUNT (18U)

static ProductFbMovingAverageAdapter_t g_ch0_moving_average_adapter;
static ProductFbSchedule_t g_schedule;
static ProductFbScheduleTaskState_t g_task_states[
    PRODUCT_FB_SCHEDULE_TASK_COUNT];

static bool ReadCh0Sample(void *context,
                          ProductFbMovingAverageSample_t *sample)
{
    ProductSensorMeasurementSnapshot_t snapshot;

    (void)context;
    if ((sample == NULL) ||
        !ProductSensorMeasurementService_GetSnapshot(0U, &snapshot))
    {
        return false;
    }
    sample->sample_sequence = snapshot.sample_sequence;
    sample->engineering_value = snapshot.engineering_value;
    sample->valid = snapshot.valid;
    return true;
}

#define PRODUCT_FB_SCHEDULE_CH0_MOVING_AVERAGE_ENABLED true
#define PRODUCT_FB_SCHEDULE_CH0_MOVING_AVERAGE_CALLBACK \
    ProductFbMovingAverageAdapter_Process
#define PRODUCT_FB_SCHEDULE_CH0_MOVING_AVERAGE_CONTEXT \
    (&g_ch0_moving_average_adapter)
#include "ProductFbScheduleConfig.h"

#define MAKE_TASK(name, enabled_value, period_value, callback_value, context_value) \
    {enabled_value, period_value, callback_value, context_value},
static const ProductFbScheduleTask_t g_tasks[] =
{
    PRODUCT_FB_SCHEDULE_TASKS(MAKE_TASK)
};
#undef MAKE_TASK
#undef PRODUCT_FB_SCHEDULE_CH0_MOVING_AVERAGE_ENABLED
#undef PRODUCT_FB_SCHEDULE_CH0_MOVING_AVERAGE_CALLBACK
#undef PRODUCT_FB_SCHEDULE_CH0_MOVING_AVERAGE_CONTEXT

bool ProductFbScheduleIntegration_Init(uint32_t now_ms)
{
    if (!ProductFbMovingAverageAdapter_Init(
            &g_ch0_moving_average_adapter, ReadCh0Sample, NULL))
    {
        return false;
    }
    return ProductFbSchedule_Init(
        &g_schedule, g_tasks, g_task_states,
        sizeof(g_tasks) / sizeof(g_tasks[0]), now_ms);
}

void ProductFbScheduleIntegration_Process(uint32_t now_ms)
{
    ProductFbSchedule_Process(&g_schedule, now_ms);
}

bool ProductFbScheduleIntegration_GetCh0MovingAverage(
    FbMovingAverageOutput *output)
{
    return ProductFbMovingAverageAdapter_GetOutput(
        &g_ch0_moving_average_adapter, output);
}

bool ProductFbScheduleIntegration_GetCh0Diagnostics(
    ProductFbMovingAverageAdapterDiagnostics_t *diagnostics)
{
    return ProductFbMovingAverageAdapter_GetDiagnostics(
        &g_ch0_moving_average_adapter, diagnostics);
}
