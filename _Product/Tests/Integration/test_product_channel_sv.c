#include "product_fb_schedule_integration.h"
#include "product_sensor_measurement_service.h"
#include "product_temperature_input_types.h"
#include "DigitalInputService.h"
#include "FbSetpointRamp.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
static ProductSensorMeasurementSnapshot_t samples[PRODUCT_AL_CHANNEL_COUNT];
static bool available=true;
static unsigned order[8],order_count;
typedef struct {
    int32_t target, current;
    unsigned validations, commits, id;
    bool at, reject, reenter;
} Consumer;
static ProductChannelSvResult Validate(void *context,int32_t sv)
{
    Consumer *c=context; (void)sv;
    ++c->validations;
    if (order_count<8) order[order_count++]=c->id;
    if (c->reenter) {
        assert(ProductFbScheduleIntegration_SetChannelSv(0,300000)==PRODUCT_CHANNEL_SV_BUSY);
        assert(!ProductFbScheduleIntegration_BindChannelSvConsumer(0,PRODUCT_CHANNEL_SV_RAMP,NULL));
    }
    return c->reject ? PRODUCT_CHANNEL_SV_CONSUMER_REJECTED : PRODUCT_CHANNEL_SV_OK;
}
static void Commit(void *context,int32_t sv)
{ Consumer *c=context; c->target=sv; ++c->commits; if(order_count<8) order[order_count++]=c->id+10; }
static bool At(void *context) { return ((Consumer *)context)->at; }
static ProductChannelSvConsumer Binding(Consumer *c)
{ ProductChannelSvConsumer b={Validate,Commit,At,c}; return b; }
bool ProductSensorMeasurementService_GetSnapshot(uint8_t ch,ProductSensorMeasurementSnapshot_t *s)
{ if(ch>=PRODUCT_AL_CHANNEL_COUNT || !available) return false; *s=samples[ch]; return true; }
bool DigitalInputService_GetSnapshot(DigitalInputSnapshot_t *s)
{ s->valid_mask=0xffff; s->state_mask=0xffff; return true; }
static ProductChannelSv State(unsigned ch)
{ ProductChannelSv s; assert(ProductFbScheduleIntegration_GetChannelSv((uint8_t)ch,&s)); return s; }
static void Tick(uint32_t now)
{
    unsigned ch; for(ch=0;ch<PRODUCT_AL_CHANNEL_COUNT;++ch) {
        samples[ch].processed_timestamp_ms=now; ++samples[ch].sample_sequence;
    }
    ProductFbScheduleIntegration_Process(now);
}
static void Init(void)
{
    unsigned ch; available=true;
    assert(ProductFbScheduleIntegration_Init(0));
    for(ch=0;ch<PRODUCT_AL_CHANNEL_COUNT;++ch) {
        samples[ch]=(ProductSensorMeasurementSnapshot_t){0};
        samples[ch].configuration_revision=1; samples[ch].configuration_range_valid=true;
        samples[ch].configuration_minimum=-200000; samples[ch].configuration_maximum=400000;
        samples[ch].sensor_type=PRODUCT_SENSOR_TYPE_TC_T;
        samples[ch].valid=true; samples[ch].input_error=61;
        samples[ch].processed_timestamp_valid=true; samples[ch].engineering_value=120000;
    }
    Tick(1); order_count=0;
}
static FbAlarmConfig_t Relative(FbAlarmMode_t mode)
{
    FbAlarmConfig_t c; FbAlarm_DefaultConfig(&c); c.mode=mode;
    c.alarm_high=10000; c.alarm_low=5000; return c;
}
static void TestValidation(void)
{
    ProductAlarmLogicAdapter_t *a; ProductChannelSv before;
    FbAlarmConfig_t c;
    Init(); a=ProductFbScheduleIntegration_GetAlarmLogicAdapter();
    assert(!State(0).configured);
    assert(ProductFbScheduleIntegration_SetChannelSv(PRODUCT_AL_CHANNEL_COUNT,0)==PRODUCT_CHANNEL_SV_INVALID_ARGUMENT);
    assert(!ProductFbScheduleIntegration_GetChannelSv(0,NULL));
    assert(ProductFbScheduleIntegration_SetChannelSv(0,-200000)==PRODUCT_CHANNEL_SV_OK);
    assert(ProductFbScheduleIntegration_SetChannelSv(0,400000)==PRODUCT_CHANNEL_SV_OK);
    before=State(0);
    assert(ProductFbScheduleIntegration_SetChannelSv(0,400001)==PRODUCT_CHANNEL_SV_OUT_OF_RANGE);
    assert(ProductFbScheduleIntegration_SetChannelSv(0,-200001)==PRODUCT_CHANNEL_SV_OUT_OF_RANGE);
    assert(State(0).revision==before.revision && State(0).target==400000 && a->setpoint[0]==400000);
    available=false; assert(ProductFbScheduleIntegration_SetChannelSv(0,0)==PRODUCT_CHANNEL_SV_RANGE_UNAVAILABLE);
    available=true; samples[0].configuration_range_valid=false;
    assert(ProductFbScheduleIntegration_SetChannelSv(0,0)==PRODUCT_CHANNEL_SV_RANGE_UNAVAILABLE);
    samples[0].configuration_range_valid=true; samples[0].sensor_type=PRODUCT_SENSOR_TYPE_VOLTAGE_0_10V;
    assert(ProductFbScheduleIntegration_SetChannelSv(0,0)==PRODUCT_CHANNEL_SV_RANGE_UNAVAILABLE);
    samples[0].sensor_type=PRODUCT_SENSOR_TYPE_TC_T;
    c=Relative(FB_ALARM_MODE_RELATIVE_HIGH); c.alarm_low=3000000; /* unused lower bound */
    assert(ProductAlarmLogicAdapter_SetAlarm(a,0,0,&c,true));
    assert(ProductFbScheduleIntegration_SetChannelSv(0,390000)==PRODUCT_CHANNEL_SV_OK);
    before=State(0);
    assert(ProductFbScheduleIntegration_SetChannelSv(0,390001)==PRODUCT_CHANNEL_SV_ALARM_RANGE);
    assert(State(0).revision==before.revision && a->setpoint[0]==390000);
    c=Relative(FB_ALARM_MODE_HYSTERESIS_HIGH);
    assert(ProductAlarmLogicAdapter_SetAlarm(a,0,0,&c,true));
    assert(ProductFbScheduleIntegration_SetChannelSv(0,390001)==PRODUCT_CHANNEL_SV_ALARM_RANGE);
    c=Relative(FB_ALARM_MODE_HYSTERESIS_LOW);
    assert(ProductAlarmLogicAdapter_SetAlarm(a,0,0,&c,true));
    assert(ProductFbScheduleIntegration_SetChannelSv(0,-190000)==PRODUCT_CHANNEL_SV_OK);
    assert(ProductFbScheduleIntegration_SetChannelSv(0,-190001)==PRODUCT_CHANNEL_SV_ALARM_RANGE);
    c=Relative(FB_ALARM_MODE_RELATIVE_HIGH_LOW);
    assert(ProductAlarmLogicAdapter_SetAlarm(a,0,0,&c,true));
    assert(ProductFbScheduleIntegration_SetChannelSv(0,-195001)==PRODUCT_CHANNEL_SV_ALARM_RANGE);
    c=Relative(FB_ALARM_MODE_RELATIVE_HIGH); c.alarm_high=-10000;
    assert(ProductAlarmLogicAdapter_SetAlarm(a,0,0,&c,true));
    assert(ProductFbScheduleIntegration_SetChannelSv(0,0)==PRODUCT_CHANNEL_SV_OK);
    assert(ProductFbScheduleIntegration_SetChannelSv(0,-190000)==PRODUCT_CHANNEL_SV_OK);
    assert(ProductFbScheduleIntegration_SetChannelSv(0,-190001)==PRODUCT_CHANNEL_SV_ALARM_RANGE);
    /* Candidate arithmetic cannot wrap to look legal at INT32 limits. */
    c=Relative(FB_ALARM_MODE_RELATIVE_HIGH); c.alarm_high=1;
    assert(ProductAlarmLogicAdapter_SetAlarm(a,0,0,&c,true));
    assert(!ProductAlarmLogicAdapter_SetpointFitsRange(a,0,INT32_MAX,true,INT32_MIN,INT32_MAX));
    c.mode=FB_ALARM_MODE_RELATIVE_LOW; c.alarm_low=1;
    assert(ProductAlarmLogicAdapter_SetAlarm(a,0,0,&c,true));
    assert(!ProductAlarmLogicAdapter_SetpointFitsRange(a,0,INT32_MIN,true,INT32_MIN,INT32_MAX));
    /* Latest metadata wins before L05 receives the next configuration notification. */
    c.mode=FB_ALARM_MODE_RELATIVE_HIGH; c.alarm_high=10000;
    assert(ProductAlarmLogicAdapter_SetAlarm(a,0,0,&c,true));
    samples[0].configuration_maximum=200000; ++samples[0].configuration_revision;
    assert(ProductFbScheduleIntegration_SetChannelSv(0,195000)==PRODUCT_CHANNEL_SV_ALARM_RANGE);
    assert(ProductFbScheduleIntegration_SetChannelSv(0,190000)==PRODUCT_CHANNEL_SV_OK);
    /* Configuration restore leaves no accepted target until explicitly set again. */
    assert(ProductFbScheduleIntegration_Init(100)); assert(!State(0).configured);
}
static void TestAtomicConsumersAndAt(void)
{
    Consumer pid={0},ramp={0},bad={0}; ProductChannelSvConsumer p,r,b;
    ProductChannelSv before; ProductAlarmLogicAdapter_t *a;
    Init(); a=ProductFbScheduleIntegration_GetAlarmLogicAdapter();
    pid.id=1; ramp.id=2; ramp.current=75000; p=Binding(&pid); r=Binding(&ramp);
    assert(ProductFbScheduleIntegration_BindChannelSvConsumer(0,PRODUCT_CHANNEL_SV_PID,&p));
    assert(ProductFbScheduleIntegration_BindChannelSvConsumer(0,PRODUCT_CHANNEL_SV_RAMP,&r));
    assert(ProductFbScheduleIntegration_SetChannelSv(0,100000)==PRODUCT_CHANNEL_SV_OK);
    assert(order_count==4 && order[0]==1 && order[1]==2 && order[2]==11 && order[3]==12);
    assert(a->setpoint[0]==100000 && pid.target==100000 && ramp.target==100000 && ramp.current==75000);
    before=State(0); order_count=0;
    ramp.reject=true;
    assert(ProductFbScheduleIntegration_SetChannelSv(0,110000)==PRODUCT_CHANNEL_SV_CONSUMER_REJECTED);
    assert(pid.commits==1 && ramp.commits==1 && pid.target==100000 && a->setpoint[0]==100000);
    assert(State(0).revision==before.revision && State(0).target==100000);
    ramp.reject=false; pid.at=true; order_count=0;
    assert(ProductFbScheduleIntegration_SetChannelSv(0,110000)==PRODUCT_CHANNEL_SV_AT_BUSY);
    assert(order_count==0 && State(0).revision==before.revision);
    assert(ProductFbScheduleIntegration_SetChannelSv(0,100000)==PRODUCT_CHANNEL_SV_UNCHANGED);
    assert(!ProductFbScheduleIntegration_BindChannelSvConsumer(0,PRODUCT_CHANNEL_SV_PID,NULL));
    pid.at=false; pid.reenter=true; order_count=0;
    assert(ProductFbScheduleIntegration_SetChannelSv(0,110000)==PRODUCT_CHANNEL_SV_OK);
    assert(pid.target==110000 && ramp.target==110000 && State(0).revision==before.revision+1);
    pid.reenter=false;
    /* The integrated adapter's old entry also synchronizes both consumers. */
    assert(ProductAlarmLogicAdapter_SetSetpoint(a,0,120000));
    assert(State(0).target==120000 && pid.target==120000 && ramp.target==120000);
    assert(!ProductAlarmLogicAdapter_SetSetpoint(a,0,900000)); assert(pid.target==120000);
    assert(ProductFbScheduleIntegration_SetChannelSv(1,30000)==PRODUCT_CHANNEL_SV_OK);
    assert(State(0).target==120000 && pid.target==120000);
    /* Failed replacement preserves the previous binding and target. */
    bad.reject=true; b=Binding(&bad);
    assert(!ProductFbScheduleIntegration_BindChannelSvConsumer(0,PRODUCT_CHANNEL_SV_RAMP,&b));
    b=p; b.at_running=NULL;
    assert(!ProductFbScheduleIntegration_BindChannelSvConsumer(0,PRODUCT_CHANNEL_SV_PID,&b));
    assert(ProductFbScheduleIntegration_SetChannelSv(0,130000)==PRODUCT_CHANNEL_SV_OK);
    assert(ramp.target==130000 && bad.commits==0);
    assert(ProductFbScheduleIntegration_BindChannelSvConsumer(0,PRODUCT_CHANNEL_SV_RAMP,NULL));
    assert(ProductFbScheduleIntegration_SetChannelSv(0,140000)==PRODUCT_CHANNEL_SV_OK);
    assert(ramp.target==130000 && pid.target==140000);
}
static void TestHistoryPreservation(void)
{
    ProductAlarmLogicAdapter_t *a; ProductAlAlarm_t alarm; ProductAlLogic_t logic;
    FbAlarmConfig_t c; FbLogicConfig_t lc; ProductAlSource_t sources[FB_LOGIC_INPUT_COUNT]={{0}};
    FbMovingAverageOutput ma,after; ProductChannelSv before;
    ProductSensorMeasurementSnapshot_t snapshot;
    Init(); a=ProductFbScheduleIntegration_GetAlarmLogicAdapter();
    assert(ProductFbScheduleIntegration_SetChannelSv(0,100000)==PRODUCT_CHANNEL_SV_OK);
    c=Relative(FB_ALARM_MODE_RELATIVE_HIGH); c.delay_ms=40;
    c.options=FB_ALARM_OPTION_PEAK; c.reset_mode=FB_ALARM_RESET_MANUAL;
    assert(ProductAlarmLogicAdapter_SetAlarm(a,0,0,&c,true));
    FbLogic_DefaultConfig(&lc); lc.operator_type=FB_LOGIC_OPERATOR_OR; lc.on_delay_ms=100;
    sources[0]=(ProductAlSource_t){PRODUCT_AL_SOURCE_ALARM,0,0};
    assert(ProductAlarmLogicAdapter_SetLogic(a,0,&lc,sources,true));
    Tick(20); Tick(40);
    assert(a->alarms[0][0].fb.delay_elapsed_ms==20);
    assert(ProductFbScheduleIntegration_SetChannelSv(0,101000)==PRODUCT_CHANNEL_SV_OK);
    Tick(60); assert(a->alarms[0][0].output.alarm_latched && a->alarms[0][0].output.peak_valid);
    alarm=a->alarms[0][0]; logic=a->logic[0]; snapshot=samples[0];
    assert(ProductFbScheduleIntegration_GetCh0MovingAverage(&ma)); before=State(0);
    assert(ProductFbScheduleIntegration_SetChannelSv(0,105000)==PRODUCT_CHANNEL_SV_OK);
    assert(memcmp(&alarm,&a->alarms[0][0],sizeof(alarm))==0);
    assert(memcmp(&logic,&a->logic[0],sizeof(logic))==0);
    assert(memcmp(&snapshot,&samples[0],sizeof(snapshot))==0);
    assert(ProductFbScheduleIntegration_GetCh0MovingAverage(&after)); assert(memcmp(&ma,&after,sizeof(ma))==0);
    assert(State(0).revision==before.revision+1);
    before=State(0);
    assert(ProductFbScheduleIntegration_SetChannelSv(0,105000)==PRODUCT_CHANNEL_SV_UNCHANGED);
    assert(State(0).revision==before.revision);
    assert(ProductFbScheduleIntegration_SetChannelSv(0,130000)==PRODUCT_CHANNEL_SV_OK);
    Tick(80); assert(!a->alarms[0][0].output.condition_active && a->alarms[0][0].output.alarm_latched);
    /* A failed relative bound cannot clear a latch, peak or queued reset command. */
    assert(ProductAlarmLogicAdapter_Command(a,0,0,true,false)); alarm=a->alarms[0][0];
    assert(ProductFbScheduleIntegration_SetChannelSv(0,399999)==PRODUCT_CHANNEL_SV_ALARM_RANGE);
    assert(memcmp(&alarm,&a->alarms[0][0],sizeof(alarm))==0);
}
static void TestProcessedAlarmDomain(void)
{
    ProductAlarmLogicAdapter_t *a;
    ProductPvProcessingConfig cfg={0};
    FbAlarmConfig_t c=Relative(FB_ALARM_MODE_RELATIVE_HIGH);
    Init(); a=ProductFbScheduleIntegration_GetAlarmLogicAdapter();
    samples[1].configuration_maximum=200000;
    cfg.source[0]=1; cfg.maximum_age_ms=200; cfg.maximum_skew_ms=20;
    assert(ProductFbScheduleIntegration_ConfigureCh0PvProcessing(&cfg));
    assert(ProductFbScheduleIntegration_SetCh0AlarmPvSource(PRODUCT_CH0_ALARM_PV_PROCESSED));
    assert(ProductAlarmLogicAdapter_SetAlarm(a,0,0,&c,true));
    assert(ProductFbScheduleIntegration_SetChannelSv(0,195000)==PRODUCT_CHANNEL_SV_ALARM_RANGE);
    assert(ProductFbScheduleIntegration_SetChannelSv(0,190000)==PRODUCT_CHANNEL_SV_OK);
    samples[1].configuration_maximum=150000; ++samples[1].configuration_revision;
    assert(ProductFbScheduleIntegration_SetChannelSv(0,145000)==PRODUCT_CHANNEL_SV_ALARM_RANGE);
    assert(ProductFbScheduleIntegration_SetChannelSv(0,140000)==PRODUCT_CHANNEL_SV_OK);
}
static void TestStartupGuard(void)
{
    ProductAlarmLogicAdapter_t *a;
    FbAlarmConfig_t c=Relative(FB_ALARM_MODE_RELATIVE_HIGH);
    Init(); a=ProductFbScheduleIntegration_GetAlarmLogicAdapter();
    assert(ProductAlarmLogicAdapter_SetAlarm(a,0,0,&c,true));
    Tick(20); assert(a->alarm_blocked[0][0] && !a->alarms[0][0].output.alarm_active);
    assert(ProductFbScheduleIntegration_SetChannelSv(0,100000)==PRODUCT_CHANNEL_SV_OK);
    Tick(40); assert(!a->alarm_blocked[0][0] && a->alarms[0][0].output.alarm_active);
}
typedef struct { FbSetpointRampState state; FbSetpointRampInput input; } Ramp;
static ProductChannelSvResult ValidateRamp(void *context,int32_t sv)
{ Ramp *r=context; (void)sv; return r->state.initialized ? PRODUCT_CHANNEL_SV_OK : PRODUCT_CHANNEL_SV_CONSUMER_REJECTED; }
static void CommitRamp(void *context,int32_t sv)
{ ((Ramp *)context)->input.target=(float)sv; }
static void TestActualRampExtension(void)
{
    Ramp ramp={0}; FbSetpointRampConfig cfg={10000,10000}; FbSetpointRampOutput out;
    ProductChannelSvConsumer binding={ValidateRamp,CommitRamp,NULL,&ramp};
    Init(); assert(FbSetpointRamp_Init(&ramp.state,75000)==FB_SETPOINT_RAMP_OK);
    assert(ProductFbScheduleIntegration_BindChannelSvConsumer(0,PRODUCT_CHANNEL_SV_RAMP,&binding));
    assert(ProductFbScheduleIntegration_SetChannelSv(0,100000)==PRODUCT_CHANNEL_SV_OK);
    ramp.input.enable=true; ramp.input.dt_seconds=0.2F;
    assert(FbSetpointRamp_Execute(&cfg,&ramp.input,&ramp.state,&out)==FB_SETPOINT_RAMP_OK);
    assert(out.value==77000 && !out.reached);
    assert(ProductFbScheduleIntegration_SetChannelSv(0,120000)==PRODUCT_CHANNEL_SV_OK);
    assert(ramp.input.target==120000 && ramp.state.value==77000); /* no Init/Reset */
    assert(FbSetpointRamp_Execute(&cfg,&ramp.input,&ramp.state,&out)==FB_SETPOINT_RAMP_OK);
    assert(out.value==79000 && !out.reached);
    samples[0].configuration_maximum=50000; ++samples[0].configuration_revision;
    assert(!ProductFbScheduleIntegration_BindChannelSvConsumer(0,PRODUCT_CHANNEL_SV_RAMP,&binding));
    assert(ramp.state.value==79000 && ramp.input.target==120000);
}
int main(void)
{
    assert(ProductFbScheduleIntegration_SetChannelSv(0,0)==PRODUCT_CHANNEL_SV_NOT_INITIALIZED);
    TestValidation(); TestAtomicConsumersAndAt(); TestHistoryPreservation(); TestStartupGuard(); TestProcessedAlarmDomain(); TestActualRampExtension();
    puts("PASS: unified SV validation, all-or-nothing delivery, AT rejection, legacy routing and retained histories");
    return 0;
}
