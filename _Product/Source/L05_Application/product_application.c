#include "product_application.h"

#include <stddef.h>

#include "AlarmConfigurationEventConsumer.h"
#include "EventService.h"
#include "FaultService.h"
#include "NvmConfigurationEventConsumer.h"
#include "NvmService.h"
#include "PwmOutputService.h"
#include "SafetyConfigurationEventConsumer.h"
#include "product_temperature_range_resolver.h"
#include "product_modbus_register_adapter.h"

static bool g_last_pwm_inhibited = true;

static bool IsPwmOutputInhibited(uint8_t channel, void *context)
{
    (void)context;
    if (channel != 0U)
    {
        return true;
    }
    return SafetyConfigurationEventConsumer_IsOutputInhibited(0U);
}

bool ProductApplication_Init(void)
{
    FaultService_Initialize();

    if (!EventService_ConfigureTemperatureInputRequiredAckMask(
            EVENT_ACK_ALARM | EVENT_ACK_SAFETY | EVENT_ACK_NVM))
    {
        return false;
    }

    if (!AlarmConfigurationEventConsumer_Initialize(
            ProductTemperatureRangeResolver_Resolve, NULL))
    {
        return false;
    }

    if (!SafetyConfigurationEventConsumer_Initialize(
            ProductTemperatureRangeResolver_Resolve, NULL))
    {
        return false;
    }

    if (!NvmConfigurationEventConsumer_Initialize())
    {
        return false;
    }

    {
        EventTemperatureInputConfiguration_t stored;
        product_temperature_input_config_t product_config;
        uint16_t stored_revision;
        if (NvmService_GetLoadedTemperatureInputConfiguration(
                &stored_revision, &stored))
        {
            product_config.filterTimeConstantSeconds =
                stored.filter_time_constant_seconds;
            product_config.sensorType = stored.sensor_type;
            product_config.tcLinearization = stored.tc_linearization;
            if (!ProductModbusRegisterAdapter_RestoreTemperatureInputConfig(
                    &product_config, stored_revision) ||
                !EventService_RaiseTemperatureInputConfigurationChanged(
                    0U, stored_revision,
                    EVENT_TEMPERATURE_INPUT_CHANGE_ALL,
                    &stored, &stored, NULL))
            {
                return false;
            }
        }
    }

    if (PwmOutputService_Initialize(
            1U, IsPwmOutputInhibited, NULL) != PWM_OUTPUT_STATUS_OK)
    {
        return false;
    }
    g_last_pwm_inhibited = true;
    return true;
}

void ProductApplication_Process(void)
{
    (void)AlarmConfigurationEventConsumer_Process(0U);
    (void)SafetyConfigurationEventConsumer_Process(0U);
    (void)NvmConfigurationEventConsumer_Process(0U);

    {
        bool inhibited =
            SafetyConfigurationEventConsumer_IsOutputInhibited(0U);
        if (inhibited != g_last_pwm_inhibited)
        {
            (void)PwmOutputService_RefreshSafety(0U);
            g_last_pwm_inhibited = inhibited;
        }
    }
}
