/* Reuse the SDK stubs and real ADC driver boundary tests. */
#define main RunInjectionBoundaryTests
#include "test_product_internal_adc_temperature_injection.c"
#undef main
#include "product_safety_test.h"
int main(void)
{
    uint16_t command[4]={1,1,8499,0x5AFE}, data[8];
    assert(RunInjectionBoundaryTests()==0);
    ProductSafetyTest_SetAddressed(false);
    assert(ProductSafetyTest_Write(0x4960,command,4)==3);
    ProductSafetyTest_SetAddressed(true);
    assert(ProductSafetyTest_Write(0x4960,command,3)==2);
    command[3]=0;assert(ProductSafetyTest_Write(0x4960,command,4)==3);command[3]=0x5AFE;
    assert(ProductSafetyTest_Write(0x4960,command,4)==0);
    assert(ProductSafetyTest_Write(0x4960,command,4)==6);
    ProductSafetyTest_Process();
    assert(ProductSafetyTest_Read(0x4968,8,data)==0 && data[0]==2 && data[3]==8499);
    assert(ProductInternalAdcDriver_RequestMcuTemperature());ProductInternalAdcDriver_Process();
    assert(Sample().overtemperature_count==0);
    command[2]=8500;assert(ProductSafetyTest_Write(0x4960,command,4)==0);
    ProductSafetyTest_Process();
    for (unsigned i=1;i<=3;i++) {
        assert(ProductInternalAdcDriver_RequestMcuTemperature());ProductInternalAdcDriver_Process();
        assert(ProductSafetyTest_Read(0x4968,8,data)==0 && data[4]==i && data[5]==(i==3));
    }
    command[1]=0;command[2]=8499;
    assert(ProductSafetyTest_Write(0x4960,command,4)==0);ProductSafetyTest_Process();
    assert(ProductInternalAdcDriver_RequestMcuTemperature());ProductInternalAdcDriver_Process();
    assert(ProductSafetyTest_Read(0x4964,4,data)==0 && data[0]==0 && data[1]==8500);
    assert(ProductSafetyTest_Read(0x4968,8,data)==0 && data[4]==3 && data[5]==1);
    command[1]=1;
    assert(ProductSafetyTest_Write(0x4960,command,4)==0);ProductSafetyTest_Process();
    assert(ProductInternalAdcDriver_RequestMcuTemperature());ProductInternalAdcDriver_Process();
    assert(ProductSafetyTest_Read(0x4964,4,data)==0 && data[0]==1 && data[1]==8499);
    assert(ProductSafetyTest_Read(0x4967,2,data)==2);
    command[0]=0;command[1]=0;command[2]=0;
    assert(ProductSafetyTest_Write(0x4960,command,4)==0);ProductSafetyTest_Process();
    assert(ProductSafetyTest_Read(0x4968,8,data)==0 && data[1]==0 && data[4]==0);
    assert(ProductSafetyTest_Read(0x496F,2,data)==2);
    return 0;
}
