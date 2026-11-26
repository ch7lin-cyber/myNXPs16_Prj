#include "product_fb_schedule_integration.h"
#include "product_sensor_measurement_service.h"
#include "product_temperature_input_types.h"
#include "DigitalInputService.h"
#include <assert.h>
#include <stdio.h>
static ProductSensorMeasurementSnapshot_t samples[PRODUCT_AL_CHANNEL_COUNT];
bool ProductSensorMeasurementService_GetSnapshot(uint8_t ch,ProductSensorMeasurementSnapshot_t *s)
{ if (ch>=PRODUCT_AL_CHANNEL_COUNT) return false; *s=samples[ch]; return true; }
bool DigitalInputService_GetSnapshot(DigitalInputSnapshot_t *s)
{ s->valid_mask=0xffff; s->state_mask=0xffff; return true; }
static void Tick(uint32_t now)
{
    unsigned ch;
    for(ch=0;ch<PRODUCT_AL_CHANNEL_COUNT;++ch) {
        samples[ch].processed_timestamp_ms=now; ++samples[ch].sample_sequence;
    }
    ProductFbScheduleIntegration_Process(now);
}
static void Init(void)
{
    unsigned ch;
    assert(ProductFbScheduleIntegration_Init(0));
    for(ch=0;ch<PRODUCT_AL_CHANNEL_COUNT;++ch) {
        samples[ch]=(ProductSensorMeasurementSnapshot_t){0};
        samples[ch].sensor_type=PRODUCT_SENSOR_TYPE_TC_K;
        samples[ch].valid=true; samples[ch].input_error=61;
        samples[ch].processed_timestamp_valid=true;
        samples[ch].engineering_value=100000;
        samples[ch].configuration_revision=1;
        samples[ch].configuration_minimum=-200000;
        samples[ch].configuration_maximum=1300000;
        samples[ch].configuration_range_valid=true;
    }
    Tick(1); /* initial publication before configuring histories */
}
static void Alarm(ProductAlarmLogicAdapter_t *a,unsigned ch,unsigned i,int32_t limit)
{
    FbAlarmConfig_t c; FbAlarm_DefaultConfig(&c);
    c.mode=FB_ALARM_MODE_ABSOLUTE_HIGH; c.alarm_high=limit;
    c.reset_mode=FB_ALARM_RESET_MANUAL; c.options=FB_ALARM_OPTION_PEAK;
    assert(ProductAlarmLogicAdapter_SetAlarm(a,(uint8_t)ch,(uint8_t)i,&c,true));
}
static void Logic(ProductAlarmLogicAdapter_t *a,unsigned i,ProductAlSourceKind_t kind,unsigned ch,unsigned index)
{
    FbLogicConfig_t c; ProductAlSource_t src[FB_LOGIC_INPUT_COUNT]={{0}};
    FbLogic_DefaultConfig(&c); c.operator_type=FB_LOGIC_OPERATOR_OR;
    src[0]=(ProductAlSource_t){kind,(uint8_t)ch,(uint8_t)index};
    assert(ProductAlarmLogicAdapter_SetLogic(a,(uint8_t)i,&c,src,true));
}
static void TestMissedInvalidAndIsolation(void)
{
    ProductChannelConfigurationNotification n;
    ProductAlarmLogicAdapter_t *a;
    FbMovingAverageOutput m;
    Init(); a=ProductFbScheduleIntegration_GetAlarmLogicAdapter();
    Alarm(a,0,0,50000); Alarm(a,1,0,50000);
    Logic(a,5,PRODUCT_AL_SOURCE_ALARM,0,0);
    Logic(a,0,PRODUCT_AL_SOURCE_LOGIC,0,5); /* forward reference */
    Logic(a,6,PRODUCT_AL_SOURCE_EXTERNAL,0,0);
    Tick(20); Tick(40); Tick(60);
    assert(a->alarms[0][0].output.alarm_latched && a->alarms[0][0].output.peak_valid);
    assert(a->logic[0].output.value && a->logic[6].output.value);
    assert(ProductFbScheduleIntegration_GetCh0MovingAverage(&m) && m.sample_count==3);
    /* Already valid at notification; no invalid scan and no scheduled release at61. */
    ++samples[0].configuration_revision; samples[0].engineering_value=20000;
    Tick(61);
    assert(!a->alarms[0][0].output.peak_valid && !a->alarms[0][0].output.alarm_latched);
    assert(!a->logic[0].valid && !a->logic[5].valid && !a->logic[0].output.value);
    assert(a->logic[6].output.value && a->alarms[1][0].output.alarm_latched);
    assert(ProductFbScheduleIntegration_GetCh0MovingAverage(&m) && m.sample_count==0);
    assert(ProductFbScheduleIntegration_GetChannelConfigurationNotification(0,&n));
    assert(n.configuration_revision==2 && n.notification_count==2 && n.moving_average_reset);
    Tick(80);
    assert(!a->alarms[0][0].output.alarm_latched && a->alarms[0][0].config.alarm_high==50000);
    assert(ProductFbScheduleIntegration_GetCh0MovingAverage(&m) && m.sample_count==1 && m.average==20000);
    Tick(81); assert(ProductFbScheduleIntegration_GetChannelConfigurationNotification(0,&n) && n.notification_count==2);
    /* Generation rollover is still a change; zero is not an implicit acknowledgement. */
    samples[0].configuration_revision=UINT32_MAX; Tick(82);
    samples[0].configuration_revision=0; Tick(83);
    assert(ProductFbScheduleIntegration_GetChannelConfigurationNotification(0,&n) && n.notification_count==4);
}
static void TestRangeAndUnitGuard(void)
{
    ProductAlarmLogicAdapter_t *a;
    Init(); a=ProductFbScheduleIntegration_GetAlarmLogicAdapter();
    Alarm(a,0,0,900000); Alarm(a,0,1,50000);
    Logic(a,5,PRODUCT_AL_SOURCE_ALARM,0,0); Logic(a,0,PRODUCT_AL_SOURCE_LOGIC,0,5);
    Logic(a,6,PRODUCT_AL_SOURCE_ALARM,0,1);
    Tick(20);
    ++samples[0].configuration_revision; samples[0].configuration_maximum=400000;
    Tick(21); Tick(40);
    assert(a->alarm_blocked[0][0] && !a->alarm_blocked[0][1]);
    assert(a->logic_blocked[0] && a->logic_blocked[5] && !a->logic_blocked[6]);
    assert(!a->logic[0].valid && !a->logic[5].valid && a->logic[6].valid);
    assert(a->alarms[0][0].config.alarm_high==900000); /* preserved */
    Alarm(a,0,0,60000); Tick(60);
    assert(!a->alarm_blocked[0][0] && !a->logic_blocked[0]);
    samples[0].sensor_type=PRODUCT_SENSOR_TYPE_VOLTAGE_0_10V;
    samples[0].configuration_range_valid=false; ++samples[0].configuration_revision;
    Tick(61); Tick(80); assert(a->alarm_blocked[0][0]);
    assert(!a->alarms[0][0].output.alarm_active && !a->logic[0].valid);
    samples[0].sensor_type=PRODUCT_SENSOR_TYPE_TC_T; samples[0].configuration_range_valid=true;
    ++samples[0].configuration_revision; Tick(100); assert(!a->alarm_blocked[0][0]);
    /* Hysteresis thresholds are relative to SV, including the release threshold. */
    FbAlarmConfig_t c; FbAlarm_DefaultConfig(&c); c.mode=FB_ALARM_MODE_HYSTERESIS_HIGH;
    c.alarm_high=100000; c.alarm_low=50000;
    assert(ProductAlarmLogicAdapter_SetSetpoint(a,0,350000));
    assert(ProductAlarmLogicAdapter_SetAlarm(a,0,0,&c,true));
    Tick(120); assert(a->alarm_blocked[0][0]);
    assert(ProductAlarmLogicAdapter_SetSetpoint(a,0,100000)); Tick(140);
    assert(!a->alarm_blocked[0][0]);
}
static void TestPvDependenciesAndReview(void)
{
    ProductPvProcessingConfig c={0}; ProductPvProcessingOutput out;
    ProductChannelConfigurationNotification n;
    Init(); c.source[0]=1; c.source[1]=2; c.maximum_age_ms=200; c.maximum_skew_ms=20;
    assert(ProductFbScheduleIntegration_ConfigureCh0PvProcessing(&c)); Tick(20);
    assert(ProductFbScheduleIntegration_SetCh0AlarmPvSource(PRODUCT_CH0_ALARM_PV_PROCESSED));
    ++samples[0].configuration_revision; Tick(21);
    assert(ProductFbScheduleIntegration_GetChannelConfigurationNotification(0,&n) && !n.pv_reset && n.alarm_channels_reset==0);
    ++samples[2].configuration_revision; Tick(22);
    assert(ProductFbScheduleIntegration_GetChannelConfigurationNotification(2,&n) && !n.pv_reset); /* inactive source2 */
    ++samples[1].configuration_revision; Tick(23);
    assert(ProductFbScheduleIntegration_GetChannelConfigurationNotification(1,&n) && n.pv_reset && (n.alarm_channels_reset&1));
    assert(ProductFbScheduleIntegration_GetCh0ProcessedPv(&out) && !out.valid);
    c.switch_enabled=true; c.switch_config=(FbInputSwitchOverConfig){-200000,1300000,200000,300000,FB_SWITCH_USE_OTHER};
    assert(ProductFbScheduleIntegration_ConfigureCh0PvProcessing(&c)); Tick(40);
    ++samples[2].configuration_revision; Tick(41); Tick(60);
    assert(ProductFbScheduleIntegration_IsCh0PvConfigurationBlocked());
    assert(ProductFbScheduleIntegration_GetCh0ProcessedPv(&out) && !out.valid);
    assert(ProductFbScheduleIntegration_ConfigureCh0PvProcessing(&c));
    assert(!ProductFbScheduleIntegration_IsCh0PvConfigurationBlocked()); Tick(80);
    assert(ProductFbScheduleIntegration_GetCh0ProcessedPv(&out) && out.valid);
    samples[2].configuration_maximum=400000; ++samples[2].configuration_revision; Tick(81);
    assert(!ProductFbScheduleIntegration_ConfigureCh0PvProcessing(&c));
    assert(ProductFbScheduleIntegration_IsCh0PvConfigurationBlocked());
    c.switch_config.sensor_high=400000;
    assert(ProductFbScheduleIntegration_ConfigureCh0PvProcessing(&c));
    samples[2].configuration_range_valid=false; ++samples[2].configuration_revision; Tick(82);
    assert(!ProductFbScheduleIntegration_ConfigureCh0PvProcessing(&c));
    assert(ProductFbScheduleIntegration_IsCh0PvConfigurationBlocked());
}
static void TestCycleAndCurveGuard(void)
{
    ProductAlarmLogicAdapter_t *a;
    ProductPvProcessingConfig c={0};
    static const FbLinearizationPoint curve[]={{0,0},{500000,500000}};
    Init(); a=ProductFbScheduleIntegration_GetAlarmLogicAdapter(); Alarm(a,0,0,50000);
    Logic(a,0,PRODUCT_AL_SOURCE_LOGIC,0,5); Logic(a,5,PRODUCT_AL_SOURCE_LOGIC,0,0);
    a->logic[5].sources[1]=(ProductAlSource_t){PRODUCT_AL_SOURCE_ALARM,0,0};
    /* Seed prior scan history: exercise cycle traversal without recursive calls. */
    a->logic[0].fb.delay_elapsed_ms=100; a->logic[5].fb.delay_elapsed_ms=100;
    a->logic[0].output.value=1; a->logic[5].output.value=1;
    ++samples[0].configuration_revision; Tick(2);
    assert(!a->logic[0].output.value && !a->logic[5].output.value);
    assert(a->logic[0].fb.delay_elapsed_ms==0 && a->logic[5].fb.delay_elapsed_ms==0);
    c.source[0]=0; c.maximum_age_ms=200; c.maximum_skew_ms=20;
    c.linearization[0].mode=FB_INPUT_LINEARIZATION_VALUE;
    c.linearization[0].parameters.curve=(FbLinearizationConfig){curve,2,FB_LINEARIZATION_CLAMP};
    assert(ProductFbScheduleIntegration_ConfigureCh0PvProcessing(&c));
    samples[0].configuration_maximum=400000; ++samples[0].configuration_revision; Tick(3);
    assert(ProductFbScheduleIntegration_IsCh0PvConfigurationBlocked());
    assert(!ProductFbScheduleIntegration_ConfigureCh0PvProcessing(&c)); /* X exceeds new range */
    assert(ProductFbScheduleIntegration_IsCh0PvConfigurationBlocked());
    c.linearization[0].mode=FB_INPUT_LINEARIZATION_BYPASS;
    assert(ProductFbScheduleIntegration_ConfigureCh0PvProcessing(&c));
    assert(!ProductFbScheduleIntegration_IsCh0PvConfigurationBlocked());
}
int main(void)
{
    TestMissedInvalidAndIsolation(); TestRangeAndUnitGuard(); TestPvDependenciesAndReview(); TestCycleAndCurveGuard();
    puts("PASS: configuration generations, missed invalid, range/unit guard, dependency closure and PV review");
    return 0;
}
