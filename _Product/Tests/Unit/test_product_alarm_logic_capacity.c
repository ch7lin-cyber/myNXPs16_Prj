#include "product_alarm_logic_adapter.h"
#include <assert.h>
#include <stdio.h>

static bool Sample(void *context, uint8_t channel, ProductAlSample_t *sample)
{
    (void)context;
    (void)channel;
    sample->pv = 12345;
    sample->sequence = 1U;
    sample->valid = true;
    return true;
}

int main(void)
{
    ProductAlarmLogicAdapter_t a;
    FbAlarmConfig_t alarm;
    FbLogicConfig_t logic;
    ProductAlSource_t sources[4] = {{0}};
    FbAlarmOutput_t ao;
    FbLogicOutput_t lo;
    bool valid;
    unsigned ch, i;
    assert(ProductAlarmLogicAdapter_Init(&a, Sample, NULL, NULL));
    FbAlarm_DefaultConfig(&alarm);
    alarm.mode = FB_ALARM_MODE_ABSOLUTE_HIGH;
    alarm.alarm_high = 12344;
    for (ch = 0U; ch < PRODUCT_AL_CHANNEL_COUNT; ++ch)
        for (i = 0U; i < PRODUCT_AL_ALARMS_PER_CHANNEL; ++i)
            assert(ProductAlarmLogicAdapter_SetAlarm(&a, (uint8_t)ch, (uint8_t)i, &alarm, true));
    FbLogic_DefaultConfig(&logic);
    logic.operator_type = FB_LOGIC_OPERATOR_OR;
    sources[0].kind = PRODUCT_AL_SOURCE_ALARM;
    sources[0].channel = PRODUCT_AL_CHANNEL_COUNT - 1U;
    sources[0].index = PRODUCT_AL_ALARMS_PER_CHANNEL - 1U;
    for (i = 0U; i < PRODUCT_AL_LOGIC_COUNT; ++i)
        assert(ProductAlarmLogicAdapter_SetLogic(&a, (uint8_t)i, &logic, sources, true));
    ProductAlarmLogicAdapter_Process(&a, 20U);
    assert(ProductAlarmLogicAdapter_GetAlarm(&a, sources[0].channel, sources[0].index, &ao));
    assert(ao.alarm_active);
    assert(ProductAlarmLogicAdapter_GetLogic(&a, PRODUCT_AL_LOGIC_COUNT - 1U, &lo, &valid));
    assert(valid && lo.value == 1U);
    assert(!ProductAlarmLogicAdapter_Command(&a, PRODUCT_AL_CHANNEL_COUNT, 0U, true, false));
    puts("PASS: configurable L05 capacities (2 channels x 2 alarms, 4 Logic)");
    return 0;
}
