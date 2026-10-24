/* Reuse the full register adapter regression and HAL PWM mock driver. */
#define main RunRegisterRegression
#include "test_product_modbus_register_adapter.c"
#undef main
#include "product_safety_test.h"

/* This suite exercises the PWM read-only branch, not the injection module. */
void ProductSafetyTest_SetAddressed(bool addressed) { (void)addressed; }
uint8_t ProductSafetyTest_Read(uint16_t a, uint16_t c, uint16_t *v)
{ (void)a; (void)c; (void)v; return 2U; }
uint8_t ProductSafetyTest_Write(uint16_t a, const uint16_t *v, uint16_t c)
{ (void)a; (void)c; (void)v; return 2U; }
static bool inhibit;
static bool TestInhibit(uint8_t channel, void *context)
{ (void)channel; (void)context; return inhibit; }
int main(void)
{
    ModbusSlaveRegisterInterface_t api;
    uint16_t data[32];
    assert(RunRegisterRegression() == 0);
    ProductModbusRegisterAdapter_GetInterface(&api);
    assert(PwmOutputService_Initialize(4U, TestInhibit, NULL) == PWM_OUTPUT_STATUS_OK);
    for (uint8_t ch=0U; ch<4U; ch++)
    {
        assert(PwmOutputService_SetPeriod(ch, 100U, PWM_OUTPUT_PERIOD_UPDATE_IMMEDIATE)==PWM_OUTPUT_STATUS_OK);
        assert(PwmOutputService_SetCommand(ch, 500U)==PWM_OUTPUT_STATUS_OK);
    }
    assert(api.read_holding_registers(api.context,0x4980U,32U,data)==MODBUS_EXCEPTION_NONE);
    for (unsigned ch=0U; ch<4U; ch++)
    {
        unsigned i=ch*8U;
        assert(data[i]==500U && data[i+1U]==500U && data[i+2U]==0U);
        assert(data[i+3U]==0U && data[i+4U]==100U && data[i+5U]==0U);
        assert(data[i+6U]==1U && data[i+7U]==1U && g_pwmDriver[ch].dutyPermille==500U);
    }
    inhibit=true;
    for (uint8_t ch=0U;ch<4U;ch++)
        assert(PwmOutputService_SetCommand(ch,700U)==PWM_OUTPUT_STATUS_OK);
    assert(api.read_holding_registers(api.context,0x4980U,32U,data)==MODBUS_EXCEPTION_NONE);
    for (unsigned ch=0U;ch<4U;ch++)
        assert(data[ch*8U]==700U && data[ch*8U+1U]==0U && data[ch*8U+2U]==1U &&
               g_pwmDriver[ch].dutyPermille==0U);
    inhibit=false;
    for (uint8_t ch=0U;ch<4U;ch++)
        assert(PwmOutputService_RefreshSafety(ch)==PWM_OUTPUT_STATUS_OK);
    assert(api.read_holding_registers(api.context,0x4980U,32U,data)==MODBUS_EXCEPTION_NONE);
    for (unsigned ch=0U;ch<4U;ch++)
        assert(data[ch*8U+1U]==700U && g_pwmDriver[ch].dutyPermille==700U);
    assert(api.read_holding_registers(api.context,0x499FU,2U,data)==MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS);
    assert(api.write_single_register(api.context,0x4980U,1U)==MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS);
    assert(api.write_multiple_registers(api.context,0x4980U,data,4U)==MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS);
    return 0;
}
