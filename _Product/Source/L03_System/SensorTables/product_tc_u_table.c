#include "product_tc_u_table.h"

#include <stddef.h>

#if PRODUCT_TC_U_TABLE_COMPLETE

#if (PRODUCT_TC_U_MEASUREMENT_SEGMENT_COUNT == 0U)
#error "Enter the U measurement segment count"
#endif

typedef struct
{
    int32_t slope;
    int32_t intercept_millicelsius;
} ProductTcUMeasurementCoefficient_t;

typedef struct
{
    int32_t slope;
    int32_t intercept_deci_microvolts;
} ProductTcUCjcCoefficient_t;

/*
 * TODO: USER TABLE DATA
 * Enter MEASUREMENT_SEGMENT_COUNT + 1 strictly increasing boundaries.
 * Each value is input_uV + PRODUCT_TC_U_INPUT_SHIFT_UV.
 */
static const int32_t s_tc_u_measurement_boundaries_shifted_uv
    [PRODUCT_TC_U_MEASUREMENT_BOUNDARY_COUNT] =
{
    /* TODO: boundary_0, boundary_1, ... boundary_N */
};

/*
 * TODO: USER TABLE DATA
 * temperature_mC = slope * shifted_uV / 100 + intercept_mC
 */
static const ProductTcUMeasurementCoefficient_t
    s_tc_u_measurement_coefficients[PRODUCT_TC_U_MEASUREMENT_SEGMENT_COUNT] =
{
    /* TODO: {slope_0, intercept_0}, ... {slope_N-1, intercept_N-1} */
};

/*
 * TODO: USER TABLE DATA
 * shifted_temperature_deci_C = temperature_deci_C + 200
 * cjc_deci_uV = slope * shifted_temperature_deci_C + intercept_deci_uV
 */
static const ProductTcUCjcCoefficient_t
    s_tc_u_cjc_coefficients[PRODUCT_TC_U_CJC_SEGMENT_COUNT] =
{
    /* TODO: 13 rows: {slope_0, intercept_0}, ... {slope_12, intercept_12} */
};

static PiecewiseLinearSegment_t
    s_tc_u_measurement_segments[PRODUCT_TC_U_MEASUREMENT_SEGMENT_COUNT];
static PiecewiseLinearSegment_t
    s_tc_u_cjc_segments[PRODUCT_TC_U_CJC_SEGMENT_COUNT];
static bool s_tables_initialized;

static const PiecewiseLinearTable_t s_tc_u_measurement_table =
{
    s_tc_u_measurement_segments,
    PRODUCT_TC_U_MEASUREMENT_SEGMENT_COUNT,
    PRODUCT_TC_U_MEASUREMENT_COEFFICIENT_SCALE
};

static const PiecewiseLinearTable_t s_tc_u_cjc_table =
{
    s_tc_u_cjc_segments,
    PRODUCT_TC_U_CJC_SEGMENT_COUNT,
    PRODUCT_TC_U_CJC_COEFFICIENT_SCALE
};

static void InitializeTables(void)
{
    uint16_t index;

    if (s_tables_initialized)
    {
        return;
    }

    for (index = 0U; index < PRODUCT_TC_U_MEASUREMENT_SEGMENT_COUNT; index++)
    {
        int32_t slope = s_tc_u_measurement_coefficients[index].slope;
        s_tc_u_measurement_segments[index].x_min =
            s_tc_u_measurement_boundaries_shifted_uv[index] -
            PRODUCT_TC_U_INPUT_SHIFT_UV;
        s_tc_u_measurement_segments[index].x_max =
            s_tc_u_measurement_boundaries_shifted_uv[index + 1U] -
            PRODUCT_TC_U_INPUT_SHIFT_UV;
        s_tc_u_measurement_segments[index].slope = slope;
        s_tc_u_measurement_segments[index].intercept =
            ((int64_t)slope * PRODUCT_TC_U_INPUT_SHIFT_UV) +
            ((int64_t)s_tc_u_measurement_coefficients[index]
                 .intercept_millicelsius *
             PRODUCT_TC_U_MEASUREMENT_COEFFICIENT_SCALE);
    }

    for (index = 0U; index < PRODUCT_TC_U_CJC_SEGMENT_COUNT; index++)
    {
        int32_t slope = s_tc_u_cjc_coefficients[index].slope;
        s_tc_u_cjc_segments[index].x_min =
            PRODUCT_TC_U_CJC_MIN_MILLICELSIUS +
            ((int32_t)index * PRODUCT_TC_U_CJC_INTERVAL_MC);
        s_tc_u_cjc_segments[index].x_max =
            s_tc_u_cjc_segments[index].x_min + PRODUCT_TC_U_CJC_INTERVAL_MC;
        s_tc_u_cjc_segments[index].slope = slope;
        s_tc_u_cjc_segments[index].intercept =
            (((int64_t)slope * PRODUCT_TC_U_CJC_INPUT_SHIFT_DECICELSIUS) +
             s_tc_u_cjc_coefficients[index].intercept_deci_microvolts) *
            100LL;
    }

    s_tables_initialized = true;
}

const PiecewiseLinearTable_t *ProductTcUTable_GetMeasurementTable(void)
{
    InitializeTables();
    return &s_tc_u_measurement_table;
}

const PiecewiseLinearTable_t *ProductTcUTable_GetCjcTable(void)
{
    InitializeTables();
    return &s_tc_u_cjc_table;
}

bool ProductTcUTable_IsReady(void)
{
    InitializeTables();
    return PiecewiseLinearTable_IsValid(&s_tc_u_measurement_table) &&
           PiecewiseLinearTable_IsValid(&s_tc_u_cjc_table);
}

#else

const PiecewiseLinearTable_t *ProductTcUTable_GetMeasurementTable(void)
{
    return NULL;
}

const PiecewiseLinearTable_t *ProductTcUTable_GetCjcTable(void)
{
    return NULL;
}

bool ProductTcUTable_IsReady(void)
{
    return false;
}

#endif /* PRODUCT_TC_U_TABLE_COMPLETE */

