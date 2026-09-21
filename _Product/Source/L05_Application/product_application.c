#include "product_application.h"

#include <stddef.h>

#include "AlarmConfigurationEventConsumer.h"
#include "EventService.h"
#include "product_temperature_range_resolver.h"

bool ProductApplication_Init(void)
{
    if (!EventService_ConfigureTemperatureInputRequiredAckMask(
            EVENT_ACK_ALARM))
    {
        return false;
    }

    return AlarmConfigurationEventConsumer_Initialize(
        ProductTemperatureRangeResolver_Resolve, NULL);
}

void ProductApplication_Process(void)
{
    (void)AlarmConfigurationEventConsumer_Process(0U);
}
