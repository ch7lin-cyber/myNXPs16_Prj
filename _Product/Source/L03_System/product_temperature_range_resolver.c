#include "product_temperature_range_resolver.h"

#include <stddef.h>

#include "product_temperature_input_types.h"

bool ProductTemperatureRangeResolver_Resolve(
    const EventTemperatureInputConfiguration_t *configuration,
    float *minimum,
    float *maximum,
    bool *input_enabled,
    void *context)
{
    (void)context;
    if ((configuration == NULL) || (minimum == NULL) ||
        (maximum == NULL) || (input_enabled == NULL))
    {
        return false;
    }

    if (configuration->sensor_type == PRODUCT_SENSOR_TYPE_OFF)
    {
        *minimum = 0.0F;
        *maximum = 0.0F;
        *input_enabled = false;
        return true;
    }

    *input_enabled = true;
    if ((configuration->sensor_type == PRODUCT_SENSOR_TYPE_RTD_100_OHM) ||
        (configuration->sensor_type == PRODUCT_SENSOR_TYPE_RTD_1000_OHM))
    {
        *minimum = -200.0F;
        *maximum = 850.0F;
        return true;
    }

    if (configuration->sensor_type != PRODUCT_SENSOR_TYPE_THERMOCOUPLE)
    {
        return false;
    }

    switch (configuration->tc_linearization)
    {
        case PRODUCT_TC_LINEARIZATION_B:
            *minimum = 0.0F;
            *maximum = 1820.0F;
            return true;
        case PRODUCT_TC_LINEARIZATION_C:
            *minimum = 0.0F;
            *maximum = 2320.0F;
            return true;
        case PRODUCT_TC_LINEARIZATION_D:
            *minimum = 0.0F;
            *maximum = 2495.0F;
            return true;
        case PRODUCT_TC_LINEARIZATION_E:
            *minimum = -270.0F;
            *maximum = 1000.0F;
            return true;
        case PRODUCT_TC_LINEARIZATION_J:
            *minimum = -210.0F;
            *maximum = 1200.0F;
            return true;
        case PRODUCT_TC_LINEARIZATION_K:
            *minimum = -270.0F;
            *maximum = 1372.0F;
            return true;
        case PRODUCT_TC_LINEARIZATION_N:
            *minimum = -270.0F;
            *maximum = 1300.0F;
            return true;
        case PRODUCT_TC_LINEARIZATION_R:
        case PRODUCT_TC_LINEARIZATION_S:
            *minimum = -50.0F;
            *maximum = 1768.0F;
            return true;
        case PRODUCT_TC_LINEARIZATION_T:
            *minimum = -270.0F;
            *maximum = 400.0F;
            return true;
        default:
            return false;
    }
}
