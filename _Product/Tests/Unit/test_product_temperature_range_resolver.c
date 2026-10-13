#include <assert.h>
#include "product_temperature_range_resolver.h"
#include "product_temperature_input_types.h"

int main(void)
{
    static const struct { uint16_t type; float low; float high; } ranges[] = {
        {PRODUCT_SENSOR_TYPE_TC_B, 80.0F, 1820.0F},
        {PRODUCT_SENSOR_TYPE_TC_C, -20.0F, 2320.0F},
        {PRODUCT_SENSOR_TYPE_TC_D, -20.0F, 2320.0F},
        {PRODUCT_SENSOR_TYPE_TC_E, -20.0F, 620.0F},
        {PRODUCT_SENSOR_TYPE_TC_J, -220.0F, 1220.0F},
        {PRODUCT_SENSOR_TYPE_TC_K, -220.0F, 1320.0F},
        {PRODUCT_SENSOR_TYPE_TC_N, -220.0F, 1320.0F},
        {PRODUCT_SENSOR_TYPE_TC_R, -20.0F, 1720.0F},
        {PRODUCT_SENSOR_TYPE_TC_S, -20.0F, 1720.0F},
        {PRODUCT_SENSOR_TYPE_TC_T, -220.0F, 420.0F},
        {PRODUCT_SENSOR_TYPE_TC_L, -220.0F, 870.0F},
        {PRODUCT_SENSOR_TYPE_TC_U, -220.0F, 520.0F},
        {PRODUCT_SENSOR_TYPE_TC_TXK, -170.0F, 820.0F},
        {PRODUCT_SENSOR_TYPE_RTD_100_OHM, -220.0F, 870.0F},
        {PRODUCT_SENSOR_TYPE_RTD_1000_OHM, -220.0F, 870.0F},
        {PRODUCT_SENSOR_TYPE_RTD_JPT100, -40.0F, 420.0F},
        {PRODUCT_SENSOR_TYPE_RTD_NI120, -100.0F, 320.0F},
        {PRODUCT_SENSOR_TYPE_RTD_CU50, -70.0F, 170.0F},
        {PRODUCT_SENSOR_TYPE_VOLTAGE_0_5V, -32000.0F, 32000.0F},
        {PRODUCT_SENSOR_TYPE_CURRENT_4_20MA, -32000.0F, 32000.0F},
        {PRODUCT_SENSOR_TYPE_OFF, 0.0F, 0.0F}
    };
    EventTemperatureInputConfiguration_t config = {0};
    float low, high;
    bool enabled;
    for (unsigned int i = 0; i < sizeof(ranges)/sizeof(ranges[0]); i++)
    {
        config.sensor_type = ranges[i].type;
        assert(ProductTemperatureRangeResolver_Resolve(&config, &low, &high, &enabled, 0));
        assert(low == ranges[i].low && high == ranges[i].high);
        assert(enabled == (config.sensor_type != PRODUCT_SENSOR_TYPE_OFF));
    }
    config.sensor_type = 65535U;
    assert(!ProductTemperatureRangeResolver_Resolve(&config, &low, &high, &enabled, 0));
    return 0;
}
