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

#if PRODUCT_FB_CH0_TRACE_ENABLED
volatile ProductFbCh0Trace_t g_product_fb_ch0_trace;
static uint32_t g_trace_now_ms, g_trace_lateness_ms;
static ProductFbMovingAverageSample_t g_trace_sample;
static bool g_trace_provider_ok;

/* DWT is enabled by the debugger, never by product firmware. Host builds
 * deliberately mark timing unavailable. Counter subtraction handles one wrap. */
static uint32_t TraceCycles(void)
{
#if defined(__arm__) || defined(__thumb__)
    return *(volatile const uint32_t *)0xE0001004UL;
#else
    return 0U;
#endif
}
static uint32_t TraceCyclesValid(void)
{
#if defined(__arm__) || defined(__thumb__)
    return ((*(volatile const uint32_t *)0xE000EDFCUL & (1UL << 24)) &&
            (*(volatile const uint32_t *)0xE0001000UL & 1UL)) ? 1U : 0U;
#else
    return 0U;
#endif
}
#endif

static bool ReadCh0Sample(void *context,
                          ProductFbMovingAverageSample_t *sample)
{
    ProductSensorMeasurementSnapshot_t snapshot;

    (void)context;
#if PRODUCT_FB_CH0_TRACE_ENABLED
    g_trace_provider_ok = false;
    g_trace_sample.sample_sequence = 0U;
    g_trace_sample.engineering_value = 0;
    g_trace_sample.valid = false;
#endif
    if ((sample == NULL) ||
        !ProductSensorMeasurementService_GetSnapshot(0U, &snapshot))
    {
        return false;
    }
    sample->sample_sequence = snapshot.sample_sequence;
    sample->engineering_value = snapshot.engineering_value;
    sample->valid = snapshot.valid;
#if PRODUCT_FB_CH0_TRACE_ENABLED
    g_trace_provider_ok = true;
    g_trace_sample = *sample;
#endif
    return true;
}

#if PRODUCT_FB_CH0_TRACE_ENABLED
static void TraceCh0Process(void *context, uint32_t elapsed_ms)
{
    ProductFbMovingAverageAdapter_t *a = context;
    bool had_sequence = a->has_last_sample_sequence;
    uint32_t previous_sequence = a->last_sample_sequence;
    uint32_t timing_valid = TraceCyclesValid();
    uint32_t start = TraceCycles();
    uint32_t cycles;
    ProductFbCh0TraceRecord_t record;
    ProductFbMovingAverageAdapter_Process(context, elapsed_ms);
    cycles = TraceCycles() - start;
    if (!g_product_fb_ch0_trace.capture ||
        g_product_fb_ch0_trace.count >= PRODUCT_FB_CH0_TRACE_CAPACITY)
    {
        return;
    }
    record.now_ms = g_trace_now_ms;
    record.elapsed_ms = elapsed_ms;
    record.lateness_ms = g_trace_lateness_ms;
    record.sequence = g_trace_sample.sample_sequence;
    record.pv = g_trace_sample.engineering_value;
    record.provider_ok = g_trace_provider_ok;
    record.input_valid = g_trace_sample.valid;
    record.accepted = g_trace_provider_ok && g_trace_sample.valid &&
        (!had_sequence || previous_sequence != g_trace_sample.sample_sequence);
    record.sample_count = (uint32_t)a->output.sample_count;
    record.output_valid = a->output.valid != 0;
    record.average = a->output.average;
    record.status = (uint32_t)a->last_status;
    record.cycles = cycles;
    record.cycles_valid = timing_valid && TraceCyclesValid();
    g_product_fb_ch0_trace.records[g_product_fb_ch0_trace.count] = record;
    g_product_fb_ch0_trace.count++;
    if (g_product_fb_ch0_trace.count == PRODUCT_FB_CH0_TRACE_CAPACITY)
    {
        g_product_fb_ch0_trace.capture = 0U;
    }
}
#endif

#define PRODUCT_FB_SCHEDULE_CH0_MOVING_AVERAGE_ENABLED true
#if PRODUCT_FB_CH0_TRACE_ENABLED
#define PRODUCT_FB_SCHEDULE_CH0_MOVING_AVERAGE_CALLBACK TraceCh0Process
#else
#define PRODUCT_FB_SCHEDULE_CH0_MOVING_AVERAGE_CALLBACK \
    ProductFbMovingAverageAdapter_Process
#endif
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
#if PRODUCT_FB_CH0_TRACE_ENABLED
    g_product_fb_ch0_trace.capture = 0U;
    g_product_fb_ch0_trace.schema_version = 1U;
    g_product_fb_ch0_trace.init_ms = now_ms;
    g_product_fb_ch0_trace.count = 0U;
    g_product_fb_ch0_trace.capture = 1U;
#endif
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
#if PRODUCT_FB_CH0_TRACE_ENABLED
    size_t i;
    g_trace_now_ms = now_ms;
    for (i = 0U; i < g_schedule.count; ++i)
    {
        if (g_tasks[i].callback == TraceCh0Process)
        {
            g_trace_lateness_ms = now_ms - g_task_states[i].last_release_ms;
            if (g_trace_lateness_ms >= g_tasks[i].period_ms)
            {
                g_trace_lateness_ms -= g_tasks[i].period_ms;
            }
            break;
        }
    }
#endif
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
