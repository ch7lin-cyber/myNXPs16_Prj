#ifndef PRODUCT_TEMPERATURE_RANGE_RESOLVER_H
#define PRODUCT_TEMPERATURE_RANGE_RESOLVER_H

#include <stdbool.h>

#include "EventService.h"

bool ProductTemperatureRangeResolver_Resolve(
    const EventTemperatureInputConfiguration_t *configuration,
    float *minimum,
    float *maximum,
    bool *input_enabled,
    void *context);

#endif
