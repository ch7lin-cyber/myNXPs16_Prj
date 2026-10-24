/* Keep the original MovingAverage integration assertions as regression. */
#define main previous_integration_main
#include "test_product_fb_schedule_integration.c"
#undef main
#include "product_temperature_input_types.h"

int main(void)
{
    ProductPvProcessingOutput out;
    ProductPvProcessingConfig c={0};
    const FbLinearizationPoint points[]={{0,1000},{100000,101000}};
    assert(previous_integration_main()==0);
    assert(ProductFbScheduleIntegration_Init(0));
    g_snapshot.sensor_type=PRODUCT_SENSOR_TYPE_TC_K;
    g_snapshot.valid=true; g_snapshot_available=true;
    g_snapshot.processed_timestamp_valid=true;
    g_snapshot.processed_timestamp_ms=10;
    g_snapshot.engineering_value=25000; g_snapshot.sample_sequence=1;
    ProductFbScheduleIntegration_Process(19);
    assert(ProductFbScheduleIntegration_GetCh0ProcessedPv(&out) && !out.valid);
    ProductFbScheduleIntegration_Process(20);
    ProductFbScheduleIntegration_GetCh0ProcessedPv(&out);
    assert(out.valid && out.pv==25000 && out.timestamp_ms==20);
    assert(out.sources[0].raw.timestamp_ms==10);
    ProductFbScheduleIntegration_Process(220);
    ProductFbScheduleIntegration_GetCh0ProcessedPv(&out);
    assert(!out.valid && out.pv==0 && out.sources[0].status==PRODUCT_PV_SOURCE_STALE);
    g_snapshot.processed_timestamp_ms=230; ++g_snapshot.sample_sequence;
    ProductFbScheduleIntegration_Process(240);
    ProductFbScheduleIntegration_GetCh0ProcessedPv(&out); assert(out.valid);
    c.source[0]=0; c.source[1]=0; c.maximum_age_ms=200; c.maximum_skew_ms=20;
    c.linearization[0].mode=FB_INPUT_LINEARIZATION_VALUE;
    c.linearization[0].parameters.curve=(FbLinearizationConfig){points,2,FB_LINEARIZATION_CLAMP};
    assert(ProductFbScheduleIntegration_ConfigureCh0PvProcessing(&c));
    ProductFbScheduleIntegration_Process(260);
    ProductFbScheduleIntegration_GetCh0ProcessedPv(&out); assert(out.valid && out.pv==26000);
    assert(g_snapshot.engineering_value==25000); /* raw Sensor Snapshot untouched */
    c.switch_enabled=true;
    c.switch_config=(FbInputSwitchOverConfig){-200000,3000000,0,100000,FB_SWITCH_USE_OTHER};
    assert(ProductFbScheduleIntegration_ConfigureCh0PvProcessing(&c));
    ProductFbScheduleIntegration_Process(280);
    ProductFbScheduleIntegration_GetCh0ProcessedPv(&out);
    assert(out.valid && out.selected==FB_SWITCH_BOTH && out.pv==25500);
    c.source[0]=UINT16_MAX;
    assert(!ProductFbScheduleIntegration_ConfigureCh0PvProcessing(&c));
    g_snapshot.sensor_type=PRODUCT_SENSOR_TYPE_VOLTAGE_0_10V;
    ProductFbScheduleIntegration_Process(300);
    ProductFbScheduleIntegration_GetCh0ProcessedPv(&out); assert(!out.valid);
    g_snapshot.sensor_type=PRODUCT_SENSOR_TYPE_TC_K; g_snapshot.input_error=142;
    ProductFbScheduleIntegration_Process(320);
    ProductFbScheduleIntegration_GetCh0ProcessedPv(&out); assert(!out.valid);
    g_snapshot.input_error=0; g_snapshot.processed_timestamp_valid=false;
    ProductFbScheduleIntegration_Process(340);
    ProductFbScheduleIntegration_GetCh0ProcessedPv(&out); assert(!out.valid);
    g_snapshot.processed_timestamp_valid=true;
    g_snapshot.processed_timestamp_ms=350;
    ProductFbScheduleIntegration_Process(360);
    ProductFbScheduleIntegration_GetCh0ProcessedPv(&out); assert(out.valid);
    assert(ProductFbScheduleIntegration_Init(UINT32_MAX-10));
    g_snapshot.processed_timestamp_ms=UINT32_MAX-5;
    ProductFbScheduleIntegration_Process(9);
    ProductFbScheduleIntegration_GetCh0ProcessedPv(&out);
    assert(out.valid && out.pv==25000 && out.sources[0].age_ms==15);
    assert(!ProductFbScheduleIntegration_GetCh0ProcessedPv(NULL));
    puts("PASS: scheduled Sensor Snapshot -> input curve -> Switch Over -> readonly CH0 PV");
    return 0;
}
