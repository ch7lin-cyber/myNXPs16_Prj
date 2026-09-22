#include "product_tc_c_table.h"

#include <stddef.h>

#if PRODUCT_TC_C_TABLE_COMPLETE

#if (PRODUCT_TC_C_MEASUREMENT_SEGMENT_COUNT == 0U)
#error "Enter the C measurement segment count"
#endif

typedef struct
{
    int32_t slope;
    int32_t intercept_millicelsius;
} ProductTcCMeasurementCoefficient_t;

typedef struct
{
    int32_t slope;
    int32_t intercept_deci_microvolts;
} ProductTcCCjcCoefficient_t;

/*
 * TODO: USER TABLE DATA
 * Enter MEASUREMENT_SEGMENT_COUNT + 1 strictly increasing boundaries.
 * Each value is input_uV + PRODUCT_TC_C_INPUT_SHIFT_UV.
 */
static const int32_t s_tc_c_measurement_boundaries_shifted_uv
    [PRODUCT_TC_C_MEASUREMENT_BOUNDARY_COUNT] =
{
    /* TODO: boundary_0, boundary_1, ... boundary_N */
   33,  300,  572,  854, 1145, 1444, 1751, 2065, 2387, 2715,//10
    3049, 3389, 3734, 4085, 4440, 4800, 5163, 5531, 5901, 6275,//20
    6652, 7031, 7412, 7796, 8181, 8567, 8955, 9344, 9734, 10124, //30
    10515, 10906, 11297, 11688, 12078, 12469, 12858, 13247, 13636, 14023, //40
    14409, 14794, 15177, 15560, 15940, 16320, 16697, 17073, 17447, 17819, //50
    18189, 18557, 18923, 19287, 19649, 20009, 20366, 20721, 21074, 21425, //60
    21773, 22119, 22463, 22804, 23143, 23480, 23814, 24146, 24475, 24802, //70
    25126, 25448, 25768, 26085, 26400, 26712, 27022, 27330, 27635, 27937, //80
    28238, 28536, 28831, 29124, 29414, 29702, 29988, 30271, 30552, 30830, //90
    31105, 31378, 31649, 31917, 32182, 32444, 32704, 32961, 33215, 33466, //100
    33715, 33960, 34202, 34442, 34678, 34910, 35139, 35365, 35588, 35806, //110
    36021, 36232, 36438, 36641, 36839, 37033, 37222, 37407,
};

/*
 * TODO: USER TABLE DATA
 * temperature_mC = slope * shifted_uV / 100 + intercept_mC
 */
static const ProductTcCMeasurementCoefficient_t
    s_tc_c_measurement_coefficients[PRODUCT_TC_C_MEASUREMENT_SEGMENT_COUNT] =
{
    /* TODO: {slope_0, intercept_0}, ... {slope_N-1, intercept_N-1} */
    {7491, -22088}, {7353, -21993}, {7092, -20523}, {6873, -18641}, {6689, -16536}, //5
    {6515, -14017}, {6369, -11482}, {6211, -8213}, {6098, -5511}, {5988, -2532}, //10
    {5882, 688}, {5797, 3547}, {5698, 7247}, {5634, 9865}, {5556, 13340}, //15
    {5510, 15532}, {5435, 19396}, {5405, 21051}, {5348, 24409}, {5305, 27119}, //20
    {5277, 28991}, {5249, 30950}, {5208, 33989}, {5195, 35023}, {5181, 36148}, //25
    {5155, 38371}, {5141, 39631}, {5128, 40853}, {5128, 40854}, {5115, 42166}, //30
    {5115, 42172}, {5115, 42172}, {5115, 42170}, {5128, 40640}, {5115, 42202}, //35
    {5141, 38960}, {5141, 38946}, {5141, 38970}, {5168, 35306}, {5181, 33474}, //40
    {5195, 31426}, {5222, 27449}, {5222, 27453}, {5263, 21077}, {5263, 21078}, //45
    {5305, 14236}, {5319, 11892}, {5348, 6944}, {5376, 2056}, {5405, -3116}, //50
    {5435, -8578}, {5464, -13965}, {5495, -19836}, {5525, -25618}, {5556, -31699}, //55
    {5602, -40908}, {5634, -47442}, {5666, -54077}, {5698, -60806}, {5747, -71306}, //60
    {5780, -78508}, {5814, -86016}, {5865, -97461}, {5900, -105456}, {5935, -113546}, //65
    {5988, -125976}, {6024, -134543}, {6079, -147831}, {6116, -156887}, {6169, -170042}, //70
    {6211, -180604}, {6250, -190500}, {6309, -205714}, {6349, -216152}, {6410, -232259}, //75
    {6452, -243491}, {6494, -254810}, {6557, -272027}, {6623, -290286}, {6645, -296445}, //80
    {6711, -315042}, {6780, -334733}, {6826, -348009}, {6897, -368683}, {6944, -382543}, //85
    {6993, -397092}, {7067, -419270}, {7117, -434403}, {7194, -457907}, {7273, -482284}, //90
    {7326, -498794}, {7380, -515709}, {7463, -541958}, {7547, -568757}, {7634, -596790}, //95
    {7692, -615592}, {7782, -645039}, {7874, -675361}, {7968, -706605}, {8032, -728001}, //100
    {8163, -772165}, {8264, -806487}, {8333, -830052}, {8475, -878943}, {8621, -929599}, //105
    {8734, -969090}, {8850, -1009859}, {8969, -1051894}, {9174, -1124856}, {9302, -1170697}, //110
    {9479, -1234433}, {9709, -1317804}, {9852, -1369926}, {10101, -1461158}, {10309, -1537773}, //115
    {10582, -1638888}, {10811, -1724084},
};

/*
 * TODO: USER TABLE DATA
 * shifted_temperature_deci_C = temperature_deci_C + 200
 * cjc_deci_uV = slope * shifted_temperature_deci_C + intercept_deci_uV
 */
static const ProductTcCCjcCoefficient_t
    s_tc_c_cjc_coefficients[PRODUCT_TC_C_CJC_SEGMENT_COUNT] =
{
    /* TODO: 13 rows: {slope_0, intercept_0}, ... {slope_12, intercept_12} */
    {134,-26700}, {133,-26570}, {135,-27050}, {137,-27635}, {140,-28795},
    {142,-29820}, {144,-31000}, {147,-33115}, {148,-33920}, {151,-36625},
    {152,-37610}, {155,-40900}, {156,-42130},
};

static PiecewiseLinearSegment_t
    s_tc_c_measurement_segments[PRODUCT_TC_C_MEASUREMENT_SEGMENT_COUNT];
static PiecewiseLinearSegment_t
    s_tc_c_cjc_segments[PRODUCT_TC_C_CJC_SEGMENT_COUNT];
static bool s_tables_initialized;

static const PiecewiseLinearTable_t s_tc_c_measurement_table =
{
    s_tc_c_measurement_segments,
    PRODUCT_TC_C_MEASUREMENT_SEGMENT_COUNT,
    PRODUCT_TC_C_MEASUREMENT_COEFFICIENT_SCALE
};

static const PiecewiseLinearTable_t s_tc_c_cjc_table =
{
    s_tc_c_cjc_segments,
    PRODUCT_TC_C_CJC_SEGMENT_COUNT,
    PRODUCT_TC_C_CJC_COEFFICIENT_SCALE
};

static void InitializeTables(void)
{
    uint16_t index;

    if (s_tables_initialized)
    {
        return;
    }

    for (index = 0U; index < PRODUCT_TC_C_MEASUREMENT_SEGMENT_COUNT; index++)
    {
        int32_t slope = s_tc_c_measurement_coefficients[index].slope;
        s_tc_c_measurement_segments[index].x_min =
            s_tc_c_measurement_boundaries_shifted_uv[index] -
            PRODUCT_TC_C_INPUT_SHIFT_UV;
        s_tc_c_measurement_segments[index].x_max =
            s_tc_c_measurement_boundaries_shifted_uv[index + 1U] -
            PRODUCT_TC_C_INPUT_SHIFT_UV;
        s_tc_c_measurement_segments[index].slope = slope;
        s_tc_c_measurement_segments[index].intercept =
            ((int64_t)slope * PRODUCT_TC_C_INPUT_SHIFT_UV) +
            ((int64_t)s_tc_c_measurement_coefficients[index]
                 .intercept_millicelsius *
             PRODUCT_TC_C_MEASUREMENT_COEFFICIENT_SCALE);
    }

    for (index = 0U; index < PRODUCT_TC_C_CJC_SEGMENT_COUNT; index++)
    {
        int32_t slope = s_tc_c_cjc_coefficients[index].slope;
        s_tc_c_cjc_segments[index].x_min =
            PRODUCT_TC_C_CJC_MIN_MILLICELSIUS +
            ((int32_t)index * PRODUCT_TC_C_CJC_INTERVAL_MC);
        s_tc_c_cjc_segments[index].x_max =
            s_tc_c_cjc_segments[index].x_min + PRODUCT_TC_C_CJC_INTERVAL_MC;
        s_tc_c_cjc_segments[index].slope = slope;
        s_tc_c_cjc_segments[index].intercept =
            (((int64_t)slope * PRODUCT_TC_C_CJC_INPUT_SHIFT_DECICELSIUS) +
             s_tc_c_cjc_coefficients[index].intercept_deci_microvolts) *
            100LL;
    }

    s_tables_initialized = true;
}

const PiecewiseLinearTable_t *ProductTcCTable_GetMeasurementTable(void)
{
    InitializeTables();
    return &s_tc_c_measurement_table;
}

const PiecewiseLinearTable_t *ProductTcCTable_GetCjcTable(void)
{
    InitializeTables();
    return &s_tc_c_cjc_table;
}

bool ProductTcCTable_IsReady(void)
{
    InitializeTables();
    return PiecewiseLinearTable_IsValid(&s_tc_c_measurement_table) &&
           PiecewiseLinearTable_IsValid(&s_tc_c_cjc_table);
}

#else

const PiecewiseLinearTable_t *ProductTcCTable_GetMeasurementTable(void)
{
    return NULL;
}

const PiecewiseLinearTable_t *ProductTcCTable_GetCjcTable(void)
{
    return NULL;
}

bool ProductTcCTable_IsReady(void)
{
    return false;
}

#endif /* PRODUCT_TC_C_TABLE_COMPLETE */

