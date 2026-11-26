#include "product_fb_schedule_integration.h"

#include <stddef.h>

#include "product_fb_moving_average_adapter.h"
#include "product_fb_schedule.h"
#include "product_sensor_measurement_service.h"
#include "ProductSensorMeasurementConfig.h"
#include "product_temperature_input_types.h"
#include "DigitalInputService.h"

#define PRODUCT_FB_SCHEDULE_TASK_COUNT (20U)

static ProductPvProcessingAdapter g_ch0_pv_processing;
static uint32_t g_fb_now_ms;
static bool g_sv_ready, g_sv_busy;
static ProductChannelSv g_channel_sv[PRODUCT_AL_CHANNEL_COUNT];
static ProductChannelSvConsumer g_sv_consumers[PRODUCT_AL_CHANNEL_COUNT][PRODUCT_CHANNEL_SV_CONSUMER_COUNT];
static bool g_pv_configuration_review_required;
static bool g_configuration_seen[PRODUCT_AL_CHANNEL_COUNT];
static ProductChannelConfigurationNotification g_notifications[PRODUCT_AL_CHANNEL_COUNT];
#if PRODUCT_PID_CH0_TEST_ENABLED
static bool g_pid_configuration_review_required;
#endif

static bool PvSourcesCompatible(const ProductPvProcessingConfig *config)
{
    unsigned i;
    for (i=0;i<(config->switch_enabled ? 2U : 1U);++i) {
        ProductSensorMeasurementSnapshot_t s;
        if (!ProductSensorMeasurementService_GetSnapshot((uint8_t)config->source[i],&s) ||
            !s.configuration_range_valid) return false;
    }
    return true;
}
/* Conservative reconfiguration contract: X coordinates and switch envelope
 * must fit each new source's nominal temperature range. Y/offset semantics
 * still require explicit application review; execution keeps finite/range guards. */
static bool PvConfigurationFitsRanges(const ProductPvProcessingConfig *config)
{
    unsigned i,n;
    bool known=g_configuration_seen[config->source[0]] ||
        (config->switch_enabled && g_configuration_seen[config->source[1]]);
    if (!known) return true;
    for (i=0;i<(config->switch_enabled ? 2U : 1U);++i) {
        ProductSensorMeasurementSnapshot_t s;
        const FbInputLinearizationConfig *c=&config->linearization[i];
        if (!ProductSensorMeasurementService_GetSnapshot((uint8_t)config->source[i],&s) ||
            !s.configuration_range_valid) return false;
        if (c->mode!=FB_INPUT_LINEARIZATION_BYPASS)
            for (n=0;n<c->parameters.curve.count;++n)
                if (c->parameters.curve.table[n].x<s.configuration_minimum ||
                    c->parameters.curve.table[n].x>s.configuration_maximum) return false;
        if (config->switch_enabled &&
            (config->switch_config.sensor_low<s.configuration_minimum ||
             config->switch_config.sensor_high>s.configuration_maximum)) return false;
    }
    return true;
}
static bool PvBlocked(void)
{
    return g_pv_configuration_review_required ||
        (g_ch0_pv_processing.initialized &&
         /* Before first publication, source validity gates startup. */
         ((g_configuration_seen[g_ch0_pv_processing.config.source[0]] ||
           (g_ch0_pv_processing.config.switch_enabled &&
            g_configuration_seen[g_ch0_pv_processing.config.source[1]])) &&
          !PvSourcesCompatible(&g_ch0_pv_processing.config)));
}

static ProductCh0AlarmPvSource g_ch0_alarm_source;
static bool g_processed_alarm_has_sample;
static uint32_t g_processed_alarm_sequence;
static ProductPvProcessingOutput g_processed_alarm_previous;

static bool IsTemperatureSensor(uint16_t sensor_type)
{
    switch (sensor_type)
    {
        case PRODUCT_SENSOR_TYPE_TC_B: case PRODUCT_SENSOR_TYPE_TC_C:
        case PRODUCT_SENSOR_TYPE_TC_D: case PRODUCT_SENSOR_TYPE_TC_E:
        case PRODUCT_SENSOR_TYPE_TC_J: case PRODUCT_SENSOR_TYPE_TC_K:
        case PRODUCT_SENSOR_TYPE_TC_N: case PRODUCT_SENSOR_TYPE_TC_R:
        case PRODUCT_SENSOR_TYPE_TC_S: case PRODUCT_SENSOR_TYPE_TC_T:
        case PRODUCT_SENSOR_TYPE_TC_L: case PRODUCT_SENSOR_TYPE_TC_U:
        case PRODUCT_SENSOR_TYPE_TC_TXK: case PRODUCT_SENSOR_TYPE_RTD_100_OHM:
        case PRODUCT_SENSOR_TYPE_RTD_1000_OHM: case PRODUCT_SENSOR_TYPE_RTD_JPT100:
        case PRODUCT_SENSOR_TYPE_RTD_NI120: case PRODUCT_SENSOR_TYPE_RTD_CU50:
            return true;
        default: return false;
    }
}

static bool SnapshotUsable(const ProductSensorMeasurementSnapshot_t *snapshot)
{
    return snapshot->valid && snapshot->quality_flags == 0U &&
        snapshot->input_error == PRODUCT_INPUT_ERROR_NONE &&
        snapshot->conversion_flags == 0U && snapshot->processed_timestamp_valid &&
        (uint32_t)(g_fb_now_ms - snapshot->processed_timestamp_ms) <
            PRODUCT_SENSOR_STALE_TIMEOUT_MS && IsTemperatureSensor(snapshot->sensor_type);
}

static bool ReadProcessingSample(void *context, uint16_t source, ProductPvSourceSample *sample)
{
    ProductSensorMeasurementSnapshot_t snapshot;
    (void)context;
    if (source>=PRODUCT_AL_CHANNEL_COUNT || sample==NULL ||
        !ProductSensorMeasurementService_GetSnapshot((uint8_t)source,&snapshot)) return false;
    sample->value=snapshot.engineering_value;
    sample->sequence=snapshot.sample_sequence;
    sample->timestamp_ms=snapshot.processed_timestamp_ms;
    sample->unit=IsTemperatureSensor(snapshot.sensor_type) ?
        PRODUCT_PV_MILLICELSIUS : PRODUCT_PV_UNSUPPORTED_UNIT;
    sample->valid=SnapshotUsable(&snapshot);
    return true;
}

static void ProcessCh0Pv(void *context, uint32_t elapsed_ms)
{
    (void)elapsed_ms;
    (void)ProductPvProcessing_Process(context,g_fb_now_ms,!PvBlocked(),false);
}

static ProductAlarmLogicAdapter_t g_alarm_logic;

#if PRODUCT_PID_CH0_TEST_ENABLED
static ProductPidControllerAdapter_t g_ch0_pid_test;
static ProductPidControllerInput_t g_ch0_pid_test_input;
volatile uint32_t g_ch0_pid_test_enable, g_ch0_pid_test_reset;
volatile int32_t g_ch0_pid_test_sv, g_ch0_pid_test_mv;
volatile uint32_t g_ch0_pid_test_sv_result;
volatile ProductCh0PidTestDiagnostics g_ch0_pid_test_watch;

static bool PidSvInChannelRange(int32_t sv)
{
    ProductSensorMeasurementSnapshot_t s;
    if (!g_configuration_seen[0]) return true;
    return ProductSensorMeasurementService_GetSnapshot(0,&s) && s.configuration_range_valid &&
        sv>=s.configuration_minimum && sv<=s.configuration_maximum;
}

static bool ReadPidTestSample(void *context, ProductPidControllerInput_t *input)
{
    ProductPvSourceSample source={0};
    bool available;
    (void)context;
    available=ReadProcessingSample(NULL,0U,&source);
    input->sample_sequence=source.sequence;
    input->pv=source.value;
    input->sv=g_channel_sv[0].target;
    input->enable=g_ch0_pid_test_enable==1U;
    input->reset=g_ch0_pid_test_reset==1U;
    input->valid=g_channel_sv[0].configured && !g_pid_configuration_review_required &&
        PidSvInChannelRange(input->sv) &&
        available && source.valid && source.unit==PRODUCT_PV_MILLICELSIUS &&
        (uint32_t)(g_fb_now_ms-source.timestamp_ms)<=200U &&
        g_ch0_pid_test_mv>=0 && g_ch0_pid_test_mv<=1000 &&
        input->sv>=FB_ALARM_LIMIT_MIN && input->sv<=FB_ALARM_LIMIT_MAX;
    g_ch0_pid_test_reset=false;
    g_ch0_pid_test_input=*input;
    return true; /* unavailable source is explicit invalid input, never fake PV */
}

bool ProductFbScheduleIntegration_ConfigureCh0PidTest(bool enable, int32_t sv, int32_t mv)
{
    if (!g_ch0_pid_test.initialized || sv<FB_ALARM_LIMIT_MIN || sv>FB_ALARM_LIMIT_MAX ||
        mv<0 || mv>1000) return false;
    if (!PidSvInChannelRange(sv)) return false;
    {
        ProductChannelSvResult result=ProductFbScheduleIntegration_SetChannelSv(0U,sv);
        if (result!=PRODUCT_CHANNEL_SV_OK && result!=PRODUCT_CHANNEL_SV_UNCHANGED) return false;
    }
    if (FbPid_SetTestMv(&g_ch0_pid_test.fb_pid,mv)!=FB_PID_STATUS_OK) return false;
    if (!ProductPidControllerAdapter_Reset(&g_ch0_pid_test)) return false;
    g_ch0_pid_test_enable=enable; g_ch0_pid_test_sv=sv; g_ch0_pid_test_reset=false;
    g_ch0_pid_test_mv=mv;
    g_pid_configuration_review_required=false;
    return true;
}
bool ProductFbScheduleIntegration_ResetCh0PidTest(void)
{
    if (!ProductPidControllerAdapter_Reset(&g_ch0_pid_test)) return false;
    g_ch0_pid_test_reset=true; return true;
}
bool ProductFbScheduleIntegration_GetCh0PidTest(ProductCh0PidTestDiagnostics *out)
{
    if (!out || !g_ch0_pid_test.initialized) return false;
    out->input=g_ch0_pid_test_input; out->fixed_mv_test_backend=true;
    return ProductPidControllerAdapter_GetDiagnostics(&g_ch0_pid_test,&out->controller);
}
static ProductChannelSvResult ValidatePidTestSv(void *context, int32_t sv)
{
    const ProductPidControllerAdapter_t *a=context;
    (void)sv;
    return a->initialized ? PRODUCT_CHANNEL_SV_OK : PRODUCT_CHANNEL_SV_CONSUMER_REJECTED;
}
static bool PidTestAtRunning(void *context)
{ return ProductPidControllerAdapter_IsAutoTuneRunning(context); }
static void CommitPidTestSv(void *context, int32_t sv)
{ (void)context; g_ch0_pid_test_sv=sv; }
static void HandlePidTestSvRequest(void)
{
    if ((g_channel_sv[0].configured && g_ch0_pid_test_sv!=g_channel_sv[0].target) ||
        (!g_channel_sv[0].configured && (g_ch0_pid_test_enable==1U || g_ch0_pid_test_sv!=0))) {
        g_ch0_pid_test_sv_result=ProductFbScheduleIntegration_SetChannelSv(0U,g_ch0_pid_test_sv);
        /* Rejected commands keep the previous accepted target; no invalid input pulse. */
        g_ch0_pid_test_sv=g_channel_sv[0].configured ? g_channel_sv[0].target : 0;
    }
}
static void ProcessCh0PidTest(void *context, uint32_t elapsed_ms)
{
    ProductCh0PidTestDiagnostics snapshot;
    /* Invalid SWD MV is rejected by provider validity; never reuse it as valid. */
    (void)FbPid_SetTestMv(&g_ch0_pid_test.fb_pid,g_ch0_pid_test_mv);
    ProductPidControllerAdapter_Process(context,elapsed_ms);
    if (ProductFbScheduleIntegration_GetCh0PidTest(&snapshot))
        g_ch0_pid_test_watch=snapshot;
}
#endif

/* Derive validation from current Snapshot metadata, even before the next
 * notification delivery; do not validate a new SV against a stale old range. */
static bool AlarmSvDomain(uint8_t ch, const ProductSensorMeasurementSnapshot_t *physical,
    int32_t *minimum, int32_t *maximum)
{
    const ProductPvProcessingConfig *c=&g_ch0_pv_processing.config;
    ProductSensorMeasurementSnapshot_t source;
    unsigned i;
    *minimum=physical->configuration_minimum; *maximum=physical->configuration_maximum;
    if (ch!=0U || g_ch0_alarm_source==PRODUCT_CH0_ALARM_PV_RAW) return true;
    if (!c->switch_enabled && c->linearization[0].mode==FB_INPUT_LINEARIZATION_BYPASS) {
        if (!ProductSensorMeasurementService_GetSnapshot((uint8_t)c->source[0],&source) ||
            !source.configuration_range_valid || !IsTemperatureSensor(source.sensor_type)) return false;
        *minimum=source.configuration_minimum; *maximum=source.configuration_maximum;
        return true;
    }
    *minimum=FB_ALARM_LIMIT_MIN; *maximum=FB_ALARM_LIMIT_MAX;
    if (PvBlocked()) return false;
    for (i=0;i<(c->switch_enabled ? 2U : 1U);++i) {
        uint8_t src=(uint8_t)c->source[i];
        if (!ProductSensorMeasurementService_GetSnapshot(src,&source) ||
            source.configuration_revision!=g_notifications[src].configuration_revision) return false;
    }
    return true;
}

/* A transaction publishes targets, not computed FB outputs. Consumers execute
 * on their next release. Serialization provides atomicity relative to callbacks. */
ProductChannelSvResult ProductFbScheduleIntegration_SetChannelSv(uint8_t ch, int32_t sv)
{
    ProductSensorMeasurementSnapshot_t s;
    int32_t minimum, maximum;
    unsigned i;
    bool alarm_compatible;
    ProductChannelSvResult result=PRODUCT_CHANNEL_SV_OK;
    if (!g_sv_ready) return PRODUCT_CHANNEL_SV_NOT_INITIALIZED;
    if (ch>=PRODUCT_AL_CHANNEL_COUNT) return PRODUCT_CHANNEL_SV_INVALID_ARGUMENT;
    if (g_sv_busy) return PRODUCT_CHANNEL_SV_BUSY;
    if (!ProductSensorMeasurementService_GetSnapshot(ch,&s) ||
        !s.configuration_range_valid || !IsTemperatureSensor(s.sensor_type) ||
        s.configuration_minimum>s.configuration_maximum)
        return PRODUCT_CHANNEL_SV_RANGE_UNAVAILABLE;
    if (sv<s.configuration_minimum || sv>s.configuration_maximum)
        return PRODUCT_CHANNEL_SV_OUT_OF_RANGE;
    alarm_compatible=AlarmSvDomain(ch,&s,&minimum,&maximum);
    if (!ProductAlarmLogicAdapter_SetpointFitsRange(&g_alarm_logic,ch,sv,alarm_compatible,minimum,maximum))
        return PRODUCT_CHANNEL_SV_ALARM_RANGE;
    if (g_channel_sv[ch].configured && g_channel_sv[ch].target==sv)
        return PRODUCT_CHANNEL_SV_UNCHANGED;
    g_sv_busy=true;
    for (i=0;i<PRODUCT_CHANNEL_SV_CONSUMER_COUNT;++i) {
        const ProductChannelSvConsumer *c=&g_sv_consumers[ch][i];
        if (c->at_running && c->at_running(c->context)) {
            result=PRODUCT_CHANNEL_SV_AT_BUSY; break;
        }
    }
    if (result==PRODUCT_CHANNEL_SV_OK)
        for (i=0;i<PRODUCT_CHANNEL_SV_CONSUMER_COUNT;++i) {
            const ProductChannelSvConsumer *c=&g_sv_consumers[ch][i];
            if (!c->validate) continue;
            result=c->validate(c->context,sv);
            if (result!=PRODUCT_CHANNEL_SV_OK) {
                if (result!=PRODUCT_CHANNEL_SV_AT_BUSY) result=PRODUCT_CHANNEL_SV_CONSUMER_REJECTED;
                break;
            }
        }
    if (result==PRODUCT_CHANNEL_SV_OK) {
        g_channel_sv[ch].target=sv; g_channel_sv[ch].configured=true;
        ++g_channel_sv[ch].revision;
        /* Commit to the owned adapter directly; its public setter delegates here. */
        g_alarm_logic.setpoint[ch]=sv; g_alarm_logic.setpoint_configured[ch]=true;
        for (i=0;i<PRODUCT_CHANNEL_SV_CONSUMER_COUNT;++i) {
            const ProductChannelSvConsumer *c=&g_sv_consumers[ch][i];
            if (c->commit) c->commit(c->context,sv);
        }
    }
    g_sv_busy=false;
    return result;
}

bool ProductFbScheduleIntegration_GetChannelSv(uint8_t ch, ProductChannelSv *out)
{
    if (!g_sv_ready || ch>=PRODUCT_AL_CHANNEL_COUNT || !out) return false;
    *out=g_channel_sv[ch]; return true;
}

bool ProductFbScheduleIntegration_BindChannelSvConsumer(uint8_t ch,
    ProductChannelSvConsumerSlot slot, const ProductChannelSvConsumer *consumer)
{
    ProductChannelSvConsumer *old;
    bool accepted=true;
    if (!g_sv_ready || g_sv_busy || ch>=PRODUCT_AL_CHANNEL_COUNT ||
        slot<PRODUCT_CHANNEL_SV_PID || slot>=PRODUCT_CHANNEL_SV_CONSUMER_COUNT) return false;
#if PRODUCT_PID_CH0_TEST_ENABLED
    if (ch==0U && slot==PRODUCT_CHANNEL_SV_PID) return false;
#endif
    if (consumer && (!consumer->validate || !consumer->commit ||
        (slot==PRODUCT_CHANNEL_SV_PID && !consumer->at_running))) return false;
    if (consumer && g_channel_sv[ch].configured &&
        ProductFbScheduleIntegration_SetChannelSv(ch,g_channel_sv[ch].target)!=PRODUCT_CHANNEL_SV_UNCHANGED)
        return false;
    old=&g_sv_consumers[ch][slot];
    g_sv_busy=true;
    if ((old->at_running && old->at_running(old->context)) ||
        (consumer && consumer->at_running && consumer->at_running(consumer->context))) accepted=false;
    if (accepted && consumer && g_channel_sv[ch].configured)
        accepted=consumer->validate(consumer->context,g_channel_sv[ch].target)==PRODUCT_CHANNEL_SV_OK;
    if (accepted) {
        *old=consumer ? *consumer : (ProductChannelSvConsumer){0};
        if (consumer && g_channel_sv[ch].configured)
            consumer->commit(consumer->context,g_channel_sv[ch].target);
    }
    g_sv_busy=false;
    return accepted;
}

static bool IntegratedSetpointRequest(void *context, uint8_t ch, int32_t sv)
{
    ProductChannelSvResult result;
    (void)context;
    result=ProductFbScheduleIntegration_SetChannelSv(ch,sv);
    return result==PRODUCT_CHANNEL_SV_OK || result==PRODUCT_CHANNEL_SV_UNCHANGED;
}

static bool ReadAlarmSample(void *context, uint8_t channel, ProductAlSample_t *sample)
{
    ProductSensorMeasurementSnapshot_t snapshot;
    (void)context;
    if (channel==0U && g_ch0_alarm_source==PRODUCT_CH0_ALARM_PV_PROCESSED) {
        ProductPvProcessingOutput pv;
        bool changed;
        unsigned i;
        if (!ProductPvProcessing_GetOutput(&g_ch0_pv_processing,&pv)) return false;
        sample->valid=pv.valid && pv.timestamp_ms==g_fb_now_ms;
        for (i=0;i<2;++i) {
            bool contributes=pv.selected==FB_SWITCH_BOTH ||
                (i==0 && pv.selected==FB_SWITCH_SOURCE1) ||
                (i==1 && pv.selected==FB_SWITCH_SOURCE2);
            if (contributes && PRODUCT_AL_MAX_SAMPLE_AGE_MS!=0U &&
                g_fb_now_ms-pv.sources[i].raw.timestamp_ms>PRODUCT_AL_MAX_SAMPLE_AGE_MS)
                sample->valid=false;
        }
        sample->pv=sample->valid ? pv.pv : 0;
        if (sample->valid) {
            changed=!g_processed_alarm_has_sample || pv.selected!=g_processed_alarm_previous.selected ||
                pv.pv!=g_processed_alarm_previous.pv;
            for (i=0;i<2;++i) {
                bool contributes=pv.selected==FB_SWITCH_BOTH ||
                    (i==0 && pv.selected==FB_SWITCH_SOURCE1) ||
                    (i==1 && pv.selected==FB_SWITCH_SOURCE2);
                if (contributes && (pv.sources[i].raw.sequence!=g_processed_alarm_previous.sources[i].raw.sequence ||
                    pv.sources[i].raw.timestamp_ms!=g_processed_alarm_previous.sources[i].raw.timestamp_ms))
                    changed=true;
            }
            if (changed) ++g_processed_alarm_sequence;
            g_processed_alarm_previous=pv; g_processed_alarm_has_sample=true;
        }
        sample->sequence=g_processed_alarm_sequence;
        return true;
    }
    if (!ProductSensorMeasurementService_GetSnapshot(channel, &snapshot)) return false;
    sample->pv = snapshot.engineering_value;
    sample->sequence = snapshot.sample_sequence;
    sample->valid = SnapshotUsable(&snapshot);
    switch (snapshot.sensor_type)
    {
        case PRODUCT_SENSOR_TYPE_TC_B: case PRODUCT_SENSOR_TYPE_TC_C:
        case PRODUCT_SENSOR_TYPE_TC_D: case PRODUCT_SENSOR_TYPE_TC_E:
        case PRODUCT_SENSOR_TYPE_TC_J: case PRODUCT_SENSOR_TYPE_TC_K:
        case PRODUCT_SENSOR_TYPE_TC_N: case PRODUCT_SENSOR_TYPE_TC_R:
        case PRODUCT_SENSOR_TYPE_TC_S: case PRODUCT_SENSOR_TYPE_TC_T:
        case PRODUCT_SENSOR_TYPE_TC_L: case PRODUCT_SENSOR_TYPE_TC_U:
        case PRODUCT_SENSOR_TYPE_TC_TXK: case PRODUCT_SENSOR_TYPE_RTD_100_OHM:
        case PRODUCT_SENSOR_TYPE_RTD_1000_OHM: case PRODUCT_SENSOR_TYPE_RTD_JPT100:
        case PRODUCT_SENSOR_TYPE_RTD_NI120: case PRODUCT_SENSOR_TYPE_RTD_CU50:
            break;
        default: sample->valid = false; break;
    }
    return true;
}

static bool ReadLogicDi(void *context, uint8_t index, bool *value)
{
    DigitalInputSnapshot_t snapshot;
    (void)context;
    if (index >= 16U || !DigitalInputService_GetSnapshot(&snapshot) ||
        (snapshot.valid_mask & (uint16_t)(1UL << index)) == 0U) return false;
    *value = (snapshot.state_mask & (uint16_t)(1UL << index)) != 0U;
    return true;
}

static ProductFbMovingAverageAdapter_t g_ch0_moving_average_adapter;
static ProductFbSchedule_t g_schedule;
static uint32_t g_ch0_measurement_revision;
static bool g_ch0_ma_source_valid;
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
    sample->valid = SnapshotUsable(&snapshot);
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
    {true, 20U, ProcessCh0Pv, &g_ch0_pv_processing},
    {true, PRODUCT_AL_PERIOD_MS, ProductAlarmLogicAdapter_Process, &g_alarm_logic},
    PRODUCT_FB_SCHEDULE_TASKS(MAKE_TASK)
};
#undef MAKE_TASK
#undef PRODUCT_FB_SCHEDULE_CH0_MOVING_AVERAGE_ENABLED
#undef PRODUCT_FB_SCHEDULE_CH0_MOVING_AVERAGE_CALLBACK
#undef PRODUCT_FB_SCHEDULE_CH0_MOVING_AVERAGE_CONTEXT

bool ProductFbScheduleIntegration_Init(uint32_t now_ms)
{
    ProductPvProcessingConfig pv_config={0};
    g_sv_ready=false; g_sv_busy=false;
    for (unsigned ch=0;ch<PRODUCT_AL_CHANNEL_COUNT;++ch) {
        g_channel_sv[ch]=(ProductChannelSv){0};
        for (unsigned i=0;i<PRODUCT_CHANNEL_SV_CONSUMER_COUNT;++i)
            g_sv_consumers[ch][i]=(ProductChannelSvConsumer){0};
    }
    g_fb_now_ms=now_ms;
    for (unsigned ch=0;ch<PRODUCT_AL_CHANNEL_COUNT;++ch) {
        g_configuration_seen[ch]=false;
        g_notifications[ch]=(ProductChannelConfigurationNotification){0};
    }
    g_pv_configuration_review_required=false;
#if PRODUCT_PID_CH0_TEST_ENABLED
    g_pid_configuration_review_required=false;
#endif
    g_ch0_measurement_revision=0U;
    g_ch0_ma_source_valid=false;
    pv_config.source[0]=0U;
    pv_config.source[1]=1U;
    pv_config.maximum_age_ms=200U;
    pv_config.maximum_skew_ms=20U;
    g_ch0_alarm_source=PRODUCT_CH0_ALARM_PV_RAW;
    g_processed_alarm_has_sample=false; g_processed_alarm_sequence=0;
    g_processed_alarm_previous=(ProductPvProcessingOutput){0};
    /* Default read-only CH0 bypass; no point table or switch activated. */
    if (!ProductPvProcessing_Init(&g_ch0_pv_processing,&pv_config,ReadProcessingSample,NULL))
        return false;
    if (!ProductAlarmLogicAdapter_Init(&g_alarm_logic, ReadAlarmSample, ReadLogicDi, NULL))
        return false;
    g_alarm_logic.setpoint_handler=IntegratedSetpointRequest;
#if PRODUCT_PID_CH0_TEST_ENABLED
    {
        const ProductPidControllerAdapterConfig_t pid_config={
            FB_PID_MODE_PID,FB_PID_MODE_PID,FB_PID_ACTION_HEAT,200U,100U,0U};
        if (g_ch0_pid_test.initialized) ProductPidControllerAdapter_Deinit(&g_ch0_pid_test);
        g_ch0_pid_test_enable=false; g_ch0_pid_test_reset=false; g_ch0_pid_test_sv=0;
        g_ch0_pid_test_sv_result=PRODUCT_CHANNEL_SV_UNCHANGED;
        g_ch0_pid_test_input=(ProductPidControllerInput_t){0};
        g_ch0_pid_test_mv=0;
        g_ch0_pid_test_watch=(ProductCh0PidTestDiagnostics){0};
        if (!ProductPidControllerAdapter_Init(&g_ch0_pid_test,&pid_config,ReadPidTestSample,NULL))
            return false;
        g_sv_consumers[0][PRODUCT_CHANNEL_SV_PID]=(ProductChannelSvConsumer){
            ValidatePidTestSv,CommitPidTestSv,PidTestAtRunning,&g_ch0_pid_test};
        if (!ProductAlarmLogicAdapter_SetPidStep(&g_alarm_logic,
                ProcessCh0PidTest,&g_ch0_pid_test)) return false;
    }
#endif
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
    g_sv_ready=ProductFbSchedule_Init(
        &g_schedule, g_tasks, g_task_states,
        sizeof(g_tasks) / sizeof(g_tasks[0]), now_ms);
    return g_sv_ready;
}

ProductAlarmLogicAdapter_t *ProductFbScheduleIntegration_GetAlarmLogicAdapter(void)
{
    return &g_alarm_logic;
}


bool ProductFbScheduleIntegration_GetChannelConfigurationNotification(
    uint8_t ch, ProductChannelConfigurationNotification *out)
{
    if (ch>=PRODUCT_AL_CHANNEL_COUNT || !out || !g_configuration_seen[ch]) return false;
    *out=g_notifications[ch]; return true;
}
bool ProductFbScheduleIntegration_IsCh0PvConfigurationBlocked(void) { return PvBlocked(); }

static void RefreshProcessedAlarmDomain(void)
{
    ProductSensorMeasurementSnapshot_t s;
    bool bypass=!g_ch0_pv_processing.config.switch_enabled &&
        g_ch0_pv_processing.config.linearization[0].mode==FB_INPUT_LINEARIZATION_BYPASS;
    bool compatible=!PvBlocked();
    int32_t minimum=FB_ALARM_LIMIT_MIN, maximum=FB_ALARM_LIMIT_MAX;
    if (bypass) {
        compatible=ProductSensorMeasurementService_GetSnapshot(
            (uint8_t)g_ch0_pv_processing.config.source[0],&s) && s.configuration_range_valid;
        if (compatible) { minimum=s.configuration_minimum; maximum=s.configuration_maximum; }
    }
    (void)ProductAlarmLogicAdapter_NotifyChannelConfigurationChanged(&g_alarm_logic,0U,
        compatible,minimum,maximum);
}

static void DeliverConfigurationChanges(void)
{
    unsigned ch;
    for (ch=0;ch<PRODUCT_AL_CHANNEL_COUNT;++ch) {
        ProductSensorMeasurementSnapshot_t s;
        ProductChannelConfigurationNotification *n=&g_notifications[ch];
        bool pv_depends;
        if (!ProductSensorMeasurementService_GetSnapshot((uint8_t)ch,&s)) continue;
        if (!g_configuration_seen[ch] && s.configuration_revision==0U) continue;
        if (g_configuration_seen[ch] && n->configuration_revision==s.configuration_revision) continue;
        g_configuration_seen[ch]=true;
        n->configuration_revision=s.configuration_revision; ++n->notification_count;
        n->moving_average_reset=n->pv_reset=n->pid_reset=false;
        n->alarm_channels_reset=0U;
        pv_depends=g_ch0_pv_processing.config.source[0]==ch ||
            (g_ch0_pv_processing.config.switch_enabled && g_ch0_pv_processing.config.source[1]==ch);
        if (ch==0U) {
            (void)ProductFbMovingAverageAdapter_Reset(&g_ch0_moving_average_adapter);
            n->moving_average_reset=true;
#if PRODUCT_PID_CH0_TEST_ENABLED
            (void)ProductPidControllerAdapter_Reset(&g_ch0_pid_test);
            g_ch0_pid_test_input=(ProductPidControllerInput_t){0};
            g_pid_configuration_review_required=true; n->pid_reset=true;
#endif
        }
        if (pv_depends) {
            (void)ProductPvProcessing_Process(&g_ch0_pv_processing,g_fb_now_ms,false,true);
            g_processed_alarm_has_sample=false; n->pv_reset=true;
            if (g_ch0_pv_processing.config.switch_enabled ||
                g_ch0_pv_processing.config.linearization[0].mode!=FB_INPUT_LINEARIZATION_BYPASS)
                g_pv_configuration_review_required=true;
        }
        /* A raw Alarm belongs to its channel; processed CH0 follows actual PV sources. */
        if (ch!=0U || g_ch0_alarm_source==PRODUCT_CH0_ALARM_PV_RAW) {
            (void)ProductAlarmLogicAdapter_NotifyChannelConfigurationChanged(&g_alarm_logic,
                (uint8_t)ch,s.configuration_range_valid,s.configuration_minimum,s.configuration_maximum);
            n->alarm_channels_reset|=1UL<<ch;
        }
        if (pv_depends && g_ch0_alarm_source==PRODUCT_CH0_ALARM_PV_PROCESSED) {
            /* Transformed-domain semantics require explicit reconfiguration review. */
            RefreshProcessedAlarmDomain();
            n->alarm_channels_reset|=1UL;
        }
    }
}

void ProductFbScheduleIntegration_Process(uint32_t now_ms)
{
    ProductSensorMeasurementSnapshot_t measurement;
    bool source_valid;
    g_fb_now_ms=now_ms;
    DeliverConfigurationChanges();
#if PRODUCT_PID_CH0_TEST_ENABLED
    HandlePidTestSvRequest();
#endif
    source_valid = ProductSensorMeasurementService_GetSnapshot(0U, &measurement);
    if (source_valid)
    {
        if (measurement.quality_revision != g_ch0_measurement_revision)
        {
            (void)ProductFbMovingAverageAdapter_Reset(&g_ch0_moving_average_adapter);
            g_ch0_measurement_revision = measurement.quality_revision;
        }
        source_valid = SnapshotUsable(&measurement);
    }
    if (!source_valid && g_ch0_ma_source_valid)
        (void)ProductFbMovingAverageAdapter_Reset(&g_ch0_moving_average_adapter);
    g_ch0_ma_source_valid=source_valid;
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

bool ProductFbScheduleIntegration_ConfigureCh0PvProcessing(const ProductPvProcessingConfig *config)
{
    if (config==NULL || config->source[0]>=PRODUCT_AL_CHANNEL_COUNT ||
        (config->switch_enabled && config->source[1]>=PRODUCT_AL_CHANNEL_COUNT)) return false;
    if (!ProductPvProcessing_ValidateConfig(config) || !g_ch0_pv_processing.initialized) return false;
    if (!PvConfigurationFitsRanges(config)) return false;
    if (g_ch0_alarm_source==PRODUCT_CH0_ALARM_PV_PROCESSED &&
        !ProductAlarmLogicAdapter_ResetDisabledChannel(&g_alarm_logic,0U)) return false;
    if (!ProductPvProcessing_SetConfig(&g_ch0_pv_processing,config)) return false;
    g_pv_configuration_review_required=false;
    if (g_ch0_alarm_source==PRODUCT_CH0_ALARM_PV_PROCESSED)
        RefreshProcessedAlarmDomain();
    g_processed_alarm_has_sample=false;
    return true;
}

bool ProductFbScheduleIntegration_SetCh0AlarmPvSource(ProductCh0AlarmPvSource source)
{
    if (!g_alarm_logic.initialized || (source!=PRODUCT_CH0_ALARM_PV_RAW &&
        source!=PRODUCT_CH0_ALARM_PV_PROCESSED)) return false;
    if (source==g_ch0_alarm_source) return true;
    if (!ProductAlarmLogicAdapter_ResetDisabledChannel(&g_alarm_logic,0U)) return false;
    g_ch0_alarm_source=source; g_processed_alarm_has_sample=false;
    if (g_configuration_seen[0] || (source==PRODUCT_CH0_ALARM_PV_PROCESSED &&
        (g_configuration_seen[g_ch0_pv_processing.config.source[0]] ||
         (g_ch0_pv_processing.config.switch_enabled &&
          g_configuration_seen[g_ch0_pv_processing.config.source[1]])))) {
        ProductSensorMeasurementSnapshot_t s;
        if (source==PRODUCT_CH0_ALARM_PV_RAW &&
            ProductSensorMeasurementService_GetSnapshot(0U,&s))
            (void)ProductAlarmLogicAdapter_NotifyChannelConfigurationChanged(&g_alarm_logic,0U,
                s.configuration_range_valid,s.configuration_minimum,s.configuration_maximum);
        else if (source==PRODUCT_CH0_ALARM_PV_PROCESSED)
            RefreshProcessedAlarmDomain();
    }
    return true;
}

ProductCh0AlarmPvSource ProductFbScheduleIntegration_GetCh0AlarmPvSource(void)
{
    return g_ch0_alarm_source;
}

bool ProductFbScheduleIntegration_GetCh0ProcessedPv(ProductPvProcessingOutput *output)
{
    return ProductPvProcessing_GetOutput(&g_ch0_pv_processing,output);
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
