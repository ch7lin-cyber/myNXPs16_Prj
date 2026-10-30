#include "product_fb_schedule_integration.h"
#include "product_sensor_measurement_service.h"
#include "product_temperature_input_types.h"
#include "DigitalInputService.h"
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
static ProductSensorMeasurementSnapshot_t sample;
static bool available=true;
bool ProductSensorMeasurementService_GetSnapshot(uint8_t channel,ProductSensorMeasurementSnapshot_t *out)
{
    if (channel!=0 || !available || !out) return false;
    *out=sample; return true;
}
bool DigitalInputService_GetSnapshot(DigitalInputSnapshot_t *out)
{ (void)out; return false; }
static ProductCh0PidTestDiagnostics Read(void)
{
    ProductCh0PidTestDiagnostics d;
    assert(ProductFbScheduleIntegration_GetCh0PidTest(&d));
    assert(d.fixed_mv_test_backend); return d;
}
static void Fresh(uint32_t now)
{
    sample.processed_timestamp_ms=now; ++sample.sample_sequence;
    ProductFbScheduleIntegration_Process(now);
}
int main(void)
{
    ProductCh0PidTestDiagnostics d;
    FbPid_t fb={0};
    FbPidConfig_t config={FB_PID_MODE_AUTO_TUNE,FB_PID_MODE_PID,FB_PID_ACTION_HEAT,100,0};
    assert(FbPid_Init(&fb,&config)==FB_PID_STATUS_INVALID_CONFIG);
    config.mode=FB_PID_MODE_PID;
    assert(FbPid_Init(&fb,&config)==FB_PID_STATUS_OK);
    assert(FbPid_SetParameter(&fb,FB_PID_PARAMETER_PB,300)==FB_PID_STATUS_INVALID_CONFIG);
    assert(FbPid_SetTestMv(&fb,1001)==FB_PID_STATUS_INVALID_ARGUMENT);
    FbPid_Deinit(&fb);
    assert(!ProductFbScheduleIntegration_ConfigureCh0PidTest(true,30000,100));
    assert(ProductFbScheduleIntegration_Init(0));
    sample.sensor_type=PRODUCT_SENSOR_TYPE_TC_K; sample.valid=true;
    sample.processed_timestamp_valid=true; sample.engineering_value=25123;
    Fresh(20);
    assert(Read().controller.output.status==FB_PID_STATUS_DISABLED);
    assert(ProductFbScheduleIntegration_ConfigureCh0PidTest(true,30000,100));
    assert(!Read().controller.output_valid); /* configuration immediately clears old MV */
    Fresh(40); d=Read();
    assert(d.input.pv==25123 && d.input.sv==30000 && d.input.valid);
    assert(d.controller.output.mv[0]==100 && d.controller.output.mv_count==1 && d.controller.output_valid);
    assert(d.controller.callback_count==2 && d.controller.last_elapsed_ms==20);
    ProductFbScheduleIntegration_Process(41);
    assert(Read().controller.callback_count==2); /* one shared callback, no duplicate task */
    assert(!ProductFbScheduleIntegration_ConfigureCh0PidTest(true,30000,1001));
    assert(Read().controller.output.mv[0]==100);
    assert(ProductFbScheduleIntegration_ConfigureCh0PidTest(true,31000,250));
    assert(!Read().controller.output_valid);
    Fresh(60); assert(Read().controller.output.mv[0]==250);
    assert(ProductFbScheduleIntegration_ResetCh0PidTest());
    assert(!Read().controller.output_valid);
    Fresh(80); assert(Read().controller.output.status==FB_PID_STATUS_RESET);
    Fresh(100); assert(Read().controller.output.mv[0]==250);
    sample.valid=false; Fresh(120); assert(!Read().controller.output_valid);
    sample.valid=true; Fresh(140); assert(Read().controller.output_valid);
    sample.input_error=1; Fresh(160); assert(!Read().controller.output_valid);
    sample.input_error=0; sample.conversion_flags=1; Fresh(180); assert(!Read().controller.output_valid);
    sample.conversion_flags=0; sample.sensor_type=65535; Fresh(200); assert(!Read().controller.output_valid);
    sample.sensor_type=PRODUCT_SENSOR_TYPE_TC_K; sample.processed_timestamp_valid=false;
    Fresh(220); assert(!Read().controller.output_valid);
    sample.processed_timestamp_valid=true; Fresh(240);
    ProductFbScheduleIntegration_Process(440); assert(Read().controller.output_valid); /* age == 200 */
    ProductFbScheduleIntegration_Process(460); assert(!Read().controller.output_valid);
    Fresh(480); assert(Read().controller.output_valid);
    available=false; ProductFbScheduleIntegration_Process(500); assert(!Read().controller.output_valid);
    available=true; Fresh(520); assert(Read().controller.output_valid);
    assert(ProductFbScheduleIntegration_ConfigureCh0PidTest(false,31000,250));
    assert(!Read().controller.output_valid);
    Fresh(540); assert(Read().controller.output.status==FB_PID_STATUS_DISABLED);
    assert(ProductFbScheduleIntegration_ConfigureCh0PidTest(true,31000,0));
    Fresh(560); assert(Read().controller.output_valid && Read().controller.output.mv[0]==0);
    assert(ProductFbScheduleIntegration_Init(600));
    Fresh(620); assert(!Read().controller.output_valid && !Read().input.enable);
    /* Unsigned timestamp rollover remains a small age. */
    assert(ProductFbScheduleIntegration_Init(0xfffffff0U));
    assert(ProductFbScheduleIntegration_ConfigureCh0PidTest(true,30000,1000));
    Fresh(4U); assert(Read().controller.output_valid && Read().controller.output.mv[0]==1000);
    puts("PASS: CH0 snapshot -> shared PID callback -> L05 adapter -> L04 fixed MV; guards, reset, timing and recovery");
    return 0;
}
