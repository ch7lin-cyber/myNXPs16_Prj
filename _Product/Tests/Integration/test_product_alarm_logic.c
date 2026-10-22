#include "product_fb_schedule_integration.h"
#include "product_sensor_measurement_service.h"
#include "product_temperature_input_types.h"
#include "DigitalInputService.h"

#include <assert.h>
#include <stddef.h>
#include <stdio.h>

static ProductSensorMeasurementSnapshot_t samples[PRODUCT_AL_CHANNEL_COUNT];
static bool available = true, di = true, di_valid = true;
static unsigned pid_calls;

bool ProductSensorMeasurementService_GetSnapshot(uint8_t ch,
    ProductSensorMeasurementSnapshot_t *snapshot)
{
    if (!available || ch >= PRODUCT_AL_CHANNEL_COUNT) return false;
    *snapshot = samples[ch];
    return true;
}

bool DigitalInputService_GetSnapshot(DigitalInputSnapshot_t *snapshot)
{
    snapshot->state_mask = di ? 1U : 0U;
    snapshot->valid_mask = di_valid ? 1U : 0U;
    return true;
}

static FbAlarmOutput_t Alarm(ProductAlarmLogicAdapter_t *a, unsigned ch, unsigned i)
{
    FbAlarmOutput_t out;
    assert(ProductAlarmLogicAdapter_GetAlarm(a, (uint8_t)ch, (uint8_t)i, &out));
    return out;
}

static uint16_t Logic(ProductAlarmLogicAdapter_t *a, unsigned i, bool expected_valid)
{
    FbLogicOutput_t out;
    bool valid;
    assert(ProductAlarmLogicAdapter_GetLogic(a, (uint8_t)i, &out, &valid));
    assert(valid == expected_valid);
    return out.value;
}

static void Pid(void *context, uint32_t ms)
{
    ProductAlarmLogicAdapter_t *a = context;
    assert(ms != 0U);
    assert(Logic(a, 0U, true) == 1U); /* current-cycle pre-PID */
    assert(Logic(a, 8U, pid_calls != 0U) == (pid_calls != 0U ? 1U : 0U));
    ++pid_calls;
}

int main(void)
{
    ProductAlarmLogicAdapter_t *a;
    FbAlarmConfig_t alarm;
    FbLogicConfig_t logic;
    ProductAlSource_t sources[4] = {{0}};
    unsigned ch, i;
    uint32_t now = 100U;
    assert(ProductFbScheduleIntegration_Init(now));
    a = ProductFbScheduleIntegration_GetAlarmLogicAdapter();
    FbAlarm_DefaultConfig(&alarm);
    alarm.mode = FB_ALARM_MODE_ABSOLUTE_HIGH;
    alarm.alarm_high = 25123;
    alarm.options = FB_ALARM_OPTION_PEAK;
    for (ch = 0; ch < PRODUCT_AL_CHANNEL_COUNT; ++ch)
    {
        samples[ch].sensor_type = PRODUCT_SENSOR_TYPE_TC_K;
        samples[ch].sample_sequence = 1U;
        samples[ch].valid = true;
        samples[ch].engineering_value = 25124;
        for (i = 0; i < PRODUCT_AL_ALARMS_PER_CHANNEL; ++i)
        {
            assert(Alarm(a, ch, i).status == FB_ALARM_STATUS_DISABLED);
            assert(ProductAlarmLogicAdapter_SetAlarm(a, (uint8_t)ch, (uint8_t)i, &alarm, true));
        }
    }
    FbLogic_DefaultConfig(&logic);
    logic.operator_type = FB_LOGIC_OPERATOR_OR;
    sources[0].kind = PRODUCT_AL_SOURCE_ALARM;
    for (i = 0; i < PRODUCT_AL_LOGIC_COUNT; ++i)
        assert(ProductAlarmLogicAdapter_SetLogic(a, (uint8_t)i, &logic, sources, true));
    assert(ProductAlarmLogicAdapter_SetPidStep(a, Pid, a));
    ProductFbScheduleIntegration_Process(now + PRODUCT_AL_PERIOD_MS - 1U);
    assert(a->cycle_count == 0U);
    now += PRODUCT_AL_PERIOD_MS;
    ProductFbScheduleIntegration_Process(now);
    assert(pid_calls == 1U && a->cycle_count == 1U);
    for (ch = 0; ch < PRODUCT_AL_CHANNEL_COUNT; ++ch)
        for (i = 0; i < PRODUCT_AL_ALARMS_PER_CHANNEL; ++i)
            assert(Alarm(a, ch, i).alarm_active);
    for (i = 0; i < PRODUCT_AL_LOGIC_COUNT; ++i) assert(Logic(a, i, true) == 1U);
    assert(ProductAlarmLogicAdapter_SetPidStep(a, NULL, NULL));
    /* MANUAL reset, independent peak clear and repeated command edge gap. */
    alarm.reset_mode = FB_ALARM_RESET_MANUAL;
    assert(ProductAlarmLogicAdapter_SetAlarm(a, 0U, 0U, &alarm, true));
    ProductAlarmLogicAdapter_Process(a, 20U);
    assert(ProductAlarmLogicAdapter_Command(a, 0U, 0U, true, false));
    assert(!ProductAlarmLogicAdapter_Command(a, 0U, 0U, true, false));
    ProductAlarmLogicAdapter_Process(a, 20U);
    assert(Alarm(a, 0U, 0U).status == FB_ALARM_STATUS_RESET_REJECTED);
    samples[0].engineering_value = 25123;
    ++samples[0].sample_sequence;
    assert(ProductAlarmLogicAdapter_Command(a, 0U, 0U, true, true));
    ProductAlarmLogicAdapter_Process(a, 20U); /* reset low; peak clear high */
    assert(Alarm(a, 0U, 0U).alarm_latched && !Alarm(a, 0U, 0U).peak_valid);
    ProductAlarmLogicAdapter_Process(a, 20U); /* next reset edge */
    assert(Alarm(a, 0U, 0U).status == FB_ALARM_STATUS_RESET);
    assert(!Alarm(a, 0U, 0U).alarm_active);
    assert(Alarm(a, 1U, 0U).alarm_active); /* channel independence */
    /* DI and Logic source chains; input failure forces zero, valid=false. */
    sources[0].kind = PRODUCT_AL_SOURCE_EXTERNAL;
    assert(ProductAlarmLogicAdapter_SetLogic(a, 0U, &logic, sources, true));
    sources[0].kind = PRODUCT_AL_SOURCE_LOGIC;
    sources[0].index = 0U;
    assert(ProductAlarmLogicAdapter_SetLogic(a, 1U, &logic, sources, true));
    ProductAlarmLogicAdapter_Process(a, 20U);
    assert(Logic(a, 0U, true) == 1U && Logic(a, 1U, true) == 1U);
    di_valid = false;
    ProductAlarmLogicAdapter_Process(a, 20U);
    assert(Logic(a, 0U, false) == 0U && Logic(a, 1U, false) == 0U);
    di_valid = true;
    ProductAlarmLogicAdapter_Process(a, 20U);
    assert(Logic(a, 1U, true) == 1U);
    /* Voltage units must not enter temperature alarm, then recover. */
    samples[0].sensor_type = PRODUCT_SENSOR_TYPE_VOLTAGE_0_10V;
    ProductAlarmLogicAdapter_Process(a, 20U);
    assert(Alarm(a, 0U, 0U).status == FB_ALARM_STATUS_INVALID_INPUT);
    samples[0].sensor_type = PRODUCT_SENSOR_TYPE_TC_K;
    samples[0].engineering_value = 26000;
    ++samples[0].sample_sequence;
    ProductAlarmLogicAdapter_Process(a, 20U);
    assert(Alarm(a, 0U, 0U).alarm_active);
    ProductAlarmLogicAdapter_Process(a, PRODUCT_AL_MAX_SAMPLE_AGE_MS + 1U);
    assert(Alarm(a, 0U, 0U).status == FB_ALARM_STATUS_INVALID_INPUT);
    ++samples[0].sample_sequence;
    ProductAlarmLogicAdapter_Process(a, 20U);
    assert(Alarm(a, 0U, 0U).alarm_active);
    available = false;
    ProductAlarmLogicAdapter_Process(a, 20U);
    assert(!Alarm(a, 0U, 0U).alarm_active);
    available = true;
    /* Illegal configs/bindings are rejected without replacing prior config. */
    alarm.alarm_high = FB_ALARM_LIMIT_MAX + 1;
    assert(!ProductAlarmLogicAdapter_SetAlarm(a, 0U, 0U, &alarm, true));
    sources[0].index = PRODUCT_AL_LOGIC_COUNT;
    assert(!ProductAlarmLogicAdapter_SetLogic(a, 0U, &logic, sources, true));
    assert(!ProductAlarmLogicAdapter_SetSetpoint(a, PRODUCT_AL_CHANNEL_COUNT, 0));
    assert(!ProductAlarmLogicAdapter_GetAlarm(a, 0U, 0U, NULL));
    assert(!ProductAlarmLogicAdapter_Init(NULL, NULL, NULL, NULL));
    /* L05 passes actual elapsed time to the L04 ON/OFF delay. */
    sources[0].kind = PRODUCT_AL_SOURCE_EXTERNAL;
    sources[0].index = 0U;
    logic.on_delay_ms = 40U;
    logic.off_delay_ms = 60U;
    assert(ProductAlarmLogicAdapter_SetLogic(a, 0U, &logic, sources, true));
    ProductAlarmLogicAdapter_Process(a, 1000U);
    assert(Logic(a, 0U, true) == 0U && a->logic[0].output.delay_elapsed_ms == 0U);
    ProductAlarmLogicAdapter_Process(a, 39U);
    assert(Logic(a, 0U, true) == 0U);
    ProductAlarmLogicAdapter_Process(a, 1U);
    assert(Logic(a, 0U, true) == 1U);
    di = false;
    ProductAlarmLogicAdapter_Process(a, 1000U);
    assert(Logic(a, 0U, true) == 1U && a->logic[0].output.off_delay_active);
    ProductAlarmLogicAdapter_Process(a, 60U);
    assert(Logic(a, 0U, true) == 0U);
    /* Forward reference reads previous-cycle output, never a future result. */
    logic.on_delay_ms = logic.off_delay_ms = 0U;
    sources[0].kind = PRODUCT_AL_SOURCE_LOGIC;
    sources[0].index = 3U;
    assert(ProductAlarmLogicAdapter_SetLogic(a, 2U, &logic, sources, true));
    sources[0].kind = PRODUCT_AL_SOURCE_ONE;
    assert(ProductAlarmLogicAdapter_SetLogic(a, 3U, &logic, sources, true));
    ProductAlarmLogicAdapter_Process(a, 20U);
    assert(Logic(a, 2U, false) == 0U && Logic(a, 3U, true) == 1U);
    ProductAlarmLogicAdapter_Process(a, 20U);
    assert(Logic(a, 2U, true) == 1U);
    puts("PASS: L05 12 Alarm / 16 Logic, ordering, units, reset/peak, stale/failure, isolation");
    return 0;
}
