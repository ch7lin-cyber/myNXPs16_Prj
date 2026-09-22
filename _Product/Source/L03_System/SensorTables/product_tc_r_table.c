#include "product_tc_r_table.h"

#include <stddef.h>

#if PRODUCT_TC_R_TABLE_COMPLETE

#if (PRODUCT_TC_R_MEASUREMENT_SEGMENT_COUNT == 0U)
#error "The R measurement segment count must not be zero"
#endif

typedef struct
{
    int32_t slope;
    int32_t intercept_millicelsius;
} ProductTcRMeasurementCoefficient_t;

typedef struct
{
    int32_t slope;
    int32_t intercept_deci_microvolts;
} ProductTcRCjcCoefficient_t;

/* Values are input_uV + PRODUCT_TC_R_INPUT_SHIFT_UV. */
static const int32_t s_tc_r_measurement_boundaries_shifted_uv
    [PRODUCT_TC_R_MEASUREMENT_BOUNDARY_COUNT] =
{
    10, 110, 221, 342, 473, 611, 757, 910, 1069, 1234,
    1404, 1579, 1758, 1941, 2127, 2317, 2511, 2707, 2906, 3107,
    3311, 3518, 3726, 3937, 4150, 4365, 4581, 4800, 5020, 5243,
    5467, 5693, 5922, 6151, 6383, 6617, 6853, 7090, 7330, 7571,
    7815, 8060, 8307, 8556, 8807, 9060, 9315, 9571, 9830, 10090,
    10352, 10616, 10881, 11149, 11417, 11688, 11960, 12233, 12507, 12783,
    13060, 13338, 13617, 13896, 14176, 14457, 14739, 15021, 15303, 15585,
    15868, 16150, 16433, 16715, 16997, 17279, 17561, 17842, 18122, 18402,
    18681, 18959, 19236, 19512, 19787, 20061, 20332, 20598,
};

/* temperature_mC = slope * shifted_uV / scale + intercept_mC. */
static const ProductTcRMeasurementCoefficient_t
    s_tc_r_measurement_coefficients[PRODUCT_TC_R_MEASUREMENT_SEGMENT_COUNT] =
{
    {20000, -21900}, {18018, -19685}, {16529, -16476}, {15267, -12133}, {14493, -8451},
    {13699, -3660}, {13072, 1077}, {12579, 5565}, {12121, 10442}, {11765, 14878},
    {11429, 19594}, {11173, 23657}, {10929, 27942}, {10753, 31327}, {10526, 36112},
    {10309, 41172}, {10204, 43839}, {10050, 47984}, {9950, 50871}, {9804, 55380},
    {9662, 60091}, {9615, 61764}, {9479, 66819}, {9390, 70339}, {9302, 74014},
    {9259, 75864}, {9132, 81675}, {9091, 83632}, {8969, 89742}, {8929, 91861},
    {8850, 96153}, {8734, 102782}, {8734, 102792}, {8621, 109696}, {8547, 114441},
    {8475, 119226}, {8439, 121681}, {8333, 129191}, {8299, 131697}, {8197, 139421},
    {8163, 142093}, {8097, 147400}, {8032, 152788}, {7968, 158256}, {7905, 163813},
    {7843, 169444}, {7813, 172211}, {7722, 180929}, {7692, 183877}, {7634, 189725},
    {7576, 195740}, {7547, 198811}, {7463, 207958}, {7463, 207965}, {7380, 217435},
    {7353, 220611}, {7326, 223832}, {7299, 227106}, {7246, 233722}, {7220, 237059},
    {7194, 240468}, {7168, 243949}, {7168, 243950}, {7143, 247409}, {7117, 251060},
    {7092, 254710}, {7092, 254742}, {7092, 254739}, {7092, 254722}, {7090, 255012},
    {7092, 254652}, {7067, 258690}, {7092, 254579}, {7092, 254559}, {7092, 254555},
    {7092, 254580}, {7117, 250204}, {7143, 245546}, {7143, 245546}, {7168, 240950},
    {7194, 236093}, {7220, 231151}, {7246, 226145}, {7273, 220873}, {7299, 215747},
    {7380, 199496}, {7519, 171203},
};

/* cjc_deci_uV = slope * shifted_temperature_deci_C + intercept. */
static const ProductTcRCjcCoefficient_t
    s_tc_r_cjc_coefficients[PRODUCT_TC_R_CJC_SEGMENT_COUNT] =
{
    {49, -10035}, {51, -10235}, {54, -10810}, {57, -11705}, {60, -12945},
    {61, -13415}, {64, -15170}, {67, -17300}, {68, -18140}, {70, -19880},
    {72, -21880}, {74, -24080}, {76, -26500},
};

static PiecewiseLinearSegment_t
    s_tc_r_measurement_segments[PRODUCT_TC_R_MEASUREMENT_SEGMENT_COUNT];
static PiecewiseLinearSegment_t
    s_tc_r_cjc_segments[PRODUCT_TC_R_CJC_SEGMENT_COUNT];
static bool s_tables_initialized;

static const PiecewiseLinearTable_t s_tc_r_measurement_table =
{
    s_tc_r_measurement_segments,
    PRODUCT_TC_R_MEASUREMENT_SEGMENT_COUNT,
    PRODUCT_TC_R_MEASUREMENT_COEFFICIENT_SCALE
};

static const PiecewiseLinearTable_t s_tc_r_cjc_table =
{
    s_tc_r_cjc_segments,
    PRODUCT_TC_R_CJC_SEGMENT_COUNT,
    PRODUCT_TC_R_CJC_COEFFICIENT_SCALE
};

static void InitializeTables(void)
{
    uint16_t index;

    if (s_tables_initialized)
    {
        return;
    }

    for (index = 0U; index < PRODUCT_TC_R_MEASUREMENT_SEGMENT_COUNT; index++)
    {
        int32_t slope = s_tc_r_measurement_coefficients[index].slope;
        s_tc_r_measurement_segments[index].x_min =
            s_tc_r_measurement_boundaries_shifted_uv[index] -
            PRODUCT_TC_R_INPUT_SHIFT_UV;
        s_tc_r_measurement_segments[index].x_max =
            s_tc_r_measurement_boundaries_shifted_uv[index + 1U] -
            PRODUCT_TC_R_INPUT_SHIFT_UV;
        s_tc_r_measurement_segments[index].slope = slope;
        s_tc_r_measurement_segments[index].intercept =
            ((int64_t)slope * PRODUCT_TC_R_INPUT_SHIFT_UV) +
            ((int64_t)s_tc_r_measurement_coefficients[index]
                 .intercept_millicelsius *
             PRODUCT_TC_R_MEASUREMENT_COEFFICIENT_SCALE);
    }

    for (index = 0U; index < PRODUCT_TC_R_CJC_SEGMENT_COUNT; index++)
    {
        int32_t slope = s_tc_r_cjc_coefficients[index].slope;
        s_tc_r_cjc_segments[index].x_min =
            PRODUCT_TC_R_CJC_MIN_MILLICELSIUS +
            ((int32_t)index * PRODUCT_TC_R_CJC_INTERVAL_MC);
        s_tc_r_cjc_segments[index].x_max =
            s_tc_r_cjc_segments[index].x_min + PRODUCT_TC_R_CJC_INTERVAL_MC;
        s_tc_r_cjc_segments[index].slope = slope;
        s_tc_r_cjc_segments[index].intercept =
            (((int64_t)slope * PRODUCT_TC_R_CJC_INPUT_SHIFT_DECICELSIUS) +
             s_tc_r_cjc_coefficients[index].intercept_deci_microvolts) *
            100LL;
    }

    s_tables_initialized = true;
}

const PiecewiseLinearTable_t *ProductTcRTable_GetMeasurementTable(void)
{
    InitializeTables();
    return &s_tc_r_measurement_table;
}

const PiecewiseLinearTable_t *ProductTcRTable_GetCjcTable(void)
{
    InitializeTables();
    return &s_tc_r_cjc_table;
}

bool ProductTcRTable_IsReady(void)
{
    InitializeTables();
    return PiecewiseLinearTable_IsValid(&s_tc_r_measurement_table) &&
           PiecewiseLinearTable_IsValid(&s_tc_r_cjc_table);
}

#else

const PiecewiseLinearTable_t *ProductTcRTable_GetMeasurementTable(void)
{
    return NULL;
}

const PiecewiseLinearTable_t *ProductTcRTable_GetCjcTable(void)
{
    return NULL;
}

bool ProductTcRTable_IsReady(void)
{
    return false;
}

#endif /* PRODUCT_TC_R_TABLE_COMPLETE */
