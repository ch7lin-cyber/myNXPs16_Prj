#include "product_application.h"

#include <stddef.h>

#include "AlarmConfigurationEventConsumer.h"
#include "EventService.h"
#include "PwmOutputService.h"
#include "SafetyConfigurationEventConsumer.h"
#include "product_temperature_range_resolver.h"

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
    if (!EventService_ConfigureTemperatureInputRequiredAckMask(
            EVENT_ACK_ALARM | EVENT_ACK_SAFETY))
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
