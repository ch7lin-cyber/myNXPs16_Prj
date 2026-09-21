#include <assert.h>
#include <string.h>

#include "AlarmConfigurationEventConsumer.h"
#include "EventService.h"
#include "HalPwm.h"
#include "HalNvm.h"
#include "PwmOutputService.h"
#include "SafetyConfigurationEventConsumer.h"
#include "product_application.h"
#include "product_modbus_register_adapter.h"
#include "product_temperature_input_types.h"

static uint16_t g_hardware_duty_permille;
#define TEST_NVM_SLOT_SIZE (32768U)
static uint8_t g_nvm_storage[HAL_NVM_SLOT_COUNT][TEST_NVM_SLOT_SIZE];

static HalPwmStatus_t MockPwmInitialize(void *context)
{
    (void)context;
    g_hardware_duty_permille = 0U;
    return HAL_PWM_STATUS_OK;
}

static HalPwmStatus_t MockPwmSetDuty(void *context, uint16_t duty_permille)
{
    (void)context;
    g_hardware_duty_permille = duty_permille;
    return HAL_PWM_STATUS_OK;
}

static HalNvmStatus_t MockNvmInitialize(void *context)
{
    (void)context;
    return HAL_NVM_STATUS_OK;
}

static HalNvmStatus_t MockNvmRead(void *context, uint8_t slot,
                                  uint32_t offset, uint8_t *data,
                                  uint32_t length)
{
    (void)context;
    if ((offset + length) > TEST_NVM_SLOT_SIZE)
    {
        return HAL_NVM_STATUS_IO_ERROR;
    }
    (void)memcpy(data, &g_nvm_storage[slot][offset], length);
    return HAL_NVM_STATUS_OK;
}

static HalNvmStatus_t MockNvmErase(void *context, uint8_t slot)
{
    (void)context;
    (void)memset(g_nvm_storage[slot], 0xFF, TEST_NVM_SLOT_SIZE);
    return HAL_NVM_STATUS_OK;
}

static HalNvmStatus_t MockNvmProgram(void *context, uint8_t slot,
                                     uint8_t page, const uint8_t *data)
{
    (void)context;
    (void)memcpy(&g_nvm_storage[slot][(uint32_t)page * HAL_NVM_PAGE_SIZE],
                 data, HAL_NVM_PAGE_SIZE);
    return HAL_NVM_STATUS_OK;
}

static void ProcessConfigurationEvent(void)
{
    uint8_t step;
    for (step = 0U; step < 4U; step++)
    {
        ProductApplication_Process();
    }
    assert(!EventService_IsTemperatureInputConfigurationChangedPending(0U));
}

int main(void)
{
    ModbusSlaveRegisterInterface_t interface;
    AlarmConfigurationRange_t range;
    SafetyConfigurationRange_t safety_range;
    const uint16_t thermocouple_k[2] =
        {PRODUCT_SENSOR_TYPE_THERMOCOUPLE, PRODUCT_TC_LINEARIZATION_K};
    static const HalPwmDriverOps_t pwm_ops =
        {MockPwmInitialize, MockPwmSetDuty};
    static const HalNvmDriverOps_t nvm_ops =
        {MockNvmInitialize, MockNvmRead, MockNvmErase, MockNvmProgram};

    assert(EventService_Initialize(EVENT_ACK_SERIAL_REQUIRED_DEFAULT));
    (void)memset(g_nvm_storage, 0xFF, sizeof(g_nvm_storage));
    assert(HalPwm_RegisterDriver(0U, &pwm_ops, NULL) == HAL_PWM_STATUS_OK);
    assert(HalNvm_RegisterDriver(&nvm_ops, NULL) == HAL_NVM_STATUS_OK);
    assert(ProductApplication_Init());
    assert(g_hardware_duty_permille == 0U);
    ProductModbusRegisterAdapter_GetInterface(&interface);

    assert(interface.write_multiple_registers(
               interface.context, PRODUCT_MODBUS_SENSOR_TYPE_ADDRESS,
               thermocouple_k, 2U) == MODBUS_EXCEPTION_NONE);
    assert(interface.write_single_register(
               interface.context, PRODUCT_MODBUS_APPLY_KEY_ADDRESS,
               PRODUCT_MODBUS_APPLY_KEY_VALUE) == MODBUS_EXCEPTION_NONE);
    assert(EventService_IsTemperatureInputConfigurationChangedPending(0U));

    ProcessConfigurationEvent();
    assert(AlarmConfigurationEventConsumer_GetRange(0U, &range));
    assert(range.input_enabled);
    assert(range.setpoint_minimum == -270.0F);
    assert(range.setpoint_maximum == 1372.0F);
    assert(range.configuration_revision == 1U);
    assert(SafetyConfigurationEventConsumer_GetRange(0U, &safety_range));
    assert(safety_range.input_enabled);
    assert(!safety_range.output_inhibit);
    assert(safety_range.measurement_minimum == -270.0F);
    assert(safety_range.measurement_maximum == 1372.0F);
    assert(safety_range.configuration_revision == 1U);
    assert(PwmOutputService_SetCommand(0U, 700U) == PWM_OUTPUT_STATUS_OK);
    assert(g_hardware_duty_permille == 700U);

    assert(interface.write_single_register(
               interface.context, PRODUCT_MODBUS_SENSOR_TYPE_ADDRESS,
               PRODUCT_SENSOR_TYPE_OFF) == MODBUS_EXCEPTION_NONE);
    assert(interface.write_single_register(
               interface.context, PRODUCT_MODBUS_APPLY_KEY_ADDRESS,
               PRODUCT_MODBUS_APPLY_KEY_VALUE) == MODBUS_EXCEPTION_NONE);
    assert(g_hardware_duty_permille == 700U);
    ProcessConfigurationEvent();
    assert(AlarmConfigurationEventConsumer_GetRange(0U, &range));
    assert(!range.input_enabled);
    assert(range.configuration_revision == 2U);
    assert(SafetyConfigurationEventConsumer_GetRange(0U, &safety_range));
    assert(!safety_range.input_enabled);
    assert(safety_range.output_inhibit);
    assert(SafetyConfigurationEventConsumer_IsOutputInhibited(0U));
    assert(safety_range.configuration_revision == 2U);
    assert(g_hardware_duty_permille == 0U);
    return 0;
}
