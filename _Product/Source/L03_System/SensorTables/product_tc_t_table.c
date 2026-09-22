#include "product_tc_t_table.h"

#include <stddef.h>

#if PRODUCT_TC_T_TABLE_COMPLETE

#if (PRODUCT_TC_T_MEASUREMENT_SEGMENT_COUNT == 0U)
#error "Enter the T measurement segment count"
#endif

typedef struct
{
    int32_t slope;
    int32_t intercept_millicelsius;
} ProductTcTMeasurementCoefficient_t;

typedef struct
{
    int32_t slope;
    int32_t intercept_deci_microvolts;
} ProductTcTCjcCoefficient_t;

/*
 * TODO: USER TABLE DATA
 * Enter MEASUREMENT_SEGMENT_COUNT + 1 strictly increasing boundaries.
 * Each value is input_uV + PRODUCT_TC_T_INPUT_SHIFT_UV.
 */
static const int32_t s_tc_t_measurement_boundaries_shifted_uv
    [PRODUCT_TC_T_MEASUREMENT_BOUNDARY_COUNT] =
{
    /* TODO: boundary_0, boundary_1, ... boundary_N */
   97,  439,  835, 1281, 1777, 2321, 2912, 3547, 4225, 4943,//10
    5700, 6490, 7312, 8168, 9058, 9979, 10928, 11906, 12909, 13937, //20
    14988, 16062, 17158, 18274, 19409, 20562, 21732, 22919, 24122, 25341, //30
    26572,
};

/*
 * TODO: USER TABLE DATA
 * temperature_mC = slope * shifted_uV / 100 + intercept_mC
 */
static const ProductTcTMeasurementCoefficient_t
    s_tc_t_measurement_coefficients[PRODUCT_TC_T_MEASUREMENT_SEGMENT_COUNT] =
{
    /* TODO: {slope_0, intercept_0}, ... {slope_N-1, intercept_N-1} */
  {5848, -205468}, {5051, -201997}, {4484, -197290}, {4032, -191525}, {3676, -185212}, //5
    {3384, -178440}, {3150, -171630}, {2950, -164560}, {2786, -157638}, {2642, -150531}, //10
    {2532, -144274}, {2433, -137841}, {2336, -130756}, {2247, -123484}, {2172, -116692}, //15
    {2107, -110214}, {2045, -103445}, {1994, -97369}, {1946, -91175}, {1903, -85188}, //20
    {1862, -79048}, {1825, -73106}, {1792, -67444}, {1762, -61963}, {1735, -56725}, //25
    {1709, -51382}, {1685, -46166}, {1663, -41130}, {1641, -35828}, {1625, -31776}, //30

};

/*
 * TODO: USER TABLE DATA
 * shifted_temperature_deci_C = temperature_deci_C + 200
 * cjc_deci_uV = slope * shifted_temperature_deci_C + intercept_deci_uV
 */
static const ProductTcTCjcCoefficient_t
    s_tc_t_cjc_coefficients[PRODUCT_TC_T_CJC_SEGMENT_COUNT] =
{
    /* TODO: 13 rows: {slope_0, intercept_0}, ... {slope_12, intercept_12} */
    {374, -75770}, {383, -76675}, {391, -78235}, {399, -80675}, {406, -83470},
    {416, -88440}, {424, -93280}, {432, -98850}, {441, -106075}, {449, -113280},
    {456, -120270}, {465, -130175}, {471, -137375},
};

static PiecewiseLinearSegment_t
    s_tc_t_measurement_segments[PRODUCT_TC_T_MEASUREMENT_SEGMENT_COUNT];
static PiecewiseLinearSegment_t
    s_tc_t_cjc_segments[PRODUCT_TC_T_CJC_SEGMENT_COUNT];
static bool s_tables_initialized;

static const PiecewiseLinearTable_t s_tc_t_measurement_table =
{
    s_tc_t_measurement_segments,
    PRODUCT_TC_T_MEASUREMENT_SEGMENT_COUNT,
    PRODUCT_TC_T_MEASUREMENT_COEFFICIENT_SCALE
};

static const PiecewiseLinearTable_t s_tc_t_cjc_table =
{
    s_tc_t_cjc_segments,
    PRODUCT_TC_T_CJC_SEGMENT_COUNT,
    PRODUCT_TC_T_CJC_COEFFICIENT_SCALE
};

static void InitializeTables(void)
{
    uint16_t index;

    if (s_tables_initialized)
    {
        return;
    }

    for (index = 0U; index < PRODUCT_TC_T_MEASUREMENT_SEGMENT_COUNT; index++)
    {
        int32_t slope = s_tc_t_measurement_coefficients[index].slope;
        s_tc_t_measurement_segments[index].x_min =
            s_tc_t_measurement_boundaries_shifted_uv[index] -
            PRODUCT_TC_T_INPUT_SHIFT_UV;
        s_tc_t_measurement_segments[index].x_max =
            s_tc_t_measurement_boundaries_shifted_uv[index + 1U] -
            PRODUCT_TC_T_INPUT_SHIFT_UV;
        s_tc_t_measurement_segments[index].slope = slope;
        s_tc_t_measurement_segments[index].intercept =
            ((int64_t)slope * PRODUCT_TC_T_INPUT_SHIFT_UV) +
            ((int64_t)s_tc_t_measurement_coefficients[index]
                 .intercept_millicelsius *
             PRODUCT_TC_T_MEASUREMENT_COEFFICIENT_SCALE);
    }

    for (index = 0U; index < PRODUCT_TC_T_CJC_SEGMENT_COUNT; index++)
    {
        int32_t slope = s_tc_t_cjc_coefficients[index].slope;
        s_tc_t_cjc_segments[index].x_min =
            PRODUCT_TC_T_CJC_MIN_MILLICELSIUS +
            ((int32_t)index * PRODUCT_TC_T_CJC_INTERVAL_MC);
        s_tc_t_cjc_segments[index].x_max =
            s_tc_t_cjc_segments[index].x_min + PRODUCT_TC_T_CJC_INTERVAL_MC;
        s_tc_t_cjc_segments[index].slope = slope;
        s_tc_t_cjc_segments[index].intercept =
            (((int64_t)slope * PRODUCT_TC_T_CJC_INPUT_SHIFT_DECICELSIUS) +
             s_tc_t_cjc_coefficients[index].intercept_deci_microvolts) *
            100LL;
    }

    s_tables_initialized = true;
}

const PiecewiseLinearTable_t *ProductTcTTable_GetMeasurementTable(void)
{
    InitializeTables();
    return &s_tc_t_measurement_table;
}

const PiecewiseLinearTable_t *ProductTcTTable_GetCjcTable(void)
{
    InitializeTables();
    return &s_tc_t_cjc_table;
}

bool ProductTcTTable_IsReady(void)
{
    InitializeTables();
    return PiecewiseLinearTable_IsValid(&s_tc_t_measurement_table) &&
           PiecewiseLinearTable_IsValid(&s_tc_t_cjc_table);
}

#else

const PiecewiseLinearTable_t *ProductTcTTable_GetMeasurementTable(void)
{
    return NULL;
}

const PiecewiseLinearTable_t *ProductTcTTable_GetCjcTable(void)
{
    return NULL;
}

bool ProductTcTTable_IsReady(void)
{
    return false;
}

#endif /* PRODUCT_TC_T_TABLE_COMPLETE */

