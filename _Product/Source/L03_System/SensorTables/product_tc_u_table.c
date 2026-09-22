#include "product_tc_u_table.h"

#include <stddef.h>

#if PRODUCT_TC_U_TABLE_COMPLETE

#if (PRODUCT_TC_U_MEASUREMENT_SEGMENT_COUNT == 0U)
#error "The U measurement segment count must not be zero"
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

/* Values are input_uV + PRODUCT_TC_U_INPUT_SHIFT_UV. */
static const int32_t s_tc_u_measurement_boundaries_shifted_uv
    [PRODUCT_TC_U_MEASUREMENT_BOUNDARY_COUNT] =
{
    100, 480, 890, 1340, 1850, 2400, 2990, 3620, 4300, 5030,
    5800, 6600, 7430, 8280, 9150, 10050, 10980, 11930, 12920, 13950,
    15000, 16090, 17210, 18350, 19510, 20700, 21900, 23110, 24330, 25560,
    26800, 28050, 29310, 30590, 31890, 33210, 34550, 35910, 37290, 38690,
    40110,
};

/* temperature_mC = slope * shifted_uV / scale + intercept_mC. */
static const ProductTcUMeasurementCoefficient_t
    s_tc_u_measurement_coefficients[PRODUCT_TC_U_MEASUREMENT_SEGMENT_COUNT] =
{
    {5263, -205263}, {4878, -203292}, {4444, -199440}, {3922, -192457}, {3636, -187175},
    {3390, -181274}, {3175, -174848}, {2941, -166317}, {2740, -157752}, {2597, -150563},
    {2500, -145000}, {2410, -139001}, {2353, -134769}, {2299, -130300}, {2222, -123312},
    {2151, -116123}, {2105, -111076}, {2020, -100935}, {1942, -90859}, {1905, -85701},
    {1835, -75205}, {1786, -67369}, {1754, -61861}, {1724, -56353}, {1681, -47922},
    {1667, -45071}, {1653, -41966}, {1639, -38771}, {1626, -35565}, {1613, -32283},
    {1600, -28760}, {1587, -25152}, {1563, -18118}, {1538, -10471}, {1515, -3133},
    {1493, 4172}, {1471, 11767}, {1449, 19666}, {1429, 27123}, {1408, 35248},
};

/* cjc_deci_uV = slope * shifted_temperature_deci_C + intercept. */
static const ProductTcUCjcCoefficient_t
    s_tc_u_cjc_coefficients[PRODUCT_TC_U_CJC_SEGMENT_COUNT] =
{
    {380, -77000}, {390, -78000}, {400, -80000}, {400, -80000}, {410, -84000},
    {420, -89000}, {420, -89000}, {430, -96000}, {430, -96000}, {440, -105000},
    {450, -115000}, {450, -115005}, {460, -127010},
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
