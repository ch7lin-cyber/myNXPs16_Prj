#include "product_tc_txk_table.h"

#include <stddef.h>

#if PRODUCT_TC_TXK_TABLE_COMPLETE

#if (PRODUCT_TC_TXK_MEASUREMENT_SEGMENT_COUNT == 0U)
#error "The TXK measurement segment count must not be zero"
#endif

typedef struct
{
    int32_t slope;
    int32_t intercept_millicelsius;
} ProductTcTxkMeasurementCoefficient_t;

typedef struct
{
    int32_t slope;
    int32_t intercept_deci_microvolts;
} ProductTcTxkCjcCoefficient_t;

/* Values are input_uV + PRODUCT_TC_TXK_INPUT_SHIFT_UV. */
static const int32_t s_tc_txk_measurement_boundaries_shifted_uv
    [PRODUCT_TC_TXK_MEASUREMENT_BOUNDARY_COUNT] =
{
    51, 597, 1238, 1970, 2787, 3683, 4654, 5693, 6796, 7957,
    9173, 10439, 11751, 13106, 14500, 15931, 17397, 18894, 20421, 21976,
    23558, 25163, 26790, 28439, 30106, 31791, 33491, 35205, 36932, 38668,
    40413, 42165, 43922, 45682, 47445, 49210, 50974, 52738, 54500, 56261,
    58020, 59776, 61531, 63284, 65035, 66783, 68528, 70268, 72000, 73721,
    75426,
};

/* temperature_mC = slope * shifted_uV / scale + intercept_mC. */
static const ProductTcTxkMeasurementCoefficient_t
    s_tc_txk_measurement_coefficients[PRODUCT_TC_TXK_MEASUREMENT_SEGMENT_COUNT] =
{
    {3663, -211643}, {3120, -208446}, {2732, -203671}, {2448, -198095}, {2232, -192094},
    {2060, -185756}, {1925, -179488}, {1813, -173143}, {1723, -167031}, {1645, -160841},
    {1580, -154884}, {1524, -149043}, {1476, -143404}, {1435, -138034}, {1398, -132680},
    {1364, -127268}, {1336, -122398}, {1310, -117489}, {1286, -112594}, {1264, -107755},
    {1246, -103510}, {1229, -99235}, {1213, -94947}, {1200, -91258}, {1187, -87345},
    {1176, -83848}, {1167, -80835}, {1158, -77667}, {1152, -75446}, {1146, -73126},
    {1142, -71512}, {1138, -69829}, {1136, -68947}, {1134, -68032}, {1133, -67551},
    {1134, -68041}, {1134, -68046}, {1135, -68576}, {1136, -69124}, {1137, -69687},
    {1139, -70848}, {1140, -71454}, {1141, -72072}, {1142, -72701}, {1144, -73997},
    {1146, -75330}, {1149, -77383}, {1155, -81600}, {1162, -86667}, {1173, -94763},
};

/* cjc_deci_uV = slope * shifted_temperature_deci_C + intercept. */
static const ProductTcTxkCjcCoefficient_t
    s_tc_txk_cjc_coefficients[PRODUCT_TC_TXK_CJC_SEGMENT_COUNT] =
{
    {614, -124200}, {627, -125490}, {639, -127885}, {650, -131150}, {662, -135970},
    {672, -140960}, {683, -147575}, {692, -153880}, {702, -161850}, {711, -169935},
    {720, -178950}, {729, -188865}, {737, -198475},
};

static PiecewiseLinearSegment_t
    s_tc_txk_measurement_segments[PRODUCT_TC_TXK_MEASUREMENT_SEGMENT_COUNT];
static PiecewiseLinearSegment_t
    s_tc_txk_cjc_segments[PRODUCT_TC_TXK_CJC_SEGMENT_COUNT];
static bool s_tables_initialized;

static const PiecewiseLinearTable_t s_tc_txk_measurement_table =
{
    s_tc_txk_measurement_segments,
    PRODUCT_TC_TXK_MEASUREMENT_SEGMENT_COUNT,
    PRODUCT_TC_TXK_MEASUREMENT_COEFFICIENT_SCALE
};

static const PiecewiseLinearTable_t s_tc_txk_cjc_table =
{
    s_tc_txk_cjc_segments,
    PRODUCT_TC_TXK_CJC_SEGMENT_COUNT,
    PRODUCT_TC_TXK_CJC_COEFFICIENT_SCALE
};

static void InitializeTables(void)
{
    uint16_t index;

    if (s_tables_initialized)
    {
        return;
    }

    for (index = 0U; index < PRODUCT_TC_TXK_MEASUREMENT_SEGMENT_COUNT; index++)
    {
        int32_t slope = s_tc_txk_measurement_coefficients[index].slope;
        s_tc_txk_measurement_segments[index].x_min =
            s_tc_txk_measurement_boundaries_shifted_uv[index] -
            PRODUCT_TC_TXK_INPUT_SHIFT_UV;
        s_tc_txk_measurement_segments[index].x_max =
            s_tc_txk_measurement_boundaries_shifted_uv[index + 1U] -
            PRODUCT_TC_TXK_INPUT_SHIFT_UV;
        s_tc_txk_measurement_segments[index].slope = slope;
        s_tc_txk_measurement_segments[index].intercept =
            ((int64_t)slope * PRODUCT_TC_TXK_INPUT_SHIFT_UV) +
            ((int64_t)s_tc_txk_measurement_coefficients[index]
                 .intercept_millicelsius *
             PRODUCT_TC_TXK_MEASUREMENT_COEFFICIENT_SCALE);
    }

    for (index = 0U; index < PRODUCT_TC_TXK_CJC_SEGMENT_COUNT; index++)
    {
        int32_t slope = s_tc_txk_cjc_coefficients[index].slope;
        s_tc_txk_cjc_segments[index].x_min =
            PRODUCT_TC_TXK_CJC_MIN_MILLICELSIUS +
            ((int32_t)index * PRODUCT_TC_TXK_CJC_INTERVAL_MC);
        s_tc_txk_cjc_segments[index].x_max =
            s_tc_txk_cjc_segments[index].x_min + PRODUCT_TC_TXK_CJC_INTERVAL_MC;
        s_tc_txk_cjc_segments[index].slope = slope;
        s_tc_txk_cjc_segments[index].intercept =
            (((int64_t)slope * PRODUCT_TC_TXK_CJC_INPUT_SHIFT_DECICELSIUS) +
             s_tc_txk_cjc_coefficients[index].intercept_deci_microvolts) *
            100LL;
    }

    s_tables_initialized = true;
}

const PiecewiseLinearTable_t *ProductTcTxkTable_GetMeasurementTable(void)
{
    InitializeTables();
    return &s_tc_txk_measurement_table;
}

const PiecewiseLinearTable_t *ProductTcTxkTable_GetCjcTable(void)
{
    InitializeTables();
    return &s_tc_txk_cjc_table;
}

bool ProductTcTxkTable_IsReady(void)
{
    InitializeTables();
    return PiecewiseLinearTable_IsValid(&s_tc_txk_measurement_table) &&
           PiecewiseLinearTable_IsValid(&s_tc_txk_cjc_table);
}

#else

const PiecewiseLinearTable_t *ProductTcTxkTable_GetMeasurementTable(void)
{
    return NULL;
}

const PiecewiseLinearTable_t *ProductTcTxkTable_GetCjcTable(void)
{
    return NULL;
}

bool ProductTcTxkTable_IsReady(void)
{
    return false;
}

#endif /* PRODUCT_TC_TXK_TABLE_COMPLETE */
