#include "product_tc_l_table.h"

#include <stddef.h>

#if PRODUCT_TC_L_TABLE_COMPLETE

#if (PRODUCT_TC_L_MEASUREMENT_SEGMENT_COUNT == 0U)
#error "The L measurement segment count must not be zero"
#endif

typedef struct
{
    int32_t slope;
    int32_t intercept_millicelsius;
} ProductTcLMeasurementCoefficient_t;

typedef struct
{
    int32_t slope;
    int32_t intercept_deci_microvolts;
} ProductTcLCjcCoefficient_t;

/* Values are input_uV + PRODUCT_TC_L_INPUT_SHIFT_UV. */
static const int32_t s_tc_l_measurement_boundaries_shifted_uv
    [PRODUCT_TC_L_MEASUREMENT_BOUNDARY_COUNT] =
{
    20, 600, 1190, 1820, 2490, 3220, 4010, 4860, 5770, 6720,
    7730, 8750, 9800, 10860, 11940, 13020, 14120, 15220, 16340, 17460,
    18580, 19700, 20820, 21940, 23060, 24190, 25310, 26430, 27550, 28670,
    29790, 30910, 32040, 33180, 34320, 35460, 36600, 37760, 38920, 40080,
    41240, 42420, 43600, 44790, 46000, 47220, 48470, 49730, 51020, 52320,
    53640, 54970, 56320, 57690, 59070, 60450,
};

/* temperature_mC = slope * shifted_uV / scale + intercept_mC. */
static const ProductTcLMeasurementCoefficient_t
    s_tc_l_measurement_coefficients[PRODUCT_TC_L_MEASUREMENT_SEGMENT_COUNT] =
{
    {3448, -220689}, {3390, -220255}, {3175, -217704}, {2985, -214252}, {2740, -208158},
    {2532, -201329}, {2353, -194167}, {2198, -186769}, {2105, -181405}, {1980, -173006},
    {1961, -171586}, {1905, -166640}, {1887, -164927}, {1852, -161128}, {1852, -161130},
    {1818, -156703}, {1818, -156701}, {1786, -151831}, {1786, -151834}, {1786, -151837},
    {1786, -151840}, {1786, -151844}, {1786, -151847}, {1786, -151850}, {1770, -148207},
    {1786, -152035}, {1786, -152038}, {1786, -152041}, {1786, -152045}, {1786, -152048},
    {1786, -152051}, {1770, -147063}, {1754, -141980}, {1754, -141975}, {1754, -141971},
    {1754, -141966}, {1724, -130983}, {1724, -130982}, {1724, -130980}, {1724, -130978},
    {1695, -119018}, {1695, -119019}, {1681, -112875}, {1653, -100338}, {1639, -93857},
    {1600, -75480}, {1587, -69217}, {1550, -50775}, {1538, -44685}, {1515, -32647},
    {1504, -26709}, {1481, -14070}, {1471, -8505}, {1449, 4074}, {1449, 4078},
};

/* cjc_deci_uV = slope * shifted_temperature_deci_C + intercept. */
static const ProductTcLCjcCoefficient_t
    s_tc_l_cjc_coefficients[PRODUCT_TC_L_CJC_SEGMENT_COUNT] =
{
    {510, -102000}, {510, -102000}, {520, -104000}, {530, -107000}, {530, -107000},
    {530, -107000}, {540, -113000}, {540, -113000}, {540, -113000}, {540, -113000},
    {550, -123000}, {550, -123005}, {550, -123010},
};

static PiecewiseLinearSegment_t
    s_tc_l_measurement_segments[PRODUCT_TC_L_MEASUREMENT_SEGMENT_COUNT];
static PiecewiseLinearSegment_t
    s_tc_l_cjc_segments[PRODUCT_TC_L_CJC_SEGMENT_COUNT];
static bool s_tables_initialized;

static const PiecewiseLinearTable_t s_tc_l_measurement_table =
{
    s_tc_l_measurement_segments,
    PRODUCT_TC_L_MEASUREMENT_SEGMENT_COUNT,
    PRODUCT_TC_L_MEASUREMENT_COEFFICIENT_SCALE
};

static const PiecewiseLinearTable_t s_tc_l_cjc_table =
{
    s_tc_l_cjc_segments,
    PRODUCT_TC_L_CJC_SEGMENT_COUNT,
    PRODUCT_TC_L_CJC_COEFFICIENT_SCALE
};

static void InitializeTables(void)
{
    uint16_t index;

    if (s_tables_initialized)
    {
        return;
    }

    for (index = 0U; index < PRODUCT_TC_L_MEASUREMENT_SEGMENT_COUNT; index++)
    {
        int32_t slope = s_tc_l_measurement_coefficients[index].slope;
        s_tc_l_measurement_segments[index].x_min =
            s_tc_l_measurement_boundaries_shifted_uv[index] -
            PRODUCT_TC_L_INPUT_SHIFT_UV;
        s_tc_l_measurement_segments[index].x_max =
            s_tc_l_measurement_boundaries_shifted_uv[index + 1U] -
            PRODUCT_TC_L_INPUT_SHIFT_UV;
        s_tc_l_measurement_segments[index].slope = slope;
        s_tc_l_measurement_segments[index].intercept =
            ((int64_t)slope * PRODUCT_TC_L_INPUT_SHIFT_UV) +
            ((int64_t)s_tc_l_measurement_coefficients[index]
                 .intercept_millicelsius *
             PRODUCT_TC_L_MEASUREMENT_COEFFICIENT_SCALE);
    }

    for (index = 0U; index < PRODUCT_TC_L_CJC_SEGMENT_COUNT; index++)
    {
        int32_t slope = s_tc_l_cjc_coefficients[index].slope;
        s_tc_l_cjc_segments[index].x_min =
            PRODUCT_TC_L_CJC_MIN_MILLICELSIUS +
            ((int32_t)index * PRODUCT_TC_L_CJC_INTERVAL_MC);
        s_tc_l_cjc_segments[index].x_max =
            s_tc_l_cjc_segments[index].x_min + PRODUCT_TC_L_CJC_INTERVAL_MC;
        s_tc_l_cjc_segments[index].slope = slope;
        s_tc_l_cjc_segments[index].intercept =
            (((int64_t)slope * PRODUCT_TC_L_CJC_INPUT_SHIFT_DECICELSIUS) +
             s_tc_l_cjc_coefficients[index].intercept_deci_microvolts) *
            100LL;
    }

    s_tables_initialized = true;
}

const PiecewiseLinearTable_t *ProductTcLTable_GetMeasurementTable(void)
{
    InitializeTables();
    return &s_tc_l_measurement_table;
}

const PiecewiseLinearTable_t *ProductTcLTable_GetCjcTable(void)
{
    InitializeTables();
    return &s_tc_l_cjc_table;
}

bool ProductTcLTable_IsReady(void)
{
    InitializeTables();
    return PiecewiseLinearTable_IsValid(&s_tc_l_measurement_table) &&
           PiecewiseLinearTable_IsValid(&s_tc_l_cjc_table);
}

#else

const PiecewiseLinearTable_t *ProductTcLTable_GetMeasurementTable(void)
{
    return NULL;
}

const PiecewiseLinearTable_t *ProductTcLTable_GetCjcTable(void)
{
    return NULL;
}

bool ProductTcLTable_IsReady(void)
{
    return false;
}

#endif /* PRODUCT_TC_L_TABLE_COMPLETE */
