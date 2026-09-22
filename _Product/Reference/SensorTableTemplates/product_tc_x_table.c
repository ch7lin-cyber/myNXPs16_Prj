#include "product_tc_x_table.h"

#include <stddef.h>

/*
 * Keep PRODUCT_TC_X_TABLE_COMPLETE at 0 until every TODO table entry has
 * been supplied and checked. The getters return NULL while incomplete, so an
 * unfinished sensor table cannot be selected by the runtime conversion code.
 */
#if PRODUCT_TC_X_TABLE_COMPLETE

#if (PRODUCT_TC_X_MEASUREMENT_SEGMENT_COUNT == 0U)
#error "Enter the TC measurement segment count"
#endif

#if (PRODUCT_TC_X_CJC_SEGMENT_COUNT == 0U)
#error "Enter the TC CJC segment count"
#endif

typedef struct
{
    int32_t slope;
    int32_t intercept_millicelsius;
} ProductTcXMeasurementCoefficient_t;

typedef struct
{
    int32_t slope;
    int32_t intercept_deci_microvolts;
} ProductTcXCjcCoefficient_t;

/*
 * TODO: USER TABLE DATA - measurement boundaries.
 *
 * Store shifted values:
 *     shifted_uV = input_uV + PRODUCT_TC_X_INPUT_SHIFT_UV
 *
 * Requirements:
 * - exactly MEASUREMENT_SEGMENT_COUNT + 1 entries;
 * - strictly increasing;
 * - first and last entries cover the extended temperature range.
 */
static const int32_t s_tc_x_measurement_boundaries_shifted_uv
    [PRODUCT_TC_X_MEASUREMENT_BOUNDARY_COUNT] =
{
    /* TODO: boundary_0, boundary_1, ... boundary_N */
};

/*
 * TODO: USER TABLE DATA - measurement coefficients.
 *
 * One {a, b} row for every interval above:
 *     temperature_mC = a * shifted_uV / 100 + b_mC
 */
static const ProductTcXMeasurementCoefficient_t
    s_tc_x_measurement_coefficients[PRODUCT_TC_X_MEASUREMENT_SEGMENT_COUNT] =
{
    /* TODO: {a_0, b_0}, {a_1, b_1}, ... {a_N-1, b_N-1} */
};

/*
 * TODO: USER TABLE DATA - cold-junction coefficients.
 *
 * Input temperature is first shifted by +20.0 degree C:
 *     shifted_temperature_deci_C = temperature_deci_C + 200
 *
 * One {a, b} row for every 10 degree C interval:
 *     cjc_deci_uV = a * shifted_temperature_deci_C + b_deci_uV
 */
static const ProductTcXCjcCoefficient_t
    s_tc_x_cjc_coefficients[PRODUCT_TC_X_CJC_SEGMENT_COUNT] =
{
    /* TODO: {a_0, b_0}, {a_1, b_1}, ... {a_N-1, b_N-1} */
};

static PiecewiseLinearSegment_t
    s_tc_x_measurement_segments[PRODUCT_TC_X_MEASUREMENT_SEGMENT_COUNT];
static PiecewiseLinearSegment_t
    s_tc_x_cjc_segments[PRODUCT_TC_X_CJC_SEGMENT_COUNT];
static bool s_tables_initialized;

static const PiecewiseLinearTable_t s_tc_x_measurement_table =
{
    s_tc_x_measurement_segments,
    PRODUCT_TC_X_MEASUREMENT_SEGMENT_COUNT,
    PRODUCT_TC_X_MEASUREMENT_COEFFICIENT_SCALE
};

static const PiecewiseLinearTable_t s_tc_x_cjc_table =
{
    s_tc_x_cjc_segments,
    PRODUCT_TC_X_CJC_SEGMENT_COUNT,
    PRODUCT_TC_X_CJC_COEFFICIENT_SCALE
};

static void InitializeTables(void)
{
    uint16_t index;

    if (s_tables_initialized)
    {
        return;
    }

    for (index = 0U; index < PRODUCT_TC_X_MEASUREMENT_SEGMENT_COUNT; index++)
    {
        int32_t slope = s_tc_x_measurement_coefficients[index].slope;
        s_tc_x_measurement_segments[index].x_min =
            s_tc_x_measurement_boundaries_shifted_uv[index] -
            PRODUCT_TC_X_INPUT_SHIFT_UV;
        s_tc_x_measurement_segments[index].x_max =
            s_tc_x_measurement_boundaries_shifted_uv[index + 1U] -
            PRODUCT_TC_X_INPUT_SHIFT_UV;
        s_tc_x_measurement_segments[index].slope = slope;
        s_tc_x_measurement_segments[index].intercept =
            ((int64_t)slope * PRODUCT_TC_X_INPUT_SHIFT_UV) +
            ((int64_t)s_tc_x_measurement_coefficients[index]
                 .intercept_millicelsius *
             PRODUCT_TC_X_MEASUREMENT_COEFFICIENT_SCALE);
    }

    for (index = 0U; index < PRODUCT_TC_X_CJC_SEGMENT_COUNT; index++)
    {
        int32_t slope = s_tc_x_cjc_coefficients[index].slope;
        s_tc_x_cjc_segments[index].x_min =
            PRODUCT_TC_X_CJC_MIN_MILLICELSIUS +
            ((int32_t)index * PRODUCT_TC_X_CJC_INTERVAL_MC);
        s_tc_x_cjc_segments[index].x_max =
            s_tc_x_cjc_segments[index].x_min +
            PRODUCT_TC_X_CJC_INTERVAL_MC;
        s_tc_x_cjc_segments[index].slope = slope;
        s_tc_x_cjc_segments[index].intercept =
            (((int64_t)slope * PRODUCT_TC_X_CJC_INPUT_SHIFT_DECICELSIUS) +
             s_tc_x_cjc_coefficients[index].intercept_deci_microvolts) *
            100LL;
    }

    s_tables_initialized = true;
}

const PiecewiseLinearTable_t *ProductTcXTable_GetMeasurementTable(void)
{
    InitializeTables();
    return &s_tc_x_measurement_table;
}

const PiecewiseLinearTable_t *ProductTcXTable_GetCjcTable(void)
{
    InitializeTables();
    return &s_tc_x_cjc_table;
}

bool ProductTcXTable_IsReady(void)
{
    InitializeTables();
    return PiecewiseLinearTable_IsValid(&s_tc_x_measurement_table) &&
           PiecewiseLinearTable_IsValid(&s_tc_x_cjc_table);
}

#else

const PiecewiseLinearTable_t *ProductTcXTable_GetMeasurementTable(void)
{
    return NULL;
}

const PiecewiseLinearTable_t *ProductTcXTable_GetCjcTable(void)
{
    return NULL;
}

bool ProductTcXTable_IsReady(void)
{
    return false;
}

#endif /* PRODUCT_TC_X_TABLE_COMPLETE */
