#include "product_tc_b_table.h"

#include <stddef.h>

#if PRODUCT_TC_B_TABLE_COMPLETE

#if (PRODUCT_TC_B_MEASUREMENT_SEGMENT_COUNT == 0U)
#error "Enter the B measurement segment count"
#endif

typedef struct
{
    int32_t slope;
    int32_t intercept_millicelsius;
} ProductTcBMeasurementCoefficient_t;

typedef struct
{
    int32_t slope;
    int32_t intercept_deci_microvolts;
} ProductTcBCjcCoefficient_t;

/*
 * TODO: USER TABLE DATA
 * Enter MEASUREMENT_SEGMENT_COUNT + 1 strictly increasing boundaries.
 * Each value is input_uV + PRODUCT_TC_B_INPUT_SHIFT_UV.
 */
static const int32_t s_tc_b_measurement_boundaries_shifted_uv
    [PRODUCT_TC_B_MEASUREMENT_BOUNDARY_COUNT] =
{
    /* TODO: boundary_0, boundary_1, ... boundary_N */
  98,  100,   97,  100,  106,  117,  133,  153,  178,  207,//10
    241,  278,  320,  367,  417,  472,  531,  594,  661,  732,//20
    807,  887,  970, 1057, 1148, 1243, 1342, 1444, 1551, 1661,//30
    1775, 1892, 2013, 2137, 2265, 2396, 2531, 2669, 2810, 2954,//40
    3102, 3254, 3408, 3566, 3726, 3890, 4057, 4227, 4399, 4575,//50
    4753, 4934, 5118, 5305, 5494, 5685, 5880, 6076, 6275, 6477,//60
    6680, 6886, 7095, 7305, 7517, 7732, 7948, 8166, 8386, 8608,//70
    8831, 9056, 9282, 9510, 9739, 9968, 10199, 10431, 10663, 10896, //80
    11129, 11363, 11597, 11831, 12065, 12299, 12533, 12766, 12998, 13230, //90
    13461, 13691, 13920,
};

/*
 * TODO: USER TABLE DATA
 * temperature_mC = slope * shifted_uV / 100 + intercept_mC
 */
static const ProductTcBMeasurementCoefficient_t
    s_tc_b_measurement_coefficients[PRODUCT_TC_B_MEASUREMENT_SEGMENT_COUNT] =
{
    /* TODO: {slope_0, intercept_0}, ... {slope_N-1, intercept_N-1} */
  {1000000, -995000}, {-666667, 665834}, {666667, -623834}, {333333, -291833}, {181818, -132682}, //5
    { 125000, -66250}, { 100000, -33000}, { 80000,  -2400}, { 68966,  17378}, { 58824,  38470}, //10
    {  54054,  50027}, {  47619, 67691}, { 42553,  83969}, { 40000,  93400}, { 36364, 108498}, //15
    {  33898, 120146}, {  31746, 131643}, { 29851, 142864}, { 28169, 153937}, { 26667, 164831}, //20
    {  25000, 178375}, {  24096, 186395}, { 22989, 197070}, { 21978, 207775}, { 21053, 218364}, //25
    {  20202, 228945}, {  19608, 236887}, { 18692, 250111}, { 18182, 258088}, { 17544, 268691}, //30
    {  17094, 276581}, {  16529, 287333}, { 16129, 295356}, { 15625, 306141}, { 15267, 314252}, //35
    {  14815, 325088}, {  14493, 333269}, { 14184, 341483}, { 13889, 349733}, { 13514, 360783}, //40
    {  13158, 371865}, {  12987, 377461}, { 12658, 388666}, { 12500, 394250}, { 12195, 405614}, //45
    {  11976, 414149}, {  11765, 422752}, { 11628, 428496}, { 11364, 440097}, { 11236, 445975}, //50
    {  11050, 454796}, {  10870, 463663}, { 10695, 472665}, { 10582, 478686}, { 10471, 484739}, //55
    {  10256, 496972}, {  10204, 500035}, { 10050, 509375}, {  9901, 518742}, {  9852, 521901}, //60
    {   9709, 531419}, {   9569, 541084}, {  9524, 544296}, {  9434, 550837}, {  9302, 560769}, //65
    {   9259, 564095}, {   9174, 570823}, {  9091, 577629}, {  9009, 584483}, {  8962, 588547}, //70
    {   8889, 594990}, {   8850, 598530}, {  8772, 605783}, {  8734, 609425}, {  8734, 609411}, //75
    {   8658, 616960}, {   8621, 620761}, {  8621, 620769}, {  8584, 624699}, {  8584, 624683}, //80
    {   8547, 628796}, {   8547, 628809}, {  8547, 628804}, {  8547, 628800}, {  8547, 628800}, //85
    {   8547, 628822}, {   8584, 624195}, {  8621, 619443}, {  8621, 619416}, {  8658, 614523}, //90
    {   8696, 609409}, {   8734, 604195},
};

/*
 * TODO: USER TABLE DATA
 * shifted_temperature_deci_C = temperature_deci_C + 200
 * cjc_deci_uV = slope * shifted_temperature_deci_C + intercept_deci_uV
 */
static const ProductTcBCjcCoefficient_t
    s_tc_b_cjc_coefficients[PRODUCT_TC_B_CJC_SEGMENT_COUNT] =
{
    /* TODO: 13 rows: {slope_0, intercept_0}, ... {slope_12, intercept_12} */
    {1, -210}, {1, -285}, {-2, 400}, {-1, 125}, {1, -695},
    {2, -1240}, {2, -1220}, {4, -2590}, {5, -3375}, {6, -4280},
    {8, -6300}, {8, -6310}, {10, -8710},
};

static PiecewiseLinearSegment_t
    s_tc_b_measurement_segments[PRODUCT_TC_B_MEASUREMENT_SEGMENT_COUNT];
static PiecewiseLinearSegment_t
    s_tc_b_cjc_segments[PRODUCT_TC_B_CJC_SEGMENT_COUNT];
static bool s_tables_initialized;

static const PiecewiseLinearTable_t s_tc_b_measurement_table =
{
    s_tc_b_measurement_segments,
    PRODUCT_TC_B_MEASUREMENT_SEGMENT_COUNT,
    PRODUCT_TC_B_MEASUREMENT_COEFFICIENT_SCALE
};

static const PiecewiseLinearTable_t s_tc_b_cjc_table =
{
    s_tc_b_cjc_segments,
    PRODUCT_TC_B_CJC_SEGMENT_COUNT,
    PRODUCT_TC_B_CJC_COEFFICIENT_SCALE
};

static void InitializeTables(void)
{
    uint16_t index;

    if (s_tables_initialized)
    {
        return;
    }

    for (index = 0U; index < PRODUCT_TC_B_MEASUREMENT_SEGMENT_COUNT; index++)
    {
        int32_t slope = s_tc_b_measurement_coefficients[index].slope;
        s_tc_b_measurement_segments[index].x_min =
            s_tc_b_measurement_boundaries_shifted_uv[index] -
            PRODUCT_TC_B_INPUT_SHIFT_UV;
        s_tc_b_measurement_segments[index].x_max =
            s_tc_b_measurement_boundaries_shifted_uv[index + 1U] -
            PRODUCT_TC_B_INPUT_SHIFT_UV;
        s_tc_b_measurement_segments[index].slope = slope;
        s_tc_b_measurement_segments[index].intercept =
            ((int64_t)slope * PRODUCT_TC_B_INPUT_SHIFT_UV) +
            ((int64_t)s_tc_b_measurement_coefficients[index]
                 .intercept_millicelsius *
             PRODUCT_TC_B_MEASUREMENT_COEFFICIENT_SCALE);
    }

    for (index = 0U; index < PRODUCT_TC_B_CJC_SEGMENT_COUNT; index++)
    {
        int32_t slope = s_tc_b_cjc_coefficients[index].slope;
        s_tc_b_cjc_segments[index].x_min =
            PRODUCT_TC_B_CJC_MIN_MILLICELSIUS +
            ((int32_t)index * PRODUCT_TC_B_CJC_INTERVAL_MC);
        s_tc_b_cjc_segments[index].x_max =
            s_tc_b_cjc_segments[index].x_min + PRODUCT_TC_B_CJC_INTERVAL_MC;
        s_tc_b_cjc_segments[index].slope = slope;
        s_tc_b_cjc_segments[index].intercept =
            (((int64_t)slope * PRODUCT_TC_B_CJC_INPUT_SHIFT_DECICELSIUS) +
             s_tc_b_cjc_coefficients[index].intercept_deci_microvolts) *
            100LL;
    }

    s_tables_initialized = true;
}

const PiecewiseLinearTable_t *ProductTcBTable_GetMeasurementTable(void)
{
    InitializeTables();
    return &s_tc_b_measurement_table;
}

const PiecewiseLinearTable_t *ProductTcBTable_GetCjcTable(void)
{
    InitializeTables();
    return &s_tc_b_cjc_table;
}

bool ProductTcBTable_IsReady(void)
{
    InitializeTables();
    return PiecewiseLinearTable_IsValid(&s_tc_b_measurement_table) &&
           PiecewiseLinearTable_IsValid(&s_tc_b_cjc_table);
}

#else

const PiecewiseLinearTable_t *ProductTcBTable_GetMeasurementTable(void)
{
    return NULL;
}

const PiecewiseLinearTable_t *ProductTcBTable_GetCjcTable(void)
{
    return NULL;
}

bool ProductTcBTable_IsReady(void)
{
    return false;
}

#endif /* PRODUCT_TC_B_TABLE_COMPLETE */

