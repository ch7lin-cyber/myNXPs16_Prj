#include "product_sensor_measurement_service.h"
#include "ProductSensorMeasurementConfig.h"

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

#define PRODUCT_SENSOR_MEDIAN_WINDOW           (5U)
#define PRODUCT_SENSOR_INPUT_COUNT             (4U)
#define PRODUCT_INPUT_ERROR_FAIL               (32U)
#define PRODUCT_INPUT_ERROR_AMBIENT            (9U)
#define PRODUCT_INPUT_ERROR_BAD_CALIBRATION    (139U)
#define PRODUCT_INPUT_ERROR_MEASUREMENT        (140U)
#define PRODUCT_INPUT_ERROR_RTD                (141U)
#define PRODUCT_ANALOG_ENGINEERING_MIN         (-32000L)
#define PRODUCT_ANALOG_ENGINEERING_MAX         (32000L)

typedef struct
{
    ProductSensorMeasurementSnapshot_t snapshot;
    float unfiltered_value;
    float filtered_value;
    uint32_t last_sample_sequence;
    uint16_t last_sensor_type;
    uint16_t last_cjc_source;
    int16_t last_cjc_offset_centi_c;
    int32_t median_samples[PRODUCT_SENSOR_MEDIAN_WINDOW];
    uint32_t median_sequence;
    uint8_t median_next;
    bool median_initialized;
    bool filter_initialized;
    bool source_known;
    bool source_online;
    bool source_fault;
    bool has_seen_sequence;
    bool has_processed_sequence;
    bool source_time_valid;
    uint32_t seen_sequence;
    uint32_t last_source_ms;
} ProductSensorMeasurementState_t;

static ProductSensorMeasurementState_t
    g_measurement_state[PRODUCT_SENSOR_INPUT_COUNT];

/* Seed with the first sample to provide an immediate output without zero bias.
 * Only distinct ADC sequences advance the per-channel rolling window. */
static int32_t FilterMedian(ProductSensorMeasurementState_t *state,
                            int32_t value, uint32_t sequence)
{
    int32_t sorted[PRODUCT_SENSOR_MEDIAN_WINDOW];
    uint8_t i;
    if (!state->median_initialized)
    {
        for (i = 0U; i < PRODUCT_SENSOR_MEDIAN_WINDOW; i++)
        {
            state->median_samples[i] = value;
        }
        state->median_next = 0U;
        state->median_initialized = true;
    }
    else if (state->median_sequence != sequence)
    {
        state->median_samples[state->median_next] = value;
        state->median_next = (uint8_t)((state->median_next + 1U) %
                                      PRODUCT_SENSOR_MEDIAN_WINDOW);
    }
    state->median_sequence = sequence;
    (void)memcpy(sorted, state->median_samples, sizeof(sorted));
    for (i = 1U; i < PRODUCT_SENSOR_MEDIAN_WINDOW; i++)
    {
        int32_t key = sorted[i];
        uint8_t j = i;
        while ((j > 0U) && (sorted[j - 1U] > key))
        {
            sorted[j] = sorted[j - 1U];
            j--;
        }
        sorted[j] = key;
    }
    return sorted[PRODUCT_SENSOR_MEDIAN_WINDOW / 2U];
}

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
    /* OUT_OF_RANGE implies OUT_OF_SPEC; report the more severe condition. */
    if ((flags & SENSOR_FLAG_OUT_OF_RANGE) != 0UL)
    {
        return PRODUCT_INPUT_ERROR_OUT_OF_RANGE;
    }
    if ((flags & SENSOR_FLAG_OUT_OF_SPEC) != 0UL)
    {
        return PRODUCT_INPUT_ERROR_OUT_OF_SPEC;
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

    (void)ProductModbusRegisterAdapter_SetMeasurementQuality(channel,
        state->snapshot.quality_flags, state->snapshot.recovery_good_samples,
        state->snapshot.sample_sequence, state->snapshot.sample_age_ms,
        state->snapshot.quality_revision);
    monitor.unfilteredProcessValue = state->snapshot.valid ?
        state->unfiltered_value : 0.0F;
    monitor.filteredProcessValue = state->snapshot.valid && state->filter_initialized ?
        state->filtered_value : 0.0F;
    monitor.inputError = error;
    state->snapshot.input_error = error;
    return ProductModbusRegisterAdapter_SetTemperatureInputMonitorForChannel(
        channel, &monitor);
}

static void InvalidateMeasurement(ProductSensorMeasurementState_t *state,
                                  uint16_t flags)
{
    if (state->snapshot.valid || state->snapshot.recovery_good_samples != 0U ||
        state->snapshot.quality_flags != flags)
        ++state->snapshot.quality_revision;
    state->snapshot.valid = false;
    state->snapshot.quality_flags = flags;
    state->snapshot.recovery_good_samples = 0U;
    state->snapshot.engineering_value = 0;
    state->unfiltered_value = 0.0F;
    state->filtered_value = 0.0F;
    state->median_initialized = false;
    state->filter_initialized = false;
}

bool ProductSensorMeasurementService_SetSourceHealth(
    uint8_t channel, bool online, bool sample_fault)
{
    ProductSensorMeasurementState_t *state;
    if (channel >= PRODUCT_SENSOR_INPUT_COUNT) return false;
    state = &g_measurement_state[channel];
    state->source_known = true;
    state->source_online = online;
    state->source_fault = sample_fault;
    return true;
}

void ProductSensorMeasurementService_Initialize(void)
{
    uint8_t channel;

    (void)memset(g_measurement_state, 0, sizeof(g_measurement_state));
    for (channel = 0U; channel < PRODUCT_SENSOR_INPUT_COUNT; channel++)
    {
        g_measurement_state[channel].last_sensor_type =
            PRODUCT_SENSOR_TYPE_OFF;
        g_measurement_state[channel].snapshot.input_error = PRODUCT_INPUT_ERROR_MEASUREMENT;
        g_measurement_state[channel].snapshot.quality_flags =
            PRODUCT_SENSOR_QUALITY_NO_SAMPLE | PRODUCT_SENSOR_QUALITY_SOURCE_UNKNOWN;
        g_measurement_state[channel].snapshot.sample_age_ms = UINT32_MAX;
        (void)UpdateErrorMonitor(channel, PRODUCT_INPUT_ERROR_MEASUREMENT);
    }
}

static bool ProcessMeasurement(uint8_t channel, uint32_t now_ms, bool timed)
{
    ProductSensorMeasurementState_t *state;
    product_temperature_input_config_t product_configuration;
    SensorConversionConfig_t conversion_configuration;
    FactoryCalibrationProfile_t calibration_profile;
    AnalogInputSample_t sample;
    SensorConversionResult_t result;
    int32_t calibrated_uv;
    int32_t cjc_millicelsius = 0L;
    float unfiltered;
    float time_constant;
    float sample_period;

    if (channel >= PRODUCT_SENSOR_INPUT_COUNT) return false;
    state = &g_measurement_state[channel];
    if (!ProductModbusRegisterAdapter_GetTemperatureInputConfigForChannel(
            channel, &product_configuration))
    {
        InvalidateMeasurement(state, PRODUCT_SENSOR_QUALITY_CONVERSION);
        return UpdateErrorMonitor(channel, PRODUCT_INPUT_ERROR_FAIL);
    }
    if (state->last_sensor_type != product_configuration.sensorType ||
        state->last_cjc_source != product_configuration.cjcSource ||
        state->last_cjc_offset_centi_c !=
            product_configuration.cjcOffsetCentiC)
    {
        state->last_sensor_type = product_configuration.sensorType;
        state->last_cjc_source = product_configuration.cjcSource;
        state->last_cjc_offset_centi_c =
            product_configuration.cjcOffsetCentiC;
        InvalidateMeasurement(state, PRODUCT_SENSOR_QUALITY_RECOVERING);
        /* Reconfiguration cannot requalify an ADC sample from the old domain. */
        state->has_processed_sequence = state->has_seen_sequence;
        state->last_sample_sequence = state->seen_sequence;
        state->snapshot.input_error = PRODUCT_INPUT_ERROR_RECOVERING;
    }
    state->snapshot.sensor_type = product_configuration.sensorType;
    if (product_configuration.sensorType == PRODUCT_SENSOR_TYPE_OFF)
    {
        InvalidateMeasurement(state, PRODUCT_SENSOR_QUALITY_DISABLED);
        state->snapshot.sensor_type = PRODUCT_SENSOR_TYPE_OFF;
        state->snapshot.input_error = PRODUCT_INPUT_ERROR_NONE;
        return UpdateErrorMonitor(channel, PRODUCT_INPUT_ERROR_NONE);
    }
    if (!AnalogInputService_GetLatestByInput(channel, &sample))
    {
        state->snapshot.sample_age_ms = UINT32_MAX;
        InvalidateMeasurement(state, PRODUCT_SENSOR_QUALITY_NO_SAMPLE);
        return UpdateErrorMonitor(channel, PRODUCT_INPUT_ERROR_MEASUREMENT);
    }
    /* Check the old age before refreshing a new sequence. A foreground gap
     * must clear old filters even when a new sample is already available. */
    if ((!state->has_seen_sequence || state->seen_sequence != sample.sequence) &&
        timed && state->source_time_valid &&
        (uint32_t)(now_ms - state->last_source_ms) >= PRODUCT_SENSOR_STALE_TIMEOUT_MS)
        InvalidateMeasurement(state, PRODUCT_SENSOR_QUALITY_STALE);
    if (!state->has_seen_sequence || state->seen_sequence != sample.sequence)
    {
        state->has_seen_sequence = true;
        state->seen_sequence = sample.sequence;
        state->last_source_ms = now_ms;
        state->source_time_valid = timed;
    }
    state->snapshot.sample_age_ms = (timed && state->source_time_valid) ?
        (uint32_t)(now_ms - state->last_source_ms) : UINT32_MAX;
    if (!state->source_known || !state->source_online || state->source_fault)
    {
        state->has_processed_sequence = true;
        state->last_sample_sequence = sample.sequence; /* quarantine cached sample */
        uint16_t flags = state->source_known ? PRODUCT_SENSOR_QUALITY_SOURCE_FAULT :
                                               PRODUCT_SENSOR_QUALITY_SOURCE_UNKNOWN;
        if (timed && state->snapshot.sample_age_ms >= PRODUCT_SENSOR_STALE_TIMEOUT_MS)
            flags |= PRODUCT_SENSOR_QUALITY_STALE;
        InvalidateMeasurement(state, flags);
        return UpdateErrorMonitor(channel, PRODUCT_INPUT_ERROR_SOURCE_FAULT);
    }
    if (timed && state->snapshot.sample_age_ms >= PRODUCT_SENSOR_STALE_TIMEOUT_MS)
    {
        state->has_processed_sequence = true;
        state->last_sample_sequence = sample.sequence;
        InvalidateMeasurement(state, PRODUCT_SENSOR_QUALITY_STALE);
        return UpdateErrorMonitor(channel, PRODUCT_INPUT_ERROR_STALE);
    }
    if (state->has_processed_sequence && state->last_sample_sequence == sample.sequence)
        return UpdateErrorMonitor(channel, state->snapshot.input_error);
    state->has_processed_sequence = true;
    state->last_sample_sequence = sample.sequence;
    if (!ProductSensorMeasurementService_ResolveConfiguration(
            product_configuration.sensorType, &conversion_configuration,
            &calibration_profile))
    {
        state->last_sample_sequence = sample.sequence;
        InvalidateMeasurement(state, PRODUCT_SENSOR_QUALITY_CONVERSION);
        return UpdateErrorMonitor(channel, PRODUCT_INPUT_ERROR_FAIL);
    }
    if (!FactoryCalibrationService_Convert(channel, calibration_profile,
            sample.microvolts, &calibrated_uv))
    {
        state->last_sample_sequence = sample.sequence;
        InvalidateMeasurement(state, PRODUCT_SENSOR_QUALITY_CONVERSION);
        return UpdateErrorMonitor(channel, PRODUCT_INPUT_ERROR_BAD_CALIBRATION);
    }
    state->snapshot.raw_code = sample.raw_code;
    state->snapshot.uncalibrated_uv = sample.microvolts;
    state->snapshot.calibrated_uv = calibrated_uv;
    if (state->snapshot.sample_sequence != sample.sequence ||
        !state->snapshot.processed_timestamp_valid)
    {
        state->snapshot.processed_timestamp_ms = now_ms;
        state->snapshot.processed_timestamp_valid = timed;
    }
    state->snapshot.sample_sequence = sample.sequence;
    state->snapshot.sensor_type = product_configuration.sensorType;
    state->snapshot.conversion_flags = 0UL;
    state->snapshot.valid = false;
    if (SensorConversionService_IsThermocouple(
            conversion_configuration.type))
    {
        uint8_t cjc_index = (channel < 2U) ? 0U : 1U;
        int32_t cjc_temperature_centi_c = 0L;
        /* External providers are reserved: never silently use TMP20 for them. */
        if ((product_configuration.cjcSource > EVENT_CJC_SOURCE_INTERNAL_TMP20) ||
            ((product_configuration.cjcSource == EVENT_CJC_SOURCE_INTERNAL_TMP20) &&
             !BspInternalAdc_GetCjcTemperature(
                 cjc_index, &cjc_temperature_centi_c)))
        {
            state->snapshot.conversion_flags = SENSOR_FLAG_CJC_ERROR;
            InvalidateMeasurement(state, PRODUCT_SENSOR_QUALITY_CONVERSION);
            return UpdateErrorMonitor(channel, PRODUCT_INPUT_ERROR_AMBIENT);
        }
        cjc_temperature_centi_c += product_configuration.cjcOffsetCentiC;
        state->snapshot.cjc_temperature_centi_c = cjc_temperature_centi_c;
        cjc_millicelsius = cjc_temperature_centi_c * 10L;
    }
    conversion_configuration.cold_junction_compensation_disabled =
        (product_configuration.cjcSource == EVENT_CJC_SOURCE_DISABLED);
    if (!SensorConversionService_Convert(&conversion_configuration,
            calibrated_uv, cjc_millicelsius, &result) || result.flags != 0U)
    {
        state->snapshot.conversion_flags = result.flags;
        InvalidateMeasurement(state, PRODUCT_SENSOR_QUALITY_CONVERSION);
        return UpdateErrorMonitor(channel,
            ConversionError(conversion_configuration.type, result.flags));
    }
    if (!SensorConversionService_Convert(
            &conversion_configuration,
            FilterMedian(state, calibrated_uv, sample.sequence),
            cjc_millicelsius, &result) || result.flags != 0U)
    {
        state->last_sample_sequence = sample.sequence;
        state->snapshot.conversion_flags = result.flags;
        InvalidateMeasurement(state, PRODUCT_SENSOR_QUALITY_CONVERSION);
        return UpdateErrorMonitor(
            channel,
            ConversionError(conversion_configuration.type, result.flags));
    }

    if (state->snapshot.recovery_good_samples < PRODUCT_SENSOR_RECOVERY_GOOD_SAMPLES)
        ++state->snapshot.recovery_good_samples;
    if (state->snapshot.recovery_good_samples < PRODUCT_SENSOR_RECOVERY_GOOD_SAMPLES)
    {
        state->snapshot.quality_flags = PRODUCT_SENSOR_QUALITY_RECOVERING;
        return UpdateErrorMonitor(channel, PRODUCT_INPUT_ERROR_RECOVERING);
    }
    state->snapshot.quality_flags = 0U;
    unfiltered = IsTemperatureType(conversion_configuration.type) ?
        ((float)result.engineering_value / 1000.0F) :
        ((float)result.engineering_value);
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

    return UpdateErrorMonitor(channel, state->snapshot.input_error);
}

bool ProductSensorMeasurementService_Process(uint8_t channel)
{
    return ProcessMeasurement(channel, 0U, false);
}

bool ProductSensorMeasurementService_ProcessAt(uint8_t channel, uint32_t now_ms)
{
    return ProcessMeasurement(channel, now_ms, true);
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

