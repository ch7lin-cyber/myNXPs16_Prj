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

bool FactoryCalibrationService_Convert(uint8_t input,
    FactoryCalibrationProfile_t profile, int32_t raw_uv, int32_t *calibrated_uv)
{
    (void)profile;
    if ((input >= 4U) || (calibrated_uv == NULL)) return false;
    *calibrated_uv = raw_uv;
    return true;
}

static void TestConfigurationMapping(void)
{
    SensorConversionConfig_t configuration;
    FactoryCalibrationProfile_t profile;

    static const struct { uint16_t type; FactoryCalibrationProfile_t profile; }
        mappings[] = {
            {PRODUCT_SENSOR_TYPE_TC_B, FACTORY_CAL_PROFILE_TC_GAIN128},
            {PRODUCT_SENSOR_TYPE_TC_S, FACTORY_CAL_PROFILE_TC_GAIN128},
            {PRODUCT_SENSOR_TYPE_TC_T, FACTORY_CAL_PROFILE_TC_GAIN64},
            {PRODUCT_SENSOR_TYPE_TC_R, FACTORY_CAL_PROFILE_TC_GAIN64},
            {PRODUCT_SENSOR_TYPE_TC_C, FACTORY_CAL_PROFILE_TC_GAIN64},
            {PRODUCT_SENSOR_TYPE_TC_J, FACTORY_CAL_PROFILE_TC_GAIN16},
            {PRODUCT_SENSOR_TYPE_TC_K, FACTORY_CAL_PROFILE_TC_GAIN32},
            {PRODUCT_SENSOR_TYPE_TC_D, FACTORY_CAL_PROFILE_TC_GAIN32},
            {PRODUCT_SENSOR_TYPE_TC_E, FACTORY_CAL_PROFILE_TC_GAIN32},
            {PRODUCT_SENSOR_TYPE_TC_N, FACTORY_CAL_PROFILE_TC_GAIN32},
            {PRODUCT_SENSOR_TYPE_TC_L, FACTORY_CAL_PROFILE_TC_GAIN32},
            {PRODUCT_SENSOR_TYPE_TC_U, FACTORY_CAL_PROFILE_TC_GAIN32},
            {PRODUCT_SENSOR_TYPE_TC_TXK, FACTORY_CAL_PROFILE_TC_GAIN32},
            {PRODUCT_SENSOR_TYPE_VOLTAGE_0_50MV, FACTORY_CAL_PROFILE_TC_GAIN32},
            {PRODUCT_SENSOR_TYPE_RTD_100_OHM, FACTORY_CAL_PROFILE_RTD_GAIN8},
            {PRODUCT_SENSOR_TYPE_RTD_NI120, FACTORY_CAL_PROFILE_RTD_GAIN8},
            {PRODUCT_SENSOR_TYPE_RTD_JPT100, FACTORY_CAL_PROFILE_RTD_GAIN16},
            {PRODUCT_SENSOR_TYPE_RTD_1000_OHM, FACTORY_CAL_PROFILE_RTD_GAIN1},
            {PRODUCT_SENSOR_TYPE_RTD_CU50, FACTORY_CAL_PROFILE_RTD_GAIN32},
            {PRODUCT_SENSOR_TYPE_VOLTAGE_0_5V, FACTORY_CAL_PROFILE_VOLTAGE_GAIN32},
            {PRODUCT_SENSOR_TYPE_VOLTAGE_0_10V, FACTORY_CAL_PROFILE_VOLTAGE_GAIN32},
            {PRODUCT_SENSOR_TYPE_CURRENT_0_20MA, FACTORY_CAL_PROFILE_CURRENT_GAIN64},
            {PRODUCT_SENSOR_TYPE_CURRENT_4_20MA, FACTORY_CAL_PROFILE_CURRENT_GAIN64}
        };
    for (size_t i = 0U; i < sizeof(mappings) / sizeof(mappings[0]); i++)
    {
        assert(ProductSensorMeasurementService_ResolveConfiguration(
            mappings[i].type, &configuration, &profile));
        assert(profile == mappings[i].profile);
    }

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
    assert(profile == FACTORY_CAL_PROFILE_TC_GAIN128);

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
    g_product_configuration.cjcSource = EVENT_CJC_SOURCE_INTERNAL_TMP20;
    g_product_configuration.cjcOffsetCentiC = -529;
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
    assert(g_monitor.unfilteredProcessValue > 19.0F);
    assert(g_monitor.unfilteredProcessValue < 21.0F);
    assert(g_monitor.filteredProcessValue == g_monitor.unfilteredProcessValue);
    assert(ProductSensorMeasurementService_GetSnapshot(2U, &snapshot));
    assert(snapshot.valid);
    assert(snapshot.raw_code == 0x00800000UL);
    assert(snapshot.uncalibrated_uv == 0L);
    assert(snapshot.calibrated_uv == 0L);
    assert(snapshot.cjc_temperature_centi_c == 1971L);
    assert(snapshot.engineering_value > 19000L);
    assert(snapshot.engineering_value < 21000L);
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
    assert(result.engineering_value >= 31950L);
    assert(result.engineering_value <= 32010L);
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


static void TestCjcSourceSelection(void)
{
    ProductSensorMeasurementSnapshot_t snapshot;
    g_product_configuration.sensorType = PRODUCT_SENSOR_TYPE_TC_K;
    g_product_configuration.filterTimeConstantSeconds = 0.0F;
    g_product_configuration.cjcSource = EVENT_CJC_SOURCE_DISABLED;
    g_product_configuration.cjcOffsetCentiC = 0;
    g_sample.microvolts = 10153L;
    g_sample.valid = true;
    g_sample.sequence = 10U;
    g_cjc_valid[0] = false;
    g_cjc_valid[1] = false;
    ProductSensorMeasurementService_Initialize();
    assert(ProductSensorMeasurementService_Process(0U));
    assert(g_monitor.inputError == 61U);
    assert(g_monitor.unfilteredProcessValue > 249.0F && g_monitor.unfilteredProcessValue < 251.0F);
    assert(ProductSensorMeasurementService_GetSnapshot(0U, &snapshot));
    assert(snapshot.cjc_temperature_centi_c == 0L);
    /* Source change must re-evaluate even when the sample sequence is unchanged. */
    g_product_configuration.cjcSource = EVENT_CJC_SOURCE_INTERNAL_TMP20;
    assert(ProductSensorMeasurementService_Process(0U));
    assert(g_monitor.inputError == 9U);
    g_cjc_valid[0] = true;
    g_cjc_temperature_centi_c[0] = 2500L;
    g_cjc_valid[1] = true;
    g_cjc_temperature_centi_c[1] = 4000L;
    g_sample.sequence++;
    assert(ProductSensorMeasurementService_Process(0U));
    assert(g_monitor.inputError == 61U);
    assert(ProductSensorMeasurementService_GetSnapshot(0U, &snapshot));
    assert(snapshot.cjc_temperature_centi_c == 2500L);
    assert(ProductSensorMeasurementService_Process(3U));
    assert(ProductSensorMeasurementService_GetSnapshot(3U, &snapshot));
    assert(snapshot.cjc_temperature_centi_c == 4000L);
    g_product_configuration.cjcSource = EVENT_CJC_SOURCE_EXTERNAL_1;
    assert(ProductSensorMeasurementService_Process(0U));
    assert(g_monitor.inputError == 9U);
    g_product_configuration.cjcSource = EVENT_CJC_SOURCE_EXTERNAL_2;
    assert(ProductSensorMeasurementService_Process(0U));
    assert(g_monitor.inputError == 9U);
    /* K 1300 C remains convertible in disabled mode. */
    g_product_configuration.cjcSource = EVENT_CJC_SOURCE_DISABLED;
    g_sample.microvolts = 52410L;
    for (unsigned int n = 0U; n < 5U; n++)
    {
        g_sample.sequence++;
        assert(ProductSensorMeasurementService_Process(0U));
    }
    assert(g_monitor.inputError == 61U);
    assert(g_monitor.unfilteredProcessValue > 1299.0F && g_monitor.unfilteredProcessValue < 1301.0F);
}

static void TestAnalogPvEngineeringRange(void)
{
    const int32_t microvolts[] = {0L, 25000L, 50000L};
    const float expected[] = {-32000.0F, 0.0F, 32000.0F};
    g_product_configuration.sensorType = PRODUCT_SENSOR_TYPE_VOLTAGE_0_50MV;
    g_product_configuration.filterTimeConstantSeconds = 0.0F;
    g_product_configuration.cjcSource = EVENT_CJC_SOURCE_DISABLED;
    g_sample.valid = true;
    ProductSensorMeasurementService_Initialize();
    for (unsigned int i = 0U; i < 3U; ++i)
    {
        g_sample.microvolts = microvolts[i];
        for (unsigned int n = 0U; n < 5U; n++)
        {
            g_sample.sequence++;
            assert(ProductSensorMeasurementService_Process(0U));
        }
        assert(g_monitor.unfilteredProcessValue == expected[i]);
    }
}

static void TestDistinctRangeErrors(void)
{
    const uint16_t types[] = {
        PRODUCT_SENSOR_TYPE_TC_K, PRODUCT_SENSOR_TYPE_RTD_100_OHM,
        PRODUCT_SENSOR_TYPE_VOLTAGE_0_50MV, PRODUCT_SENSOR_TYPE_CURRENT_4_20MA};
    SensorConversionConfig_t config;
    FactoryCalibrationProfile_t profile;
    ProductSensorMeasurementSnapshot_t snapshot;
    const uint16_t errors[] = {61U, 142U, 143U, 142U, 143U, 61U};
    g_product_configuration.filterTimeConstantSeconds = 0.0F;
    g_product_configuration.cjcSource = EVENT_CJC_SOURCE_DISABLED;
    g_product_configuration.cjcOffsetCentiC = 0;
    g_sample.valid = true;
    for (unsigned int i = 0U; i < 4U; i++)
    {
        g_product_configuration.sensorType = types[i];
        ProductSensorMeasurementService_Initialize();
        assert(ProductSensorMeasurementService_ResolveConfiguration(types[i], &config, &profile));
        for (unsigned int j = 0U; j < 6U; j++)
        {
            if (i < 2U)
            {
                const PiecewiseLinearTable_t *table = config.measurement_table;
                const PiecewiseLinearSegment_t *edge = j < 3U ?
                    &table->segments[0] : &table->segments[table->segment_count-1U];
                int32_t target = j < 3U ? config.nominal_min : config.nominal_max;
                target += (j == 0U) ? 10000L : (j == 1U) ? -10000L :
                          (j == 2U) ? -30000L : (j == 3U) ? 10000L :
                          (j == 4U) ? 30000L : -10000L;
                int64_t input = ((int64_t)target * table->coefficient_scale - edge->intercept) / edge->slope;
                g_sample.microvolts = i == 0U ? (int32_t)input :
                    (int32_t)(input * config.excitation_current_ua / 1000LL);
            }
            else if (i == 2U)
            {
                const int32_t uv[] = {1000L, -500L, -1001L, 50500L, 51001L, 1000L};
                g_sample.microvolts = uv[j];
            }
            else
            {
                const int32_t ua[] = {10000L, 3900L, 3600L, 20100L, 20500L, 10000L};
                g_sample.microvolts = (int32_t)((int64_t)ua[j] * config.shunt_resistance_milliohm *
                    config.frontend_ratio_denominator / (1000LL * config.frontend_ratio_numerator));
            }
            for (unsigned int n = 0U; n < 5U; n++)
            {
                g_sample.sequence++;
                assert(ProductSensorMeasurementService_Process(0U));
            }
            assert(g_monitor.inputError == errors[j]);
            assert(ProductSensorMeasurementService_GetSnapshot(0U, &snapshot));
            assert(snapshot.input_error == errors[j]);
            assert(snapshot.valid);
        }
    }
}

static void TestMedianWindowFive(void)
{
    /* 25 mV maps to zero; a high/low spike must not change that PV.
     * Two high samples are rejected; the third makes the window majority. */
    const int32_t inputs[] = {25000, 50000, 0, 25000, 25000,
                             50000, 50000, 50000};
    g_product_configuration.sensorType = PRODUCT_SENSOR_TYPE_VOLTAGE_0_50MV;
    g_product_configuration.cjcSource = EVENT_CJC_SOURCE_DISABLED;
    g_product_configuration.filterTimeConstantSeconds = 0.0F;
    g_sample.valid = true;
    ProductSensorMeasurementService_Initialize();
    for (unsigned int i = 0U; i < sizeof(inputs) / sizeof(inputs[0]); i++)
    {
        g_sample.microvolts = inputs[i];
        g_sample.sequence++;
        assert(ProductSensorMeasurementService_Process(0U));
        assert(g_monitor.unfilteredProcessValue == (i == 7U ? 32000.0F : 0.0F));
        /* Foreground polling the same sample must not shift the window. */
        for (unsigned int n = 0U; n < 8U; n++)
        {
            assert(ProductSensorMeasurementService_Process(0U));
            assert(g_monitor.unfilteredProcessValue == (i == 7U ? 32000.0F : 0.0F));
        }
    }
    /* A second channel starts with its own independent history. */
    g_sample.microvolts = 0;
    assert(ProductSensorMeasurementService_Process(1U));
    assert(g_monitor.unfilteredProcessValue == -32000.0F);
    /* OFF then ON must seed a fresh window, even at the same sequence. */
    g_product_configuration.sensorType = PRODUCT_SENSOR_TYPE_OFF;
    assert(ProductSensorMeasurementService_Process(0U));
    g_product_configuration.sensorType = PRODUCT_SENSOR_TYPE_VOLTAGE_0_50MV;
    assert(ProductSensorMeasurementService_Process(0U));
    assert(g_monitor.unfilteredProcessValue == -32000.0F);
}

static void TestProcessingTimestamp(void)
{
    ProductSensorMeasurementSnapshot_t snapshot;
    g_product_configuration.sensorType=PRODUCT_SENSOR_TYPE_VOLTAGE_0_50MV;
    g_product_configuration.cjcSource=EVENT_CJC_SOURCE_DISABLED;
    g_product_configuration.filterTimeConstantSeconds=0.0F;
    g_sample.valid=true; g_sample.microvolts=25000; g_sample.sequence=99;
    ProductSensorMeasurementService_Initialize();
    assert(ProductSensorMeasurementService_ProcessAt(0,100));
    ProductSensorMeasurementService_GetSnapshot(0,&snapshot);
    assert(snapshot.processed_timestamp_valid && snapshot.processed_timestamp_ms==100);
    assert(ProductSensorMeasurementService_ProcessAt(0,200));
    ProductSensorMeasurementService_GetSnapshot(0,&snapshot);
    assert(snapshot.processed_timestamp_ms==100); /* repeated sequence does not refresh */
    g_sample.valid=false;
    (void)ProductSensorMeasurementService_ProcessAt(0,250);
    g_sample.valid=true;
    assert(ProductSensorMeasurementService_ProcessAt(0,300));
    ProductSensorMeasurementService_GetSnapshot(0,&snapshot);
    assert(snapshot.processed_timestamp_ms==100); /* same ADC sample after error */
    ++g_sample.sequence;
    assert(ProductSensorMeasurementService_ProcessAt(0,400));
    ProductSensorMeasurementService_GetSnapshot(0,&snapshot);
    assert(snapshot.processed_timestamp_ms==400);
    ++g_sample.sequence;
    assert(ProductSensorMeasurementService_Process(0));
    ProductSensorMeasurementService_GetSnapshot(0,&snapshot);
    assert(!snapshot.processed_timestamp_valid); /* legacy untimed entry cannot invent time */
    ++g_sample.sequence;
    assert(ProductSensorMeasurementService_ProcessAt(0,500));
    ProductSensorMeasurementService_GetSnapshot(0,&snapshot);
    assert(snapshot.processed_timestamp_valid && snapshot.processed_timestamp_ms==500);
}

int main(void)
{
    TestProcessingTimestamp();
    TestMedianWindowFive();
    TestConfigurationMapping();
    TestKTypePipelineAndCjcPairing();
    TestCurrentShuntConversion();
    TestBTypeConversionWithCjc();
    TestInvalidCjcIsReported();
    TestCjcSourceSelection();
    TestAnalogPvEngineeringRange();
    TestDistinctRangeErrors();
    return 0;
}

