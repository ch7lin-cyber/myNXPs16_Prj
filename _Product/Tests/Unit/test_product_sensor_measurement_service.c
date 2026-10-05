#include <assert.h>
#include <string.h>

#include "AnalogInputService.h"
#include "ProductAdcConfig.h"
#include "bsp_internal_adc.h"
#include "product_modbus_register_adapter.h"
#include "product_sensor_measurement_service.h"
#include "product_temperature_input_types.h"

static AnalogInputSample_t g_sample;
static int32_t g_cjc_temperature_centi_c[BSP_INTERNAL_ADC_CJC_COUNT];
static bool g_cjc_valid[BSP_INTERNAL_ADC_CJC_COUNT];
static product_temperature_input_config_t g_product_configuration;
static product_temperature_input_monitor_t g_monitor;

bool AnalogInputService_GetLatestByInput(
    uint8_t logical_input,
    AnalogInputSample_t *sample)
{
    if ((logical_input >= 4U) || (sample == NULL) || !g_sample.valid)
    {
        return false;
    }
    *sample = g_sample;
    return true;
}

bool BspInternalAdc_GetCjcTemperature(
    uint8_t cjc,
    int32_t *temperature_centi_c)
{
    if ((cjc >= BSP_INTERNAL_ADC_CJC_COUNT) ||
        (temperature_centi_c == NULL) || !g_cjc_valid[cjc])
    {
        return false;
    }
    *temperature_centi_c = g_cjc_temperature_centi_c[cjc];
    return true;
}

bool ProductModbusRegisterAdapter_GetTemperatureInputConfigForChannel(
    uint8_t channel,
    product_temperature_input_config_t *config)
{
    if ((channel >= 4U) || (config == NULL))
    {
        return false;
    }
    *config = g_product_configuration;
    return true;
}

bool ProductModbusRegisterAdapter_SetTemperatureInputMonitorForChannel(
    uint8_t channel,
    const product_temperature_input_monitor_t *monitor)
{
    if ((channel >= 4U) || (monitor == NULL))
    {
        return false;
    }
    g_monitor = *monitor;
    return true;
}

bool FactoryCalibrationService_GetCalibration(
    uint8_t input,
    FactoryCalibrationProfile_t profile,
    HalAdcFactoryCalibration_t *calibration)
{
    (void)profile;
    if ((input >= 4U) || (calibration == NULL))
    {
        return false;
    }
    calibration->measured_zero_uv = 0L;
    calibration->measured_span_uv = 30000L;
    calibration->valid = true;
    return true;
}

static void TestConfigurationMapping(void)
{
    SensorConversionConfig_t configuration;
    FactoryCalibrationProfile_t profile;

    assert(ProductSensorMeasurementService_ResolveConfiguration(
        PRODUCT_SENSOR_TYPE_TC_K, &configuration, &profile));
    assert(configuration.type == SENSOR_TYPE_TC_K);
    assert(configuration.measurement_table != NULL);
    assert(configuration.cjc_table != NULL);
    assert(profile == FACTORY_CAL_PROFILE_TC_GAIN1);

    assert(ProductSensorMeasurementService_ResolveConfiguration(
        PRODUCT_SENSOR_TYPE_TC_B, &configuration, &profile));
    assert(configuration.type == SENSOR_TYPE_TC_B);
    assert(configuration.measurement_table != NULL);
    assert(configuration.cjc_table != NULL);
    assert(profile == FACTORY_CAL_PROFILE_TC_GAIN2);

    assert(ProductSensorMeasurementService_ResolveConfiguration(
        PRODUCT_SENSOR_TYPE_RTD_100_OHM, &configuration, &profile));
    assert(configuration.type == SENSOR_TYPE_RTD_PT100);
    assert(configuration.excitation_current_ua == 500UL);
    assert(profile == FACTORY_CAL_PROFILE_RTD);

    assert(ProductSensorMeasurementService_ResolveConfiguration(
        PRODUCT_SENSOR_TYPE_VOLTAGE_0_50MV, &configuration, &profile));
    assert(configuration.frontend_ratio_numerator == 1UL);
    assert(configuration.frontend_ratio_denominator == 1UL);
    assert(profile == FACTORY_CAL_PROFILE_MV50);

    assert(ProductSensorMeasurementService_ResolveConfiguration(
        PRODUCT_SENSOR_TYPE_VOLTAGE_0_10V, &configuration, &profile));
    assert(configuration.frontend_ratio_numerator ==
           PRODUCT_ADC_VOLTAGE_RATIO_NUMERATOR);
    assert(configuration.frontend_ratio_denominator ==
           PRODUCT_ADC_VOLTAGE_RATIO_DENOMINATOR);
    assert(profile == FACTORY_CAL_PROFILE_V10);

    assert(ProductSensorMeasurementService_ResolveConfiguration(
        PRODUCT_SENSOR_TYPE_CURRENT_4_20MA, &configuration, &profile));
    assert(configuration.shunt_resistance_milliohm == 49900UL);
    assert(configuration.frontend_ratio_numerator == 1056800UL);
    assert(configuration.frontend_ratio_denominator == 36800UL);
    assert(profile == FACTORY_CAL_PROFILE_MA);
}

static void TestKTypePipelineAndCjcPairing(void)
{
    ProductSensorMeasurementSnapshot_t snapshot;

    (void)memset(g_cjc_temperature_centi_c, 0,
                 sizeof(g_cjc_temperature_centi_c));
    (void)memset(g_cjc_valid, 0, sizeof(g_cjc_valid));
    (void)memset(&g_monitor, 0, sizeof(g_monitor));
    g_product_configuration.filterTimeConstantSeconds = 0.0F;
    g_product_configuration.sensorType = PRODUCT_SENSOR_TYPE_TC_K;
    g_sample.raw_code = 0x00800000UL;
    g_sample.microvolts = 0L;
    g_sample.sequence = 1UL;
    g_sample.channel = 0U;
    g_sample.valid = true;
    g_cjc_temperature_centi_c[0] = 0L;
    g_cjc_valid[0] = true;
    g_cjc_temperature_centi_c[1] = 2500L;
    g_cjc_valid[1] = true;

    ProductSensorMeasurementService_Initialize();
    assert(ProductSensorMeasurementService_Process(2U));
    assert(g_monitor.inputError == 61U);
    assert(g_monitor.unfilteredProcessValue > 24.0F);
    assert(g_monitor.unfilteredProcessValue < 26.0F);
    assert(g_monitor.filteredProcessValue == g_monitor.unfilteredProcessValue);
    assert(ProductSensorMeasurementService_GetSnapshot(2U, &snapshot));
    assert(snapshot.valid);
    assert(snapshot.raw_code == 0x00800000UL);
    assert(snapshot.uncalibrated_uv == 0L);
    assert(snapshot.calibrated_uv == 0L);
    assert(snapshot.cjc_temperature_centi_c == 2500L);
    assert(snapshot.engineering_value > 24000L);
    assert(snapshot.engineering_value < 26000L);
}

static void TestCurrentShuntConversion(void)
{
    SensorConversionConfig_t configuration;
    SensorConversionResult_t result;
    FactoryCalibrationProfile_t profile;

    assert(ProductSensorMeasurementService_ResolveConfiguration(
        PRODUCT_SENSOR_TYPE_CURRENT_4_20MA, &configuration, &profile));
    /* 20 mA through 49.9 ohm and the 36.8/1056.8 divider. */
    assert(SensorConversionService_Convert(
        &configuration, 34746L, 0L, &result));
    assert(result.physical_input >= 19990L);
    assert(result.physical_input <= 20010L);
    assert(result.engineering_value >= 19980L);
    assert(result.engineering_value <= 20010L);
}

static void TestBTypeConversionWithCjc(void)
{
    SensorConversionConfig_t configuration;
    SensorConversionResult_t result;
    FactoryCalibrationProfile_t profile;

    assert(ProductSensorMeasurementService_ResolveConfiguration(
        PRODUCT_SENSOR_TYPE_TC_B, &configuration, &profile));
    /* At 1000 C the B-type EMF is 4834 uV; CJC at 25 C is -2 uV. */
    assert(SensorConversionService_Convert(
        &configuration, 4836L, 25000L, &result));
    assert(result.cjc_uv == -2L);
    assert(result.physical_input == 4834L);
    assert(result.engineering_value >= 999900L);
    assert(result.engineering_value <= 1000100L);
    assert(result.flags == 0UL);
}

static void TestInvalidCjcIsReported(void)
{
    ProductSensorMeasurementSnapshot_t snapshot;

    g_sample.sequence = 2UL;
    g_cjc_valid[0] = false;
    ProductSensorMeasurementService_Initialize();
    assert(ProductSensorMeasurementService_Process(0U));
    assert(g_monitor.inputError == 9U);
    assert(ProductSensorMeasurementService_GetSnapshot(0U, &snapshot));
    assert(!snapshot.valid);
    assert((snapshot.conversion_flags & SENSOR_FLAG_CJC_ERROR) != 0UL);
}

int main(void)
{
    TestConfigurationMapping();
    TestKTypePipelineAndCjcPairing();
    TestCurrentShuntConversion();
    TestBTypeConversionWithCjc();
    TestInvalidCjcIsReported();
    return 0;
}
