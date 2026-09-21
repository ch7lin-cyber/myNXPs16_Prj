#include <assert.h>

#include "AlarmConfigurationEventConsumer.h"
#include "EventService.h"
#include "product_application.h"
#include "product_modbus_register_adapter.h"
#include "product_temperature_input_types.h"

int main(void)
{
    ModbusSlaveRegisterInterface_t interface;
    AlarmConfigurationRange_t range;
    const uint16_t thermocouple_k[2] =
        {PRODUCT_SENSOR_TYPE_THERMOCOUPLE, PRODUCT_TC_LINEARIZATION_K};

    assert(EventService_Initialize(EVENT_ACK_SERIAL_REQUIRED_DEFAULT));
    assert(ProductApplication_Init());
    ProductModbusRegisterAdapter_GetInterface(&interface);

    assert(interface.write_multiple_registers(
               interface.context, PRODUCT_MODBUS_SENSOR_TYPE_ADDRESS,
               thermocouple_k, 2U) == MODBUS_EXCEPTION_NONE);
    assert(interface.write_single_register(
               interface.context, PRODUCT_MODBUS_APPLY_KEY_ADDRESS,
               PRODUCT_MODBUS_APPLY_KEY_VALUE) == MODBUS_EXCEPTION_NONE);
    assert(EventService_IsTemperatureInputConfigurationChangedPending(0U));

    ProductApplication_Process();
    assert(!EventService_IsTemperatureInputConfigurationChangedPending(0U));
    assert(AlarmConfigurationEventConsumer_GetRange(0U, &range));
    assert(range.input_enabled);
    assert(range.setpoint_minimum == -270.0F);
    assert(range.setpoint_maximum == 1372.0F);
    assert(range.configuration_revision == 1U);

    assert(interface.write_single_register(
               interface.context, PRODUCT_MODBUS_SENSOR_TYPE_ADDRESS,
               PRODUCT_SENSOR_TYPE_OFF) == MODBUS_EXCEPTION_NONE);
    assert(interface.write_single_register(
               interface.context, PRODUCT_MODBUS_APPLY_KEY_ADDRESS,
               PRODUCT_MODBUS_APPLY_KEY_VALUE) == MODBUS_EXCEPTION_NONE);
    ProductApplication_Process();
    assert(AlarmConfigurationEventConsumer_GetRange(0U, &range));
    assert(!range.input_enabled);
    assert(range.configuration_revision == 2U);
    return 0;
}
