#include "product_alarm_logic_adapter.h"

#include <stddef.h>
#include <string.h>

static bool Ready(const ProductAlarmLogicAdapter_t *a)
{
    return a != NULL && a->initialized;
}

bool ProductAlarmLogicAdapter_Init(ProductAlarmLogicAdapter_t *a,
    ProductAlReadSample_t sample, ProductAlReadBoolean_t boolean, void *context)
{
    unsigned ch, i;
    if (a == NULL || sample == NULL) return false;
    (void)memset(a, 0, sizeof(*a));
    a->read_sample = sample;
    a->read_boolean = boolean;
    a->provider_context = context;
    for (ch = 0; ch < PRODUCT_AL_CHANNEL_COUNT; ++ch)
        for (i = 0; i < PRODUCT_AL_ALARMS_PER_CHANNEL; ++i)
        {
            FbAlarm_DefaultConfig(&a->alarms[ch][i].config);
            a->alarms[ch][i].output.status = FB_ALARM_STATUS_DISABLED;
        }
    for (i = 0; i < PRODUCT_AL_LOGIC_COUNT; ++i)
    {
        FbLogic_DefaultConfig(&a->logic[i].config);
        a->logic[i].output.status = FB_LOGIC_STATUS_DISABLED;
    }
    a->initialized = true;
    return true;
}

bool ProductAlarmLogicAdapter_SetAlarm(ProductAlarmLogicAdapter_t *a,
    uint8_t ch, uint8_t index, const FbAlarmConfig_t *config, bool enabled)
{
    FbAlarm_t probe = {0};
    FbAlarmInput_t input = {0};
    FbAlarmOutput_t output;
    if (!Ready(a) || config == NULL || ch >= PRODUCT_AL_CHANNEL_COUNT ||
        index >= PRODUCT_AL_ALARMS_PER_CHANNEL) return false;
    input.enable = true;
    input.valid = true;
    if (FbAlarm_Execute(&probe, config, &input, &output) ==
        FB_ALARM_STATUS_INVALID_CONFIGURATION) return false;
    a->alarms[ch][index].config = *config;
    a->alarms[ch][index].enabled = enabled;
    return true;
}

bool ProductAlarmLogicAdapter_SetSetpoint(ProductAlarmLogicAdapter_t *a,
    uint8_t ch, int32_t sv)
{
    if (!Ready(a) || ch >= PRODUCT_AL_CHANNEL_COUNT) return false;
    a->setpoint[ch] = sv;
    return true;
}

bool ProductAlarmLogicAdapter_Command(ProductAlarmLogicAdapter_t *a,
    uint8_t ch, uint8_t index, bool reset, bool clear_peak)
{
    ProductAlAlarm_t *slot;
    if (!Ready(a) || ch >= PRODUCT_AL_CHANNEL_COUNT ||
        index >= PRODUCT_AL_ALARMS_PER_CHANNEL) return false;
    slot = &a->alarms[ch][index];
    /* One queued request per command: do not silently merge repeated writes. */
    if ((reset && slot->reset_pending) || (clear_peak && slot->clear_pending))
        return false;
    slot->reset_pending = slot->reset_pending || reset;
    slot->clear_pending = slot->clear_pending || clear_peak;
    return true;
}

static bool SourceValid(const ProductAlSource_t *s)
{
    switch (s->kind)
    {
        case PRODUCT_AL_SOURCE_ZERO:
        case PRODUCT_AL_SOURCE_ONE: return true;
        case PRODUCT_AL_SOURCE_ALARM:
            return s->channel < PRODUCT_AL_CHANNEL_COUNT &&
                   s->index < PRODUCT_AL_ALARMS_PER_CHANNEL;
        case PRODUCT_AL_SOURCE_LOGIC: return s->index < PRODUCT_AL_LOGIC_COUNT;
        case PRODUCT_AL_SOURCE_EXTERNAL: return true;
        default: return false;
    }
}

bool ProductAlarmLogicAdapter_SetLogic(ProductAlarmLogicAdapter_t *a,
    uint8_t i, const FbLogicConfig_t *config,
    const ProductAlSource_t sources[FB_LOGIC_INPUT_COUNT], bool enabled)
{
    unsigned n;
    if (!Ready(a) || i >= PRODUCT_AL_LOGIC_COUNT || config == NULL || sources == NULL ||
        config->operator_type < FB_LOGIC_OPERATOR_OFF ||
        config->operator_type > FB_LOGIC_OPERATOR_XOR) return false;
    for (n = 0; n < FB_LOGIC_INPUT_COUNT; ++n)
        if (!SourceValid(&sources[n])) return false;
    /* Binding changes invalidate output/history even if FB config is unchanged. */
    FbLogic_Initialize(&a->logic[i].fb);
    (void)memset(&a->logic[i].output, 0, sizeof(a->logic[i].output));
    a->logic[i].output.status = FB_LOGIC_STATUS_DISABLED;
    a->logic[i].valid = false;
    a->logic[i].config = *config;
    (void)memcpy(a->logic[i].sources, sources, sizeof(a->logic[i].sources));
    a->logic[i].enabled = enabled;
    return true;
}

bool ProductAlarmLogicAdapter_SetPidStep(ProductAlarmLogicAdapter_t *a,
    ProductAlPidStep_t callback, void *context)
{
    if (!Ready(a)) return false;
    a->pid_step = callback;
    a->pid_context = context;
    return true;
}

static bool Resolve(const ProductAlarmLogicAdapter_t *a,
    const ProductAlSource_t *s, uint16_t *value)
{
    const FbAlarmOutput_t *alarm;
    bool external;
    *value = 0U;
    switch (s->kind)
    {
        case PRODUCT_AL_SOURCE_ZERO: return true;
        case PRODUCT_AL_SOURCE_ONE: *value = 1U; return true;
        case PRODUCT_AL_SOURCE_ALARM:
            alarm = &a->alarms[s->channel][s->index].output;
            *value = alarm->alarm_active ? 1U : 0U;
            return alarm->status == FB_ALARM_STATUS_OK ||
                   alarm->status == FB_ALARM_STATUS_DISABLED ||
                   alarm->status == FB_ALARM_STATUS_RESET ||
                   alarm->status == FB_ALARM_STATUS_RESET_REJECTED;
        case PRODUCT_AL_SOURCE_LOGIC:
            *value = a->logic[s->index].output.value;
            return a->logic[s->index].valid;
        case PRODUCT_AL_SOURCE_EXTERNAL:
            if (a->read_boolean == NULL ||
                !a->read_boolean(a->provider_context, s->index, &external)) return false;
            *value = external ? 1U : 0U;
            return true;
        default: return false;
    }
}

static void RunLogic(ProductAlarmLogicAdapter_t *a, unsigned begin,
    unsigned end, uint32_t ms)
{
    unsigned i, n;
    for (i = begin; i < end; ++i)
    {
        ProductAlLogic_t *slot = &a->logic[i];
        FbLogicInput_t input = {0};
        bool valid = true;
        if (slot->enabled && slot->config.operator_type != FB_LOGIC_OPERATOR_OFF)
            for (n = 0; n < FB_LOGIC_INPUT_COUNT; ++n)
                if (!Resolve(a, &slot->sources[n], &input.inputs[n])) valid = false;
        input.enable = slot->enabled && valid;
        input.elapsed_ms = ms;
        (void)FbLogic_Execute(&slot->fb, &slot->config, &input, &slot->output);
        slot->valid = !slot->enabled || valid;
    }
}

void ProductAlarmLogicAdapter_Process(void *context, uint32_t ms)
{
    ProductAlarmLogicAdapter_t *a = context;
    unsigned ch, i;
    if (!Ready(a)) return;
    ++a->cycle_count;
    for (ch = 0; ch < PRODUCT_AL_CHANNEL_COUNT; ++ch)
    {
        ProductAlSample_t sample = {0};
        bool available = a->read_sample(a->provider_context, (uint8_t)ch, &sample);
        if (available && (!a->has_sequence[ch] || a->sequence[ch] != sample.sequence))
        {
            a->has_sequence[ch] = true;
            a->sequence[ch] = sample.sequence;
            a->age_ms[ch] = 0U;
        }
        else a->age_ms[ch] = UINT32_MAX - a->age_ms[ch] < ms ?
                             UINT32_MAX : a->age_ms[ch] + ms;
        for (i = 0; i < PRODUCT_AL_ALARMS_PER_CHANNEL; ++i)
        {
            ProductAlAlarm_t *slot = &a->alarms[ch][i];
            FbAlarmInput_t input = {0};
            input.enable = slot->enabled;
            input.valid = available && sample.valid &&
                (PRODUCT_AL_MAX_SAMPLE_AGE_MS == 0U ||
                 a->age_ms[ch] <= PRODUCT_AL_MAX_SAMPLE_AGE_MS);
            input.process_value = sample.pv;
            input.setpoint = a->setpoint[ch];
            input.elapsed_ms = ms;
            /* Force a low scan between consecutive command pulses. */
            input.reset = !slot->reset_high && slot->reset_pending;
            input.clear_peak = !slot->clear_high && slot->clear_pending;
            if (input.reset) slot->reset_pending = false;
            if (input.clear_peak) slot->clear_pending = false;
            slot->reset_high = input.reset;
            slot->clear_high = input.clear_peak;
            (void)FbAlarm_Execute(&slot->fb, &slot->config, &input, &slot->output);
        }
    }
    RunLogic(a, 0U, PRODUCT_AL_PRE_LOGIC_COUNT, ms);
    if (a->pid_step != NULL) a->pid_step(a->pid_context, ms);
    RunLogic(a, PRODUCT_AL_PRE_LOGIC_COUNT, PRODUCT_AL_LOGIC_COUNT, ms);
}

bool ProductAlarmLogicAdapter_GetAlarm(const ProductAlarmLogicAdapter_t *a,
    uint8_t ch, uint8_t index, FbAlarmOutput_t *output)
{
    if (!Ready(a) || output == NULL || ch >= PRODUCT_AL_CHANNEL_COUNT ||
        index >= PRODUCT_AL_ALARMS_PER_CHANNEL) return false;
    *output = a->alarms[ch][index].output;
    return true;
}

bool ProductAlarmLogicAdapter_GetLogic(const ProductAlarmLogicAdapter_t *a,
    uint8_t i, FbLogicOutput_t *output, bool *valid)
{
    if (!Ready(a) || i >= PRODUCT_AL_LOGIC_COUNT || output == NULL || valid == NULL)
        return false;
    *output = a->logic[i].output;
    *valid = a->logic[i].valid;
    return true;
}
