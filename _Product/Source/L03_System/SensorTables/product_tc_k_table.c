#include "product_tc_k_table.h"

#include <stddef.h>

#if PRODUCT_TC_K_TABLE_COMPLETE

#if (PRODUCT_TC_K_MEASUREMENT_SEGMENT_COUNT == 0U)
#error "The K measurement segment count must not be zero"
#endif

typedef struct
{
    int32_t slope;
    int32_t intercept_millicelsius;
} ProductTcKMeasurementCoefficient_t;

typedef struct
{
    int32_t slope;
    int32_t intercept_deci_microvolts;
} ProductTcKCjcCoefficient_t;

/* Values are input_uV + PRODUCT_TC_K_INPUT_SHIFT_UV. */
static const int32_t s_tc_k_measurement_boundaries_shifted_uv
    [PRODUCT_TC_K_MEASUREMENT_BOUNDARY_COUNT] =
{
    42, 309, 650, 1059, 1531, 2062, 2646, 3280, 3957, 4673,
    5422, 6200, 6998, 7812, 8636, 9467, 10296, 11120, 11935, 12740,
    13540, 14338, 15140, 15947, 16761, 17582, 18409, 19240, 20074, 20913,
    21754, 22597, 23443, 24291, 25141, 25992, 26844, 27697, 28550, 29403,
    30255, 31105, 31955, 32802, 33647, 34489, 35329, 36165, 36998, 37828,
    38653, 39475, 40293, 41108, 41918, 42724, 43526, 44324, 45118, 45908,
    46694, 47476, 48253, 49026, 49795, 50559, 51319, 52073, 52823, 53567,
    54305, 55038, 55765, 56486, 57200, 57908, 58610, 59306,
};

/* temperature_mC = slope * shifted_uV / scale + intercept_mC. */
static const ProductTcKMeasurementCoefficient_t
    s_tc_k_measurement_coefficients[PRODUCT_TC_K_MEASUREMENT_SEGMENT_COUNT] =
{
    {7479, -222741}, {5865, -217843}, {4890, -211575}, {4237, -204701}, {3766, -197516},
    {3425, -190507}, {3155, -183375}, {2954, -176797}, {2793, -170452}, {2670, -164715},
    {2571, -159359}, {2506, -155343}, {2457, -151916}, {2427, -149585}, {2407, -147865},
    {2413, -148444}, {2427, -149901}, {2454, -152894}, {2484, -156474}, {2500, -158512},
    {2506, -159317}, {2494, -157582}, {2478, -155157}, {2457, -151810}, {2436, -148290},
    {2418, -145120}, {2407, -143089}, {2398, -141372}, {2384, -138560}, {2378, -137301},
    {2372, -135996}, {2364, -134191}, {2358, -132784}, {2353, -131568}, {2350, -130813},
    {2347, -130034}, {2345, -129495}, {2345, -129497}, {2345, -129495}, {2347, -130083},
    {2353, -131900}, {2353, -131901}, {2361, -134459}, {2367, -136429}, {2375, -139125},
    {2381, -141195}, {2392, -145080}, {2401, -148337}, {2410, -151653}, {2424, -156956},
    {2433, -160443}, {2445, -165180}, {2454, -168797}, {2469, -174956}, {2481, -179987},
    {2494, -185542}, {2506, -190767}, {2519, -196531}, {2532, -202401}, {2545, -208366},
    {2558, -214437}, {2574, -222038}, {2587, -228321}, {2601, -235181}, {2618, -243647},
    {2632, -250726}, {2653, -261501}, {2667, -268794}, {2688, -279887}, {2710, -291683},
    {2729, -302004}, {2751, -314113}, {2774, -326931}, {2801, -342190}, {2825, -355923},
    {2839, -364096}, {2874, -384460},
};

/* cjc_deci_uV = slope * shifted_temperature_deci_C + intercept. */
static const ProductTcKCjcCoefficient_t
    s_tc_k_cjc_coefficients[PRODUCT_TC_K_CJC_SEGMENT_COUNT] =
{
    {386, -77830}, {392, -78440}, {397, -79445}, {401, -80625}, {405, -82200},
    {409, -84215}, {411, -85425}, {413, -86785}, {415, -88375}, {416, -89310},
    {415, -88325}, {414, -87190}, {413, -85965},
};

static PiecewiseLinearSegment_t
    s_tc_k_measurement_segments[PRODUCT_TC_K_MEASUREMENT_SEGMENT_COUNT];
static PiecewiseLinearSegment_t
    s_tc_k_cjc_segments[PRODUCT_TC_K_CJC_SEGMENT_COUNT];
static bool s_tables_initialized;

static const PiecewiseLinearTable_t s_tc_k_measurement_table =
{
    s_tc_k_measurement_segments,
    PRODUCT_TC_K_MEASUREMENT_SEGMENT_COUNT,
    PRODUCT_TC_K_MEASUREMENT_COEFFICIENT_SCALE
};

static const PiecewiseLinearTable_t s_tc_k_cjc_table =
{
    s_tc_k_cjc_segments,
    PRODUCT_TC_K_CJC_SEGMENT_COUNT,
    PRODUCT_TC_K_CJC_COEFFICIENT_SCALE
};

static void InitializeTables(void)
{
    uint16_t index;

    if (s_tables_initialized)
    {
        return;
    }

    for (index = 0U; index < PRODUCT_TC_K_MEASUREMENT_SEGMENT_COUNT; index++)
    {
        int32_t slope = s_tc_k_measurement_coefficients[index].slope;
        s_tc_k_measurement_segments[index].x_min =
            s_tc_k_measurement_boundaries_shifted_uv[index] -
            PRODUCT_TC_K_INPUT_SHIFT_UV;
        s_tc_k_measurement_segments[index].x_max =
            s_tc_k_measurement_boundaries_shifted_uv[index + 1U] -
            PRODUCT_TC_K_INPUT_SHIFT_UV;
        s_tc_k_measurement_segments[index].slope = slope;
        s_tc_k_measurement_segments[index].intercept =
            ((int64_t)slope * PRODUCT_TC_K_INPUT_SHIFT_UV) +
            ((int64_t)s_tc_k_measurement_coefficients[index]
                 .intercept_millicelsius *
             PRODUCT_TC_K_MEASUREMENT_COEFFICIENT_SCALE);
    }

    for (index = 0U; index < PRODUCT_TC_K_CJC_SEGMENT_COUNT; index++)
    {
        int32_t slope = s_tc_k_cjc_coefficients[index].slope;
        s_tc_k_cjc_segments[index].x_min =
            PRODUCT_TC_K_CJC_MIN_MILLICELSIUS +
            ((int32_t)index * PRODUCT_TC_K_CJC_INTERVAL_MC);
        s_tc_k_cjc_segments[index].x_max =
            s_tc_k_cjc_segments[index].x_min + PRODUCT_TC_K_CJC_INTERVAL_MC;
        s_tc_k_cjc_segments[index].slope = slope;
        s_tc_k_cjc_segments[index].intercept =
            (((int64_t)slope * PRODUCT_TC_K_CJC_INPUT_SHIFT_DECICELSIUS) +
             s_tc_k_cjc_coefficients[index].intercept_deci_microvolts) *
            100LL;
    }

    s_tables_initialized = true;
}

const PiecewiseLinearTable_t *ProductTcKTable_GetMeasurementTable(void)
{
    InitializeTables();
    return &s_tc_k_measurement_table;
}

const PiecewiseLinearTable_t *ProductTcKTable_GetCjcTable(void)
{
    InitializeTables();
    return &s_tc_k_cjc_table;
}

bool ProductTcKTable_IsReady(void)
{
    InitializeTables();
    return PiecewiseLinearTable_IsValid(&s_tc_k_measurement_table) &&
           PiecewiseLinearTable_IsValid(&s_tc_k_cjc_table);
}

#else

const PiecewiseLinearTable_t *ProductTcKTable_GetMeasurementTable(void)
{
    return NULL;
}

const PiecewiseLinearTable_t *ProductTcKTable_GetCjcTable(void)
{
    return NULL;
}

bool ProductTcKTable_IsReady(void)
{
    return false;
}

#endif /* PRODUCT_TC_K_TABLE_COMPLETE */
