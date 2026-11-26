#define main legacy_conversion_main
#include "test_product_sensor_measurement_service.c"
#undef main
#include "ProductSensorMeasurementConfig.h"
#include <stdio.h>
static ProductSensorMeasurementSnapshot_t Snap(uint8_t ch)
{ ProductSensorMeasurementSnapshot_t s; assert(ProductSensorMeasurementService_GetSnapshot(ch,&s)); return s; }
static void Setup(void)
{
    g_product_configuration.sensorType=PRODUCT_SENSOR_TYPE_VOLTAGE_0_50MV;
    g_product_configuration.cjcSource=EVENT_CJC_SOURCE_DISABLED;
    g_product_configuration.cjcOffsetCentiC=0;
    g_product_configuration.filterTimeConstantSeconds=1.0F;
    g_sample=(AnalogInputSample_t){0};
    g_sample.valid=true; g_sample.sequence=10; g_sample.microvolts=25000;
    InitializeHealthy();
}
static void Next(uint8_t ch,uint32_t t)
{ ++g_sample.sequence; assert(ProductSensorMeasurementService_ProcessAt(ch,t)); }
static void Recover(uint8_t ch,uint32_t t)
{
    Next(ch,t); assert(!Snap(ch).valid && Snap(ch).recovery_good_samples==1);
    Next(ch,t+20); assert(!Snap(ch).valid && Snap(ch).recovery_good_samples==2);
    Next(ch,t+40); assert(Snap(ch).valid && Snap(ch).recovery_good_samples==3);
}
static void TestStartupAndDuplicates(void)
{
    Setup(); assert(ProductSensorMeasurementService_ProcessAt(0,100));
    assert(!Snap(0).valid && Snap(0).recovery_good_samples==1);
    for(unsigned i=0;i<20;++i) assert(ProductSensorMeasurementService_ProcessAt(0,101+i));
    assert(Snap(0).recovery_good_samples==1 && g_monitor.inputError==145);
    Next(0,121); assert(Snap(0).recovery_good_samples==2);
    Next(0,141); assert(Snap(0).valid && g_monitor.inputError==61);
    assert(Snap(0).quality_flags==0 && g_monitor.filteredProcessValue==0.0F);
}
static void TestStaleAndFilterReset(void)
{
    Setup(); Recover(0,100);
    assert(ProductSensorMeasurementService_ProcessAt(0,639)); assert(Snap(0).valid);
    assert(ProductSensorMeasurementService_ProcessAt(0,640));
    assert(!Snap(0).valid && Snap(0).quality_flags==PRODUCT_SENSOR_QUALITY_STALE);
    assert(g_monitor.inputError==144 && g_monitor.filteredProcessValue==0.0F);
    uint32_t revision=Snap(0).quality_revision;
    assert(ProductSensorMeasurementService_ProcessAt(0,650));
    assert(Snap(0).recovery_good_samples==0 && Snap(0).quality_revision==revision);
    g_sample.microvolts=50000; Recover(0,660);
    assert(Snap(0).engineering_value==32000 && g_monitor.filteredProcessValue==32000.0F);
}
static void TestBrokenRecoveryAndQuarantine(void)
{
    Setup(); Recover(0,100);
    ProductSensorMeasurementService_SetSourceHealth(0,false,true);
    assert(ProductSensorMeasurementService_ProcessAt(0,150));
    assert(Snap(0).quality_flags==PRODUCT_SENSOR_QUALITY_SOURCE_FAULT);
    ProductSensorMeasurementService_SetSourceHealth(0,true,false);
    assert(ProductSensorMeasurementService_ProcessAt(0,151));
    assert(!Snap(0).valid && Snap(0).recovery_good_samples==0);
    Next(0,160); assert(Snap(0).recovery_good_samples==1);
    g_sample.valid=false; assert(ProductSensorMeasurementService_ProcessAt(0,170));
    assert(!Snap(0).valid && Snap(0).recovery_good_samples==0);
    g_sample.valid=true; assert(ProductSensorMeasurementService_ProcessAt(0,180));
    assert(Snap(0).recovery_good_samples==0);
    Recover(0,200);
    ProductSensorMeasurementService_SetSourceHealth(0,false,true);
    assert(ProductSensorMeasurementService_ProcessAt(0,250));
    assert(ProductSensorMeasurementService_ProcessAt(0,740));
    assert(Snap(0).quality_flags==(PRODUCT_SENSOR_QUALITY_SOURCE_FAULT|PRODUCT_SENSOR_QUALITY_STALE));
    uint32_t rev=Snap(0).quality_revision;
    assert(ProductSensorMeasurementService_ProcessAt(0,750));
    assert(Snap(0).quality_revision==rev); /* no stale/fault flag oscillation */
}
static void TestRawFaultBeforeMedian(void)
{
    Setup(); Recover(0,100); g_sample.microvolts=60000; Next(0,160);
    assert(!Snap(0).valid && Snap(0).quality_flags==PRODUCT_SENSOR_QUALITY_CONVERSION);
    assert(g_monitor.inputError==143 && g_monitor.filteredProcessValue==0.0F);
    g_sample.microvolts=50000; Recover(0,180); assert(g_monitor.filteredProcessValue==32000.0F);
}
static void TestChannelAndClock(void)
{
    Setup(); Recover(0,100); Recover(1,160);
    ProductSensorMeasurementService_SetSourceHealth(0,true,true);
    assert(ProductSensorMeasurementService_ProcessAt(0,220));
    assert(!Snap(0).valid && Snap(1).valid);
    Setup(); Recover(0,UINT32_MAX-100U);
    assert(ProductSensorMeasurementService_ProcessAt(0,438)); assert(Snap(0).valid);
    assert(ProductSensorMeasurementService_ProcessAt(0,439)); assert(!Snap(0).valid);
    Setup(); Recover(0,100); g_sample.microvolts=50000; Next(0,1000);
    assert(!Snap(0).valid && Snap(0).recovery_good_samples==1);
    Next(0,1020); Next(0,1040); assert(g_monitor.filteredProcessValue==32000.0F);
}
static void TestUnknownAndConfiguration(void)
{
    Setup(); ProductSensorMeasurementService_Initialize();
    assert(ProductSensorMeasurementService_ProcessAt(0,10));
    assert(!Snap(0).valid && Snap(0).quality_flags==PRODUCT_SENSOR_QUALITY_SOURCE_UNKNOWN);
    ProductSensorMeasurementService_SetSourceHealth(0,true,false); Recover(0,100);
    g_product_configuration.sensorType=PRODUCT_SENSOR_TYPE_OFF;
    assert(ProductSensorMeasurementService_ProcessAt(0,150));
    assert(Snap(0).quality_flags==PRODUCT_SENSOR_QUALITY_DISABLED);
    g_product_configuration.sensorType=PRODUCT_SENSOR_TYPE_VOLTAGE_0_50MV;
    g_sample.microvolts=50000;
    assert(ProductSensorMeasurementService_ProcessAt(0,160));
    assert(!Snap(0).valid && Snap(0).recovery_good_samples==0);
    Recover(0,180); assert(Snap(0).valid && g_monitor.filteredProcessValue==32000.0F);
}
static void TestCjcRecovery(void)
{
    Setup();
    g_product_configuration.sensorType=PRODUCT_SENSOR_TYPE_TC_K;
    g_product_configuration.cjcSource=EVENT_CJC_SOURCE_INTERNAL_TMP20;
    g_cjc_valid[0]=true; g_cjc_temperature_centi_c[0]=2500;
    g_sample.microvolts=10153;
    Recover(0,100);
    g_cjc_valid[0]=false; Next(0,160);
    assert(!Snap(0).valid && g_monitor.inputError==9U);
    g_cjc_valid[0]=true; Next(0,180);
    assert(Snap(0).recovery_good_samples==1U && !Snap(0).valid);
    g_cjc_valid[0]=false; Next(0,200);
    assert(Snap(0).recovery_good_samples==0U);
    g_cjc_valid[0]=true; Recover(0,220);
    assert(g_monitor.inputError==61U);
}
static void TestConfigurationGeneration(void)
{
    Setup(); g_product_configuration.sensorType=PRODUCT_SENSOR_TYPE_TC_K;
    g_product_configuration.cjcSource=EVENT_CJC_SOURCE_DISABLED;
    g_sample.microvolts=10153; Recover(0,100);
    uint32_t rev=Snap(0).configuration_revision;
    assert(Snap(0).configuration_range_valid && Snap(0).configuration_maximum==1300000);
    /* A->B->A Apply between observations: final values equal, Active revision differs. */
    g_configuration_revision+=2;
    assert(ProductSensorMeasurementService_ProcessAt(0,150));
    assert(Snap(0).configuration_revision==rev+1 && !Snap(0).valid &&
           Snap(0).recovery_good_samples==0);
    Recover(0,160); rev=Snap(0).configuration_revision;
    g_product_configuration.filterTimeConstantSeconds=2.0F;
    assert(ProductSensorMeasurementService_ProcessAt(0,210));
    assert(Snap(0).configuration_revision==rev+1 && !Snap(0).valid);
    rev=Snap(0).configuration_revision;
    assert(ProductSensorMeasurementService_ProcessAt(0,211));
    assert(Snap(0).configuration_revision==rev);
    g_configuration_revision=UINT16_MAX; Next(0,220);
    rev=Snap(0).configuration_revision; g_configuration_revision=0; Next(0,240);
    assert(Snap(0).configuration_revision==rev+1);
    g_product_configuration.sensorType=PRODUCT_SENSOR_TYPE_VOLTAGE_0_10V;
    Next(0,260); assert(!Snap(0).configuration_range_valid);
}
int main(void)
{
    TestStartupAndDuplicates(); TestStaleAndFilterReset(); TestBrokenRecoveryAndQuarantine();
    TestRawFaultBeforeMedian(); TestChannelAndClock(); TestUnknownAndConfiguration(); TestCjcRecovery(); TestConfigurationGeneration();
    puts("PASS: eight sensor recovery/configuration groups"); return 0;
}
