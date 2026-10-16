/* Exercise the actual Modbus initialization and inspect its selected address. */
#include <assert.h>
#include "fsl_gpio.h"
#include "l03_product_modbus.c"

GPIO_Type g_mock_gpio;
static uint8_t raw;
static ModbusSerialPortConfiguration_t initialized[2];
uint32_t GPIO_PinRead(GPIO_Type *base, uint32_t port, uint32_t pin)
{
    assert(base == &g_mock_gpio);
    if ((port == 1U) && (pin == 27U)) return raw & 1U;
    assert(port == 0U && pin == 4U);
    return (raw >> 1U) & 1U;
}
SerialServiceStatus_t SerialConfigurationApplyService_InitializePort(
    HalSerialPort_t port, const ModbusSerialPortConfiguration_t *configuration,
    uint32_t timeout, SerialServiceEventCallback_t callback, void *context)
{
    (void)timeout; (void)callback; (void)context;
    initialized[port] = *configuration;
    return SERIAL_SERVICE_STATUS_OK;
}
uint32_t SerialConfiguration_GetCharacterTimeUs(const SerialConfiguration_t *configuration)
{
    (void)configuration;
    return 87U;
}
void ProductModbusRegisterAdapter_GetInterface(ModbusSlaveRegisterInterface_t *iface)
{
    (void)memset(iface, 0, sizeof(*iface));
}
BspRs485Status_t BspRs485_SetDirectionDelayUs(BspRs485Port_t port, uint32_t pre, uint32_t post)
{
    (void)port; (void)pre; (void)post;
    return BSP_RS485_STATUS_OK;
}
BspRs485Status_t BspRs485_SetReceiveTimeoutUs(BspRs485Port_t port, uint32_t timeout)
{
    (void)port; (void)timeout;
    return BSP_RS485_STATUS_OK;
}
BspRs485Status_t BspRs485_SetRtuTimingUs(BspRs485Port_t port, uint32_t t15, uint32_t t35)
{
    (void)port; (void)t15; (void)t35;
    return BSP_RS485_STATUS_OK;
}
BspRs485Status_t BspRs485_StartReceive(BspRs485Port_t port, uint8_t *buffer, size_t capacity)
{
    assert(port == BSP_RS485_PORT_EXTERNAL_SLAVE && buffer != NULL && capacity > 0U);
    return BSP_RS485_STATUS_OK;
}
int main(void)
{
    for (raw = 0U; raw < 4U; ++raw)
    {
        ProductRotarySwitchDriver_Initialize();
        assert(L03_ProductModbus_Init());
        assert(initialized[0].unit_id == ((raw & 1U) + 1U));
        assert(s_modbus.slave.unit_address == initialized[0].unit_id);
        assert(initialized[1].unit_id == PRODUCT_MODBUS_FC1_LOCAL_UNIT_ADDRESS);
        /* PIN0 changes during operation must not change the active station. */
        raw ^= 1U;
        ProductRotarySwitchDriver_Process();
        assert(s_modbus.slave.unit_address == initialized[0].unit_id);
        raw ^= 1U;
    }
    return 0;
}
