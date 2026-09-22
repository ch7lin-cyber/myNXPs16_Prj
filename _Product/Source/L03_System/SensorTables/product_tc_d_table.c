#include "product_tc_d_table.h"

#include <stddef.h>

#if PRODUCT_TC_D_TABLE_COMPLETE

#if (PRODUCT_TC_D_MEASUREMENT_SEGMENT_COUNT == 0U)
#error "Enter the D measurement segment count"
#endif

typedef struct
{
    int32_t slope;
    int32_t intercept_millicelsius;
} ProductTcDMeasurementCoefficient_t;

typedef struct
{
    int32_t slope;
    int32_t intercept_deci_microvolts;
} ProductTcDCjcCoefficient_t;

/*
 * TODO: USER TABLE DATA
 * Enter MEASUREMENT_SEGMENT_COUNT + 1 strictly increasing boundaries.
 * Each value is input_uV + PRODUCT_TC_D_INPUT_SHIFT_UV.
 */
static const int32_t s_tc_d_measurement_boundaries_shifted_uv
    [PRODUCT_TC_D_MEASUREMENT_BOUNDARY_COUNT] =
{
    /* TODO: boundary_0, boundary_1, ... boundary_N */
0,200,400,615,845,1088,1345,1615,1896,2188,//10
2490,2803,3124,3453,3791,4136,4487,4845,5209,5578,//20
5952,6330,6713,7099,7489,7882,8278,8676,9077,9479,//30
9883,10288,10694,11101,11509,11916,12325,12733,13142,13551,//40
13960,14370,14780,15189,15598,16005,16412,16818,17223,17627,//50
18029,18430,18829,19227,19623,20018,20411,20802,21191,21579,//60
21965,22349,22731,23111,23489,23865,24240,24612,24983,25351,//70
25717,26082,26444,26805,27163,27519,27873,28226,28575,28923,//80
29269,29612,29953,30292,30628,30962,31293,31622,31949,32272,//90
32593,32912,33227,33539,33848,34154,34457,34757,35052,35344,//100
35633,35917,36197,36473,36744,37011,37273,37529,37780,38026,//110
38266,38499,38727,38947,39161,39367,39565,39754,
};

/*
 * TODO: USER TABLE DATA
 * temperature_mC = slope * shifted_uV / 100 + intercept_mC
 */
static const ProductTcDMeasurementCoefficient_t
    s_tc_d_measurement_coefficients[PRODUCT_TC_D_MEASUREMENT_SEGMENT_COUNT] =
{
    /* TODO: {slope_0, intercept_0}, ... {slope_N-1, intercept_N-1} */
{10000,-20000},{10000,-19850},{9302,-17089},{8696,-13378},{8230,-9457},//5
{7782,-4616},{7407,450},{7117,5136},{6849,10215},{6623,15124},//10
{6390,20930},{6231,25407},{6079,30131},{5917,35716},{5797,40282},//15
{5698,44372},{5587,49336},{5495,53805},{5420,57710},{5348,61726},//20
{5291,65111},{5222,69468},{5181,72205},{5128,75964},{5089,78881},//25
{5051,81875},{5025,84035},{4988,87244},{4975,88425},{4950,90788},//30
{4938,91978},{4926,93208},{4914,94488},{4902,95839},{4914,94449},//35
{4890,97308},{4902,95838},{4890,97365},{4890,97374},{4890,97362},//40
{4878,99019},{4878,99032},{4890,97259},{4890,97269},{4914,93514},//45
{4914,93493},{4926,91528},{4938,89515},{4950,87472},{4975,83064},//50
{4988,80719},{5013,76099},{5025,73838},{5051,68838},{5063,66488},//55
{5089,61296},{5115,55984},{5141,50561},{5155,47593},{5181,42000},//60
{5208,36079},{5236,29816},{5263,23668},{5291,17192},{5319,10604},//65
{5333,7274},{5376,-3142},{5391,-6837},{5435,-17826},{5460,-24179},//70
{5479,-29051},{5525,-41047},{5540,-45021},{5587,-57602},{5618,-66040},//75
{5650,-74855},{5666,-79288},{5731,-97646},{5747,-102228},{5780,-111746},//80
{5831,-126673},{5865,-136754},{5900,-147232},{5952,-162973},{5988,-174014},//85
{6042,-190743},{6079,-202330},{6116,-214005},{6192,-238296},{6231,-250895},//90
{6270,-263590},{6349,-289575},{6410,-309857},{6472,-330678},{6536,-352345},//95
{6601,-374542},{6667,-397249},{6780,-436527},{6849,-460752},{6920,-485824},//100
{7042,-529290},{7143,-565587},{7246,-602863},{7380,-651742},{7491,-692530},//105
{7634,-745447},{7813,-812164},{7968,-870374},{8130,-931571},{8333,-1008752},//110
{8584,-1104813},{8772,-1177185},{9091,-1300717},{9346,-1400027},{9709,-1542171},//115
{10101,-1696516},{10582,-1886866},
};

/*
 * TODO: USER TABLE DATA
 * shifted_temperature_deci_C = temperature_deci_C + 200
 * cjc_deci_uV = slope * shifted_temperature_deci_C + intercept_deci_uV
 */
static const ProductTcDCjcCoefficient_t
    s_tc_d_cjc_coefficients[PRODUCT_TC_D_CJC_SEGMENT_COUNT] =
{
    /* TODO: 13 rows: {slope_0, intercept_0}, ... {slope_12, intercept_12} */
    {100,-20000}, {100,-20000}, { 98,-19640}, {102,-20850}, {105,-22000},
    {110,-24500}, {113,-26330}, {117,-29130}, {120,-31550}, {123,-34215},
    {127,-38195}, {130,-41500}, {133,-45085},
};

static PiecewiseLinearSegment_t
    s_tc_d_measurement_segments[PRODUCT_TC_D_MEASUREMENT_SEGMENT_COUNT];
static PiecewiseLinearSegment_t
    s_tc_d_cjc_segments[PRODUCT_TC_D_CJC_SEGMENT_COUNT];
static bool s_tables_initialized;

static const PiecewiseLinearTable_t s_tc_d_measurement_table =
{
    s_tc_d_measurement_segments,
    PRODUCT_TC_D_MEASUREMENT_SEGMENT_COUNT,
    PRODUCT_TC_D_MEASUREMENT_COEFFICIENT_SCALE
};

static const PiecewiseLinearTable_t s_tc_d_cjc_table =
{
    s_tc_d_cjc_segments,
    PRODUCT_TC_D_CJC_SEGMENT_COUNT,
    PRODUCT_TC_D_CJC_COEFFICIENT_SCALE
};

static void InitializeTables(void)
{
    uint16_t index;

    if (s_tables_initialized)
    {
        return;
    }

    for (index = 0U; index < PRODUCT_TC_D_MEASUREMENT_SEGMENT_COUNT; index++)
    {
        int32_t slope = s_tc_d_measurement_coefficients[index].slope;
        s_tc_d_measurement_segments[index].x_min =
            s_tc_d_measurement_boundaries_shifted_uv[index] -
            PRODUCT_TC_D_INPUT_SHIFT_UV;
        s_tc_d_measurement_segments[index].x_max =
            s_tc_d_measurement_boundaries_shifted_uv[index + 1U] -
            PRODUCT_TC_D_INPUT_SHIFT_UV;
        s_tc_d_measurement_segments[index].slope = slope;
        s_tc_d_measurement_segments[index].intercept =
            ((int64_t)slope * PRODUCT_TC_D_INPUT_SHIFT_UV) +
            ((int64_t)s_tc_d_measurement_coefficients[index]
                 .intercept_millicelsius *
             PRODUCT_TC_D_MEASUREMENT_COEFFICIENT_SCALE);
    }

    for (index = 0U; index < PRODUCT_TC_D_CJC_SEGMENT_COUNT; index++)
    {
        int32_t slope = s_tc_d_cjc_coefficients[index].slope;
        s_tc_d_cjc_segments[index].x_min =
            PRODUCT_TC_D_CJC_MIN_MILLICELSIUS +
            ((int32_t)index * PRODUCT_TC_D_CJC_INTERVAL_MC);
        s_tc_d_cjc_segments[index].x_max =
            s_tc_d_cjc_segments[index].x_min + PRODUCT_TC_D_CJC_INTERVAL_MC;
        s_tc_d_cjc_segments[index].slope = slope;
        s_tc_d_cjc_segments[index].intercept =
            (((int64_t)slope * PRODUCT_TC_D_CJC_INPUT_SHIFT_DECICELSIUS) +
             s_tc_d_cjc_coefficients[index].intercept_deci_microvolts) *
            100LL;
    }

    s_tables_initialized = true;
}

const PiecewiseLinearTable_t *ProductTcDTable_GetMeasurementTable(void)
{
    InitializeTables();
    return &s_tc_d_measurement_table;
}

const PiecewiseLinearTable_t *ProductTcDTable_GetCjcTable(void)
{
    InitializeTables();
    return &s_tc_d_cjc_table;
}

bool ProductTcDTable_IsReady(void)
{
    InitializeTables();
    return PiecewiseLinearTable_IsValid(&s_tc_d_measurement_table) &&
           PiecewiseLinearTable_IsValid(&s_tc_d_cjc_table);
}

#else

const PiecewiseLinearTable_t *ProductTcDTable_GetMeasurementTable(void)
{
    return NULL;
}

const PiecewiseLinearTable_t *ProductTcDTable_GetCjcTable(void)
{
    return NULL;
}

bool ProductTcDTable_IsReady(void)
{
    return false;
}

#endif /* PRODUCT_TC_D_TABLE_COMPLETE */

