#include "product_fb_schedule_integration.h"
#include "product_sensor_measurement_service.h"
#include "product_temperature_input_types.h"
#include "DigitalInputService.h"
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
static ProductSensorMeasurementSnapshot_t snapshot;
bool ProductSensorMeasurementService_GetSnapshot(uint8_t channel, ProductSensorMeasurementSnapshot_t *out)
{
    if (channel!=0 || out==NULL) return false;
    *out=snapshot; return true;
}
bool DigitalInputService_GetSnapshot(DigitalInputSnapshot_t *out)
{ (void)out; return false; }
static FbAlarmOutput_t Alarm(ProductAlarmLogicAdapter_t *a)
{
    FbAlarmOutput_t out;
    assert(ProductAlarmLogicAdapter_GetAlarm(a,0,0,&out)); return out;
}
int main(void)
{
    ProductAlarmLogicAdapter_t *a;
    FbAlarmConfig_t alarm;
    ProductPvProcessingConfig c={0};
    ProductPvProcessingOutput pv;
    const FbLinearizationPoint points[]={{0,10000},{100000,110000}};
    assert(ProductFbScheduleIntegration_Init(0));
    a=ProductFbScheduleIntegration_GetAlarmLogicAdapter();
    FbAlarm_DefaultConfig(&alarm);
    alarm.mode=FB_ALARM_MODE_ABSOLUTE_HIGH; alarm.alarm_high=30000;
    alarm.options=FB_ALARM_OPTION_PEAK;
    snapshot.sensor_type=PRODUCT_SENSOR_TYPE_TC_K; snapshot.valid=true;
    snapshot.processed_timestamp_valid=true; snapshot.processed_timestamp_ms=10;
    snapshot.engineering_value=25000; snapshot.sample_sequence=1;
    c.maximum_age_ms=1000; c.maximum_skew_ms=20;
    c.linearization[0].mode=FB_INPUT_LINEARIZATION_VALUE;
    c.linearization[0].parameters.curve=(FbLinearizationConfig){points,2,FB_LINEARIZATION_CLAMP};
    assert(ProductFbScheduleIntegration_ConfigureCh0PvProcessing(&c));
    assert(ProductFbScheduleIntegration_GetCh0AlarmPvSource()==PRODUCT_CH0_ALARM_PV_RAW);
    assert(ProductAlarmLogicAdapter_SetAlarm(a,0,0,&alarm,true));
    ProductFbScheduleIntegration_Process(20);
    assert(!Alarm(a).alarm_active); /* raw25C, despite processed35C */
    assert(!ProductFbScheduleIntegration_SetCh0AlarmPvSource(PRODUCT_CH0_ALARM_PV_PROCESSED));
    assert(ProductAlarmLogicAdapter_SetAlarm(a,0,0,&alarm,false));
    assert(ProductFbScheduleIntegration_SetCh0AlarmPvSource(PRODUCT_CH0_ALARM_PV_PROCESSED));
    assert(!a->has_sequence[0] && !Alarm(a).peak_valid);
    assert(ProductAlarmLogicAdapter_SetAlarm(a,0,0,&alarm,true));
    ProductFbScheduleIntegration_Process(40);
    assert(Alarm(a).alarm_active && Alarm(a).peak_high==35000);
    assert(snapshot.engineering_value==25000);
    uint32_t sequence=a->sequence[0];
    ProductFbScheduleIntegration_Process(60);
    assert(a->sequence[0]==sequence && a->age_ms[0]==20);
    /* Repeated publication does not defeat Alarm's independent 200ms age. */
    ProductFbScheduleIntegration_Process(260);
    assert(Alarm(a).status==FB_ALARM_STATUS_INVALID_INPUT && !Alarm(a).alarm_active);
    ProductFbScheduleIntegration_GetCh0ProcessedPv(&pv); assert(pv.valid); /* pipeline age limit1000 */
    assert(!ProductFbScheduleIntegration_ConfigureCh0PvProcessing(&c)); /* consumed chain protected */
    assert(!ProductFbScheduleIntegration_SetCh0AlarmPvSource(PRODUCT_CH0_ALARM_PV_RAW));
    assert(ProductFbScheduleIntegration_SetCh0AlarmPvSource(PRODUCT_CH0_ALARM_PV_PROCESSED)); /* no-op */
    snapshot.processed_timestamp_ms=270; ++snapshot.sample_sequence;
    ProductFbScheduleIntegration_Process(280);
    assert(Alarm(a).alarm_active && a->sequence[0]!=sequence);
    snapshot.valid=false; ProductFbScheduleIntegration_Process(300);
    assert(Alarm(a).status==FB_ALARM_STATUS_INVALID_INPUT); /* never falls back to raw */
    snapshot.valid=true; snapshot.processed_timestamp_valid=false;
    ProductFbScheduleIntegration_Process(320);
    assert(Alarm(a).status==FB_ALARM_STATUS_INVALID_INPUT);
    snapshot.processed_timestamp_valid=true; snapshot.processed_timestamp_ms=330;
    ++snapshot.sample_sequence; ProductFbScheduleIntegration_Process(340);
    assert(Alarm(a).alarm_active);
    assert(ProductAlarmLogicAdapter_SetAlarm(a,0,0,&alarm,false));
    assert(ProductFbScheduleIntegration_SetCh0AlarmPvSource(PRODUCT_CH0_ALARM_PV_RAW));
    assert(!Alarm(a).peak_valid && !a->has_sequence[0]);
    assert(ProductAlarmLogicAdapter_SetAlarm(a,0,0,&alarm,true));
    ProductFbScheduleIntegration_Process(360);
    assert(!Alarm(a).alarm_active && !Alarm(a).peak_valid);
    assert(!ProductFbScheduleIntegration_SetCh0AlarmPvSource((ProductCh0AlarmPvSource)99));
    assert(!ProductAlarmLogicAdapter_ResetDisabledChannel(a,0));
    assert(!ProductAlarmLogicAdapter_ResetDisabledChannel(a,PRODUCT_AL_CHANNEL_COUNT));
    assert(!ProductAlarmLogicAdapter_ResetDisabledChannel(NULL,0));
    assert(ProductFbScheduleIntegration_Init(400));
    assert(ProductFbScheduleIntegration_GetCh0AlarmPvSource()==PRODUCT_CH0_ALARM_PV_RAW);
    /* A freshly selected source still cannot smuggle an already old sample. */
    snapshot.processed_timestamp_ms=100;
    assert(ProductFbScheduleIntegration_ConfigureCh0PvProcessing(&c));
    assert(ProductFbScheduleIntegration_SetCh0AlarmPvSource(PRODUCT_CH0_ALARM_PV_PROCESSED));
    assert(ProductAlarmLogicAdapter_SetAlarm(a,0,0,&alarm,true));
    ProductFbScheduleIntegration_Process(420);
    ProductFbScheduleIntegration_GetCh0ProcessedPv(&pv); assert(pv.valid);
    assert(Alarm(a).status==FB_ALARM_STATUS_INVALID_INPUT);
    puts("PASS: CH0 Alarm raw/processed opt-in, guarded changes, sample identity and no fallback");
    return 0;
}
