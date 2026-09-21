#include "product_application.h"

#include <stddef.h>

#include "AlarmConfigurationEventConsumer.h"
#include "EventService.h"
#include "SafetyConfigurationEventConsumer.h"
#include "product_temperature_range_resolver.h"

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

    return SafetyConfigurationEventConsumer_Initialize(
        ProductTemperatureRangeResolver_Resolve, NULL);
}

void ProductApplication_Process(void)
{
    (void)AlarmConfigurationEventConsumer_Process(0U);
    (void)SafetyConfigurationEventConsumer_Process(0U);
}
