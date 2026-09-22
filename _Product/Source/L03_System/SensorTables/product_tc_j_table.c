#include "product_tc_j_table.h"

#include <stddef.h>

#if PRODUCT_TC_J_TABLE_COMPLETE

#if (PRODUCT_TC_J_MEASUREMENT_SEGMENT_COUNT == 0U)
#error "Enter the J measurement segment count"
#endif

typedef struct
{
    int32_t slope;
    int32_t intercept_millicelsius;
} ProductTcJMeasurementCoefficient_t;

typedef struct
{
    int32_t slope;
    int32_t intercept_deci_microvolts;
} ProductTcJCjcCoefficient_t;

/*
 * TODO: USER TABLE DATA
 * Enter MEASUREMENT_SEGMENT_COUNT + 1 strictly increasing boundaries.
 * Each value is input_uV + PRODUCT_TC_J_INPUT_SHIFT_UV.
 */
static const int32_t s_tc_j_measurement_boundaries_shifted_uv
    [PRODUCT_TC_J_MEASUREMENT_BOUNDARY_COUNT] =
{
    /* TODO: boundary_0, boundary_1, ... boundary_N */
 10,  497, 1079, 1741, 2474, 3267, 4114, 5007, 5939, 6905,//10
    7900, 8919, 9959, 11016, 12087, 13169, 14260, 15359, 16462, 17569, //20
    18679, 19789, 20900, 22010, 23119, 24227, 25334, 26438, 27542, 28645, //30
    29748, 30852, 31957, 33064, 34176, 35293, 36416, 37547, 38688, 39839, //40
    41002, 42179, 43370, 44575, 45796, 47032, 48282, 49545, 50819, 52103, //50
    53394, 54686, 55974, 57253, 58522, 59777, 61019, 62247, 63461, 64663, //60
    65853, 67034, 68207, 69373, 70534, 71692, 72848, 74002, 75155, 76306, //70
    77453,
};

/*
 * TODO: USER TABLE DATA
 * temperature_mC = slope * shifted_uV / 100 + intercept_mC
 */
static const ProductTcJMeasurementCoefficient_t
    s_tc_j_measurement_coefficients[PRODUCT_TC_J_MEASUREMENT_SEGMENT_COUNT] =
{
    /* TODO: {slope_0, intercept_0}, ... {slope_N-1, intercept_N-1} */
  {4107, -200155}, {3436, -196886}, {3021, -192444}, {2729, -187392}, {2522, -182299}, //5
    {2361, -177059}, {2240, -172094}, {2146, -167401}, {2070, -162895}, {2010, -158755}, //10
    {1963, -155052}, {1923, -151493}, {1892, -148399}, {1867, -145648}, {1848, -143348}, //15
    {1833, -141379}, {1820, -139527}, {1813, -138450}, {1807, -137472}, {1802, -136594}, //20
    {1802, -136597}, {1800, -136202}, {1802, -136623}, {1803, -136845}, {1805, -137303}, //25
    {1807, -137784}, {1812, -139056}, {1812, -139066}, {1813, -139340}, {1813, -139334}, //30
    {1812, -139032}, {1810, -138412}, {1807, -137462}, {1799, -134819}, {1791, -132083}, //35
    {1781, -128557}, {1768, -123821}, {1753, -118185}, {1738, -112380}, {1720, -105221}, //40
    {1699, -96609}, {1679, -88166}, {1660, -79928}, {1638, -70125}, {1618, -60963}, //45
    {1600, -52496}, {1584, -44771}, {1570, -37844}, {1558, -31750}, {1549, -27073}, //50
    {1548, -26547}, {1553, -29286}, {1564, -35448}, {1576, -42321}, {1594, -52858}, //55
    {1610, -62427}, {1629, -74017}, {1647, -85223}, {1664, -96009}, {1681, -107003}, //60
    {1693, -114902}, {1705, -122936}, {1715, -129749}, {1723, -135301}, {1727, -138129}, //65
    {1730, -140276}, {1733, -142463}, {1735, -143946}, {1738, -146200}, {1743, -150017} //70


};

/*
 * TODO: USER TABLE DATA
 * shifted_temperature_deci_C = temperature_deci_C + 200
 * cjc_deci_uV = slope * shifted_temperature_deci_C + intercept_deci_uV
 */
static const ProductTcJCjcCoefficient_t
    s_tc_j_cjc_coefficients[PRODUCT_TC_J_CJC_SEGMENT_COUNT] =
{
    /* TODO: 13 rows: {slope_0, intercept_0}, ... {slope_12, intercept_12} */
    {494,  -99530}, {501, -100235}, {507, -101460}, {512, -102940}, {518, -105340},
    {522, -107360}, {526, -109730}, {531, -113245}, {534, -115660}, {537, -118360},
    {539, -120345}, {543, -124690}, {545, -127150},
};

static PiecewiseLinearSegment_t
    s_tc_j_measurement_segments[PRODUCT_TC_J_MEASUREMENT_SEGMENT_COUNT];
static PiecewiseLinearSegment_t
    s_tc_j_cjc_segments[PRODUCT_TC_J_CJC_SEGMENT_COUNT];
static bool s_tables_initialized;

static const PiecewiseLinearTable_t s_tc_j_measurement_table =
{
    s_tc_j_measurement_segments,
    PRODUCT_TC_J_MEASUREMENT_SEGMENT_COUNT,
    PRODUCT_TC_J_MEASUREMENT_COEFFICIENT_SCALE
};

static const PiecewiseLinearTable_t s_tc_j_cjc_table =
{
    s_tc_j_cjc_segments,
    PRODUCT_TC_J_CJC_SEGMENT_COUNT,
    PRODUCT_TC_J_CJC_COEFFICIENT_SCALE
};

static void InitializeTables(void)
{
    uint16_t index;

    if (s_tables_initialized)
    {
        return;
    }

    for (index = 0U; index < PRODUCT_TC_J_MEASUREMENT_SEGMENT_COUNT; index++)
    {
        int32_t slope = s_tc_j_measurement_coefficients[index].slope;
        s_tc_j_measurement_segments[index].x_min =
            s_tc_j_measurement_boundaries_shifted_uv[index] -
            PRODUCT_TC_J_INPUT_SHIFT_UV;
        s_tc_j_measurement_segments[index].x_max =
            s_tc_j_measurement_boundaries_shifted_uv[index + 1U] -
            PRODUCT_TC_J_INPUT_SHIFT_UV;
        s_tc_j_measurement_segments[index].slope = slope;
        s_tc_j_measurement_segments[index].intercept =
            ((int64_t)slope * PRODUCT_TC_J_INPUT_SHIFT_UV) +
            ((int64_t)s_tc_j_measurement_coefficients[index]
                 .intercept_millicelsius *
             PRODUCT_TC_J_MEASUREMENT_COEFFICIENT_SCALE);
    }

    for (index = 0U; index < PRODUCT_TC_J_CJC_SEGMENT_COUNT; index++)
    {
        int32_t slope = s_tc_j_cjc_coefficients[index].slope;
        s_tc_j_cjc_segments[index].x_min =
            PRODUCT_TC_J_CJC_MIN_MILLICELSIUS +
            ((int32_t)index * PRODUCT_TC_J_CJC_INTERVAL_MC);
        s_tc_j_cjc_segments[index].x_max =
            s_tc_j_cjc_segments[index].x_min + PRODUCT_TC_J_CJC_INTERVAL_MC;
        s_tc_j_cjc_segments[index].slope = slope;
        s_tc_j_cjc_segments[index].intercept =
            (((int64_t)slope * PRODUCT_TC_J_CJC_INPUT_SHIFT_DECICELSIUS) +
             s_tc_j_cjc_coefficients[index].intercept_deci_microvolts) *
            100LL;
    }

    s_tables_initialized = true;
}

const PiecewiseLinearTable_t *ProductTcJTable_GetMeasurementTable(void)
{
    InitializeTables();
    return &s_tc_j_measurement_table;
}

const PiecewiseLinearTable_t *ProductTcJTable_GetCjcTable(void)
{
    InitializeTables();
    return &s_tc_j_cjc_table;
}

bool ProductTcJTable_IsReady(void)
{
    InitializeTables();
    return PiecewiseLinearTable_IsValid(&s_tc_j_measurement_table) &&
           PiecewiseLinearTable_IsValid(&s_tc_j_cjc_table);
}

#else

const PiecewiseLinearTable_t *ProductTcJTable_GetMeasurementTable(void)
{
    return NULL;
}

const PiecewiseLinearTable_t *ProductTcJTable_GetCjcTable(void)
{
    return NULL;
}

bool ProductTcJTable_IsReady(void)
{
    return false;
}

#endif /* PRODUCT_TC_J_TABLE_COMPLETE */

