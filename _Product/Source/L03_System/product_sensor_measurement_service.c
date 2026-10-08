#include "product_sensor_measurement_service.h"

#include <stddef.h>
#include <string.h>

#include "AnalogInputService.h"
#include "HalAdcMeasurement.h"
#include "ProductAdcConfig.h"
#include "bsp_internal_adc.h"
#include "product_modbus_register_adapter.h"
#include "product_temperature_input_types.h"

#include "SensorTables/rtd_cu50_table.h"
#include "SensorTables/rtd_jpt100_table.h"
#include "SensorTables/rtd_ni120_table.h"
#include "SensorTables/rtd_pt100_table.h"
#include "SensorTables/rtd_pt1000_table.h"
#include "SensorTables/tc_b_table.h"
#include "SensorTables/tc_c_table.h"
#include "SensorTables/tc_d_table.h"
#include "SensorTables/tc_e_table.h"
#include "SensorTables/tc_j_table.h"
#include "SensorTables/tc_k_table.h"
#include "SensorTables/tc_l_table.h"
#include "SensorTables/tc_n_table.h"
#include "SensorTables/tc_r_table.h"
#include "SensorTables/tc_s_table.h"
#include "SensorTables/tc_t_table.h"
#include "SensorTables/tc_txk_table.h"
#include "SensorTables/tc_u_table.h"

#define PRODUCT_SENSOR_INPUT_COUNT             (4U)
#define PRODUCT_INPUT_ERROR_NONE               (61U)
#define PRODUCT_INPUT_ERROR_FAIL               (32U)
#define PRODUCT_INPUT_ERROR_AMBIENT            (9U)
#define PRODUCT_INPUT_ERROR_BAD_CALIBRATION    (139U)
#define PRODUCT_INPUT_ERROR_MEASUREMENT        (140U)
#define PRODUCT_INPUT_ERROR_RTD                (141U)
#define PRODUCT_ANALOG_ENGINEERING_MIN         (-1999L)
#define PRODUCT_ANALOG_ENGINEERING_MAX         (19999L)

typedef struct
{
    ProductSensorMeasurementSnapshot_t snapshot;
    float unfiltered_value;
    float filtered_value;
    uint32_t last_sample_sequence;
    uint16_t last_sensor_type;
    bool filter_initialized;
} ProductSensorMeasurementState_t;

static ProductSensorMeasurementState_t
    g_measurement_state[PRODUCT_SENSOR_INPUT_COUNT];

static void SetCommonConfiguration(
    SensorConversionConfig_t *configuration,
    SensorConversionType_t type,
    int32_t minimum,
    int32_t maximum,
    int32_t margin,
    const PiecewiseLinearTable_t *measurement,
    const PiecewiseLinearTable_t *cjc)
{
    configuration->type = type;
    configuration->nominal_min = minimum;
    configuration->nominal_max = maximum;
    configuration->extended_margin = margin;
    configuration->measurement_table = measurement;
    configuration->cjc_table = cjc;
    configuration->frontend_ratio_numerator = 1UL;
    configuration->frontend_ratio_denominator = 1UL;
}

#define SET_TC_CONFIG(product_type, conversion_type, macro_prefix,          \
                      function_prefix, profile)                             \
    case product_type:                                                      \
        if (!function_prefix##Table_IsReady())                              \
        {                                                                   \
            return false;                                                   \
        }                                                                   \
        SetCommonConfiguration(configuration, conversion_type,              \
            macro_prefix##_MIN_MILLICELSIUS,                                \
            macro_prefix##_MAX_MILLICELSIUS,                                \
            macro_prefix##_EXTENDED_MARGIN_MC,                              \
            function_prefix##Table_GetMeasurementTable(),                   \
            function_prefix##Table_GetCjcTable());                          \
        *calibration_profile = profile;                                      \
        return true

#define SET_RTD_CONFIG(product_type, conversion_type, macro_prefix,         \
                       function_prefix, current, profile)                   \
    case product_type:                                                      \
        if (!function_prefix##Table_IsReady())                              \
        {                                                                   \
            return false;                                                   \
        }                                                                   \
        SetCommonConfiguration(configuration, conversion_type,              \
            macro_prefix##_MIN_MILLICELSIUS,                                \
            macro_prefix##_MAX_MILLICELSIUS,                                \
            macro_prefix##_EXTENDED_MARGIN_MC,                              \
            function_prefix##Table_GetMeasurementTable(), NULL);            \
        configuration->excitation_current_ua = current;                     \
        *calibration_profile = profile;                     \
        return true

bool ProductSensorMeasurementService_ResolveConfiguration(
    uint16_t sensor_type,
    SensorConversionConfig_t *configuration,
    FactoryCalibrationProfile_t *calibration_profile)
{
    if ((configuration == NULL) || (calibration_profile == NULL))
    {
        return false;
    }
    (void)memset(configuration, 0, sizeof(*configuration));

    switch (sensor_type)
    {
        SET_TC_CONFIG(PRODUCT_SENSOR_TYPE_TC_B, SENSOR_TYPE_TC_B, TC_B, TcB,
                      FACTORY_CAL_PROFILE_TC_GAIN128);
        SET_TC_CONFIG(PRODUCT_SENSOR_TYPE_TC_C, SENSOR_TYPE_TC_C, TC_C, TcC,
                      FACTORY_CAL_PROFILE_TC_GAIN64);
        SET_TC_CONFIG(PRODUCT_SENSOR_TYPE_TC_D, SENSOR_TYPE_TC_D, TC_D, TcD,
                      FACTORY_CAL_PROFILE_TC_GAIN32);
        SET_TC_CONFIG(PRODUCT_SENSOR_TYPE_TC_E, SENSOR_TYPE_TC_E, TC_E, TcE,
                      FACTORY_CAL_PROFILE_TC_GAIN32);
        SET_TC_CONFIG(PRODUCT_SENSOR_TYPE_TC_J, SENSOR_TYPE_TC_J, TC_J, TcJ,
                      FACTORY_CAL_PROFILE_TC_GAIN16);
        SET_TC_CONFIG(PRODUCT_SENSOR_TYPE_TC_K, SENSOR_TYPE_TC_K, TC_K, TcK,
                      FACTORY_CAL_PROFILE_TC_GAIN32);
        SET_TC_CONFIG(PRODUCT_SENSOR_TYPE_TC_N, SENSOR_TYPE_TC_N, TC_N, TcN,
                      FACTORY_CAL_PROFILE_TC_GAIN32);
        SET_TC_CONFIG(PRODUCT_SENSOR_TYPE_TC_R, SENSOR_TYPE_TC_R, TC_R, TcR,
                      FACTORY_CAL_PROFILE_TC_GAIN64);
        SET_TC_CONFIG(PRODUCT_SENSOR_TYPE_TC_S, SENSOR_TYPE_TC_S, TC_S, TcS,
                      FACTORY_CAL_PROFILE_TC_GAIN128);
        SET_TC_CONFIG(PRODUCT_SENSOR_TYPE_TC_T, SENSOR_TYPE_TC_T, TC_T, TcT,
                      FACTORY_CAL_PROFILE_TC_GAIN64);
        SET_TC_CONFIG(PRODUCT_SENSOR_TYPE_TC_L, SENSOR_TYPE_TC_L, TC_L, TcL,
                      FACTORY_CAL_PROFILE_TC_GAIN32);
        SET_TC_CONFIG(PRODUCT_SENSOR_TYPE_TC_U, SENSOR_TYPE_TC_U, TC_U, TcU,
                      FACTORY_CAL_PROFILE_TC_GAIN32);
        SET_TC_CONFIG(PRODUCT_SENSOR_TYPE_TC_TXK, SENSOR_TYPE_TC_TXK, TC_TXK,
                      TcTxk,
                      FACTORY_CAL_PROFILE_TC_GAIN32);

        SET_RTD_CONFIG(PRODUCT_SENSOR_TYPE_RTD_100_OHM,
                       SENSOR_TYPE_RTD_PT100, RTD_PT100, RtdPt100,
                       PRODUCT_ADC_IEX_UA_PT100, FACTORY_CAL_PROFILE_RTD_GAIN8);
        SET_RTD_CONFIG(PRODUCT_SENSOR_TYPE_RTD_1000_OHM,
                       SENSOR_TYPE_RTD_PT1000, RTD_PT1000, RtdPt1000,
                       PRODUCT_ADC_IEX_UA_PT1000, FACTORY_CAL_PROFILE_RTD_GAIN1);
        SET_RTD_CONFIG(PRODUCT_SENSOR_TYPE_RTD_JPT100,
                       SENSOR_TYPE_RTD_JPT100, RTD_JPT100, RtdJpt100,
                       PRODUCT_ADC_IEX_UA_JPT100, FACTORY_CAL_PROFILE_RTD_GAIN16);
        SET_RTD_CONFIG(PRODUCT_SENSOR_TYPE_RTD_NI120,
                       SENSOR_TYPE_RTD_NI120, RTD_NI120, RtdNi120,
                       PRODUCT_ADC_IEX_UA_NI120, FACTORY_CAL_PROFILE_RTD_GAIN8);
        SET_RTD_CONFIG(PRODUCT_SENSOR_TYPE_RTD_CU50,
                       SENSOR_TYPE_RTD_CU50, RTD_CU50, RtdCu50,
                       PRODUCT_ADC_IEX_UA_CU50, FACTORY_CAL_PROFILE_RTD_GAIN32);

        case PRODUCT_SENSOR_TYPE_VOLTAGE_0_5V:
        case PRODUCT_SENSOR_TYPE_VOLTAGE_0_10V:
        case PRODUCT_SENSOR_TYPE_VOLTAGE_0_50MV:
            SetCommonConfiguration(
                configuration,
                (sensor_type == PRODUCT_SENSOR_TYPE_VOLTAGE_0_5V) ?
                    SENSOR_TYPE_VOLTAGE_0_5V :
                (sensor_type == PRODUCT_SENSOR_TYPE_VOLTAGE_0_10V) ?
                    SENSOR_TYPE_VOLTAGE_0_10V :
                    SENSOR_TYPE_VOLTAGE_0_50MV,
                PRODUCT_ANALOG_ENGINEERING_MIN,
                PRODUCT_ANALOG_ENGINEERING_MAX, 0L, NULL, NULL);
            if (sensor_type == PRODUCT_SENSOR_TYPE_VOLTAGE_0_50MV)
            {
                *calibration_profile = FACTORY_CAL_PROFILE_TC_GAIN32;
            }
            else
            {
                configuration->frontend_ratio_numerator =
                    PRODUCT_ADC_VOLTAGE_RATIO_NUMERATOR;
                configuration->frontend_ratio_denominator =
                    PRODUCT_ADC_VOLTAGE_RATIO_DENOMINATOR;
                *calibration_profile = FACTORY_CAL_PROFILE_VOLTAGE_GAIN32;
            }
            return true;

        case PRODUCT_SENSOR_TYPE_CURRENT_0_20MA:
        case PRODUCT_SENSOR_TYPE_CURRENT_4_20MA:
            SetCommonConfiguration(
                configuration,
                (sensor_type == PRODUCT_SENSOR_TYPE_CURRENT_0_20MA) ?
                    SENSOR_TYPE_CURRENT_0_20MA :
                    SENSOR_TYPE_CURRENT_4_20MA,
                PRODUCT_ANALOG_ENGINEERING_MIN,
                PRODUCT_ANALOG_ENGINEERING_MAX, 0L, NULL, NULL);
            configuration->shunt_resistance_milliohm =
                PRODUCT_ADC_CURRENT_SHUNT_MILLIOHM;
            configuration->frontend_ratio_numerator =
                PRODUCT_ADC_CURRENT_RATIO_DENOMINATOR;
            configuration->frontend_ratio_denominator =
                PRODUCT_ADC_CURRENT_RATIO_NUMERATOR;
            *calibration_profile = FACTORY_CAL_PROFILE_CURRENT_GAIN64;
            return true;

        default:
            return false;
    }
}

#undef SET_TC_CONFIG
#undef SET_RTD_CONFIG

static bool IsTemperatureType(SensorConversionType_t type)
{
    return SensorConversionService_IsThermocouple(type) ||
           SensorConversionService_IsRtd(type);
}

static uint16_t ConversionError(
    SensorConversionType_t type,
    uint32_t flags)
{
    if ((flags & SENSOR_FLAG_CJC_ERROR) != 0UL)
    {
        return PRODUCT_INPUT_ERROR_AMBIENT;
    }
    if (SensorConversionService_IsRtd(type))
    {
        return PRODUCT_INPUT_ERROR_RTD;
    }
    return PRODUCT_INPUT_ERROR_MEASUREMENT;
}

static bool UpdateErrorMonitor(uint8_t channel, uint16_t error)
{
    ProductSensorMeasurementState_t *state = &g_measurement_state[channel];
    product_temperature_input_monitor_t monitor;

    monitor.unfilteredProcessValue = state->snapshot.valid ?
        state->unfiltered_value : 0.0F;
    monitor.filteredProcessValue = state->filter_initialized ?
        state->filtered_value : 0.0F;
    monitor.inputError = error;
    state->snapshot.input_error = error;
    return ProductModbusRegisterAdapter_SetTemperatureInputMonitorForChannel(
        channel, &monitor);
}

void ProductSensorMeasurementService_Initialize(void)
{
    uint8_t channel;

    (void)memset(g_measurement_state, 0, sizeof(g_measurement_state));
    for (channel = 0U; channel < PRODUCT_SENSOR_INPUT_COUNT; channel++)
    {
        g_measurement_state[channel].last_sensor_type =
            PRODUCT_SENSOR_TYPE_OFF;
        g_measurement_state[channel].snapshot.input_error =
            PRODUCT_INPUT_ERROR_MEASUREMENT;
    }
}

bool ProductSensorMeasurementService_Process(uint8_t channel)
{
    ProductSensorMeasurementState_t *state;
    product_temperature_input_config_t product_configuration;
    SensorConversionConfig_t conversion_configuration;
    FactoryCalibrationProfile_t calibration_profile;
    HalAdcFactoryCalibration_t calibration;
    AnalogInputSample_t sample;
    SensorConversionResult_t result;
    product_temperature_input_monitor_t monitor;
    int32_t calibrated_uv;
    int32_t cjc_millicelsius = 0L;
    float unfiltered;
    float time_constant;
    float sample_period;

    if ((channel >= PRODUCT_SENSOR_INPUT_COUNT) ||
        !ProductModbusRegisterAdapter_GetTemperatureInputConfigForChannel(
            channel, &product_configuration))
    {
        return false;
    }
    state = &g_measurement_state[channel];
    if (state->last_sensor_type != product_configuration.sensorType)
    {
        state->last_sensor_type = product_configuration.sensorType;
        state->last_sample_sequence = 0U;
        state->filter_initialized = false;
        state->snapshot.valid = false;
    }
    if (product_configuration.sensorType == PRODUCT_SENSOR_TYPE_OFF)
    {
        (void)memset(&state->snapshot, 0, sizeof(state->snapshot));
        state->snapshot.sensor_type = PRODUCT_SENSOR_TYPE_OFF;
        state->snapshot.input_error = PRODUCT_INPUT_ERROR_NONE;
        monitor.unfilteredProcessValue = 0.0F;
        monitor.filteredProcessValue = 0.0F;
        monitor.inputError = PRODUCT_INPUT_ERROR_NONE;
        return ProductModbusRegisterAdapter_SetTemperatureInputMonitorForChannel(
            channel, &monitor);
    }
    if (!AnalogInputService_GetLatestByInput(channel, &sample))
    {
        return UpdateErrorMonitor(channel, PRODUCT_INPUT_ERROR_MEASUREMENT);
    }
    if ((state->last_sample_sequence == sample.sequence) &&
        state->snapshot.valid)
    {
        return true;
    }
    if (!ProductSensorMeasurementService_ResolveConfiguration(
            product_configuration.sensorType, &conversion_configuration,
            &calibration_profile))
    {
        state->last_sample_sequence = sample.sequence;
        return UpdateErrorMonitor(channel, PRODUCT_INPUT_ERROR_FAIL);
    }
    if (!FactoryCalibrationService_GetCalibration(
            channel, calibration_profile, &calibration) ||
        !HalAdcMeasurement_ApplyFactoryCalibration(
            sample.microvolts, &calibration, &calibrated_uv))
    {
        state->last_sample_sequence = sample.sequence;
        return UpdateErrorMonitor(channel,
                                  PRODUCT_INPUT_ERROR_BAD_CALIBRATION);
    }
    state->snapshot.raw_code = sample.raw_code;
    state->snapshot.uncalibrated_uv = sample.microvolts;
    state->snapshot.calibrated_uv = calibrated_uv;
    state->snapshot.sample_sequence = sample.sequence;
    state->snapshot.sensor_type = product_configuration.sensorType;
    state->snapshot.conversion_flags = 0UL;
    state->snapshot.valid = false;
    if (SensorConversionService_IsThermocouple(
            conversion_configuration.type))
    {
        uint8_t cjc_index = (channel < 2U) ? 0U : 1U;
        int32_t cjc_temperature_centi_c;
        if (!BspInternalAdc_GetCjcTemperature(
                cjc_index, &cjc_temperature_centi_c))
        {
            state->snapshot.conversion_flags = SENSOR_FLAG_CJC_ERROR;
            return UpdateErrorMonitor(channel, PRODUCT_INPUT_ERROR_AMBIENT);
        }
        state->snapshot.cjc_temperature_centi_c =
            cjc_temperature_centi_c;
        cjc_millicelsius = cjc_temperature_centi_c * 10L;
    }
    if (!SensorConversionService_Convert(
            &conversion_configuration, calibrated_uv,
            cjc_millicelsius, &result))
    {
        state->last_sample_sequence = sample.sequence;
        state->snapshot.conversion_flags = result.flags;
        return UpdateErrorMonitor(
            channel,
            ConversionError(conversion_configuration.type, result.flags));
    }

    unfiltered = IsTemperatureType(conversion_configuration.type) ?
        ((float)result.engineering_value / 1000.0F) :
        ((float)result.engineering_value / 10.0F);
    state->unfiltered_value = unfiltered;
    time_constant = product_configuration.filterTimeConstantSeconds;
    sample_period = 1.0F / (float)PRODUCT_ADC_OUTPUT_DATA_RATE_HZ;
    if (!state->filter_initialized || (time_constant <= 0.0F))
    {
        state->filtered_value = unfiltered;
        state->filter_initialized = true;
    }
    else
    {
        float alpha = sample_period / (time_constant + sample_period);
        state->filtered_value +=
            alpha * (unfiltered - state->filtered_value);
    }

    state->last_sample_sequence = sample.sequence;
    state->snapshot.engineering_value = result.engineering_value;
    state->snapshot.conversion_flags = result.flags;
    state->snapshot.input_error =
        (result.flags == 0UL) ? PRODUCT_INPUT_ERROR_NONE :
        ConversionError(conversion_configuration.type, result.flags);
    state->snapshot.valid = true;

    monitor.unfilteredProcessValue = unfiltered;
    monitor.filteredProcessValue = state->filtered_value;
    monitor.inputError = state->snapshot.input_error;
    return ProductModbusRegisterAdapter_SetTemperatureInputMonitorForChannel(
        channel, &monitor);
}

bool ProductSensorMeasurementService_GetSnapshot(
    uint8_t channel,
    ProductSensorMeasurementSnapshot_t *snapshot)
{
    if ((channel >= PRODUCT_SENSOR_INPUT_COUNT) || (snapshot == NULL))
    {
        return false;
    }
    *snapshot = g_measurement_state[channel].snapshot;
    return true;
}
