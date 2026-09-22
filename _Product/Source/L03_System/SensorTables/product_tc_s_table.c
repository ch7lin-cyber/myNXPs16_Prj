#include "product_tc_s_table.h"

#include <stddef.h>

#if PRODUCT_TC_S_TABLE_COMPLETE

#if (PRODUCT_TC_S_MEASUREMENT_SEGMENT_COUNT == 0U)
#error "The S measurement segment count must not be zero"
#endif

typedef struct
{
    int32_t slope;
    int32_t intercept_millicelsius;
} ProductTcSMeasurementCoefficient_t;

typedef struct
{
    int32_t slope;
    int32_t intercept_deci_microvolts;
} ProductTcSCjcCoefficient_t;

/* Values are input_uV + PRODUCT_TC_S_INPUT_SHIFT_UV. */
static const int32_t s_tc_s_measurement_boundaries_shifted_uv
    [PRODUCT_TC_S_MEASUREMENT_BOUNDARY_COUNT] =
{
    7, 110, 223, 345, 475, 612, 756, 905, 1060, 1220,
    1383, 1551, 1722, 1896, 2072, 2251, 2433, 2617, 2802, 2990,
    3179, 3369, 3561, 3755, 3950, 4146, 4343, 4542, 4742, 4943,
    5145, 5349, 5553, 5759, 5967, 6175, 6385, 6596, 6809, 7023,
    7238, 7455, 7673, 7893, 8113, 8336, 8559, 8784, 9010, 9238,
    9467, 9697, 9929, 10161, 10395, 10630, 10867, 11104, 11342, 11581,
    11820, 12061, 12301, 12543, 12785, 13027, 13269, 13512, 13754, 13997,
    14240, 14483, 14725, 14967, 15209, 15451, 15692, 15932, 16172, 16411,
    16649, 16887, 17123, 17359, 17593, 17827, 18057, 18284,
};

/* temperature_mC = slope * shifted_uV / scale + intercept_mC. */
static const ProductTcSMeasurementCoefficient_t
    s_tc_s_measurement_coefficients[PRODUCT_TC_S_MEASUREMENT_SEGMENT_COUNT] =
{
    {19417, -21213}, {17699, -19332}, {16393, -16449}, {15385, -12963}, {14599, -9269},
    {13889, -4931}, {13423, -1445}, {12903, 3228}, {12500, 7563}, {12270, 10334},
    {11905, 15366}, {11696, 18662}, {11494, 22142}, {11364, 24572}, {11173, 28482},
    {10989, 32616}, {10870, 35565}, {10811, 37089}, {10638, 41934}, {10582, 43643},
    {10526, 45379}, {10417, 49020}, {10309, 52892}, {10256, 54913}, {10204, 56973},
    {10152, 59098}, {10050, 63531}, {10000, 65800}, {9950, 68199}, {9901, 70603},
    {9804, 75604}, {9804, 75584}, {9709, 80825}, {9615, 86273}, {9615, 86273},
    {9524, 91869}, {9479, 94744}, {9390, 100624}, {9346, 103640}, {9302, 106721},
    {9217, 112869}, {9174, 116092}, {9091, 122447}, {9091, 122447}, {8969, 132351},
    {8969, 132357}, {8889, 139179}, {8850, 142602}, {8772, 149625}, {8734, 153163},
    {8696, 156749}, {8621, 164039}, {8621, 164029}, {8547, 171531}, {8511, 175260},
    {8439, 182947}, {8439, 182974}, {8403, 186961}, {8368, 190925}, {8368, 190921},
    {8299, 199093}, {8333, 194957}, {8264, 203438}, {8264, 203480}, {8264, 203481},
    {8264, 203462}, {8230, 207974}, {8264, 203369}, {8230, 208022}, {8225, 208747},
    {8230, 208073}, {8264, 203138}, {8264, 203110}, {8264, 203107}, {8264, 203133},
    {8299, 197744}, {8333, 192386}, {8333, 192387}, {8368, 186721}, {8403, 180959},
    {8403, 180972}, {8475, 168809}, {8475, 168808}, {8547, 156309}, {8547, 156326},
    {8696, 129764}, {8811, 108945},
};

/* cjc_deci_uV = slope * shifted_temperature_deci_C + intercept. */
static const ProductTcSCjcCoefficient_t
    s_tc_s_cjc_coefficients[PRODUCT_TC_S_CJC_SEGMENT_COUNT] =
{
    {50, -10335}, {53, -10605}, {55, -11000}, {58, -11910}, {60, -12700},
    {62, -13730}, {64, -14920}, {66, -16320}, {68, -17930}, {69, -18835},
    {71, -20795}, {73, -23005}, {74, -24210},
};

static PiecewiseLinearSegment_t
    s_tc_s_measurement_segments[PRODUCT_TC_S_MEASUREMENT_SEGMENT_COUNT];
static PiecewiseLinearSegment_t
    s_tc_s_cjc_segments[PRODUCT_TC_S_CJC_SEGMENT_COUNT];
static bool s_tables_initialized;

static const PiecewiseLinearTable_t s_tc_s_measurement_table =
{
    s_tc_s_measurement_segments,
    PRODUCT_TC_S_MEASUREMENT_SEGMENT_COUNT,
    PRODUCT_TC_S_MEASUREMENT_COEFFICIENT_SCALE
};

static const PiecewiseLinearTable_t s_tc_s_cjc_table =
{
    s_tc_s_cjc_segments,
    PRODUCT_TC_S_CJC_SEGMENT_COUNT,
    PRODUCT_TC_S_CJC_COEFFICIENT_SCALE
};

static void InitializeTables(void)
{
    uint16_t index;

    if (s_tables_initialized)
    {
        return;
    }

    for (index = 0U; index < PRODUCT_TC_S_MEASUREMENT_SEGMENT_COUNT; index++)
    {
        int32_t slope = s_tc_s_measurement_coefficients[index].slope;
        s_tc_s_measurement_segments[index].x_min =
            s_tc_s_measurement_boundaries_shifted_uv[index] -
            PRODUCT_TC_S_INPUT_SHIFT_UV;
        s_tc_s_measurement_segments[index].x_max =
            s_tc_s_measurement_boundaries_shifted_uv[index + 1U] -
            PRODUCT_TC_S_INPUT_SHIFT_UV;
        s_tc_s_measurement_segments[index].slope = slope;
        s_tc_s_measurement_segments[index].intercept =
            ((int64_t)slope * PRODUCT_TC_S_INPUT_SHIFT_UV) +
            ((int64_t)s_tc_s_measurement_coefficients[index]
                 .intercept_millicelsius *
             PRODUCT_TC_S_MEASUREMENT_COEFFICIENT_SCALE);
    }

    for (index = 0U; index < PRODUCT_TC_S_CJC_SEGMENT_COUNT; index++)
    {
        int32_t slope = s_tc_s_cjc_coefficients[index].slope;
        s_tc_s_cjc_segments[index].x_min =
            PRODUCT_TC_S_CJC_MIN_MILLICELSIUS +
            ((int32_t)index * PRODUCT_TC_S_CJC_INTERVAL_MC);
        s_tc_s_cjc_segments[index].x_max =
            s_tc_s_cjc_segments[index].x_min + PRODUCT_TC_S_CJC_INTERVAL_MC;
        s_tc_s_cjc_segments[index].slope = slope;
        s_tc_s_cjc_segments[index].intercept =
            (((int64_t)slope * PRODUCT_TC_S_CJC_INPUT_SHIFT_DECICELSIUS) +
             s_tc_s_cjc_coefficients[index].intercept_deci_microvolts) *
            100LL;
    }

    s_tables_initialized = true;
}

const PiecewiseLinearTable_t *ProductTcSTable_GetMeasurementTable(void)
{
    InitializeTables();
    return &s_tc_s_measurement_table;
}

const PiecewiseLinearTable_t *ProductTcSTable_GetCjcTable(void)
{
    InitializeTables();
    return &s_tc_s_cjc_table;
}

bool ProductTcSTable_IsReady(void)
{
    InitializeTables();
    return PiecewiseLinearTable_IsValid(&s_tc_s_measurement_table) &&
           PiecewiseLinearTable_IsValid(&s_tc_s_cjc_table);
}

#else

const PiecewiseLinearTable_t *ProductTcSTable_GetMeasurementTable(void)
{
    return NULL;
}

const PiecewiseLinearTable_t *ProductTcSTable_GetCjcTable(void)
{
    return NULL;
}

bool ProductTcSTable_IsReady(void)
{
    return false;
}

#endif /* PRODUCT_TC_S_TABLE_COMPLETE */
