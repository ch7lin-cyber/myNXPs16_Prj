#include "product_rtd_pt1000_table.h"

#include <stddef.h>

#if PRODUCT_RTD_PT1000_TABLE_COMPLETE

#if (PRODUCT_RTD_PT1000_MEASUREMENT_SEGMENT_COUNT == 0U)
#error "Enter the PT1000 measurement segment count"
#endif

typedef struct
{
    int32_t slope;
    int32_t intercept_millicelsius;
} ProductRtdPt1000MeasurementCoefficient_t;

/*
 * TODO: USER TABLE DATA
 * Enter MEASUREMENT_SEGMENT_COUNT + 1 strictly increasing boundaries.
 * Each value is resistance_milliohm + PRODUCT_RTD_PT1000_INPUT_SHIFT_MILLIOHM.
 */
static const int32_t s_rtd_pt1000_measurement_boundaries_shifted_milliohm
    [PRODUCT_RTD_PT1000_MEASUREMENT_BOUNDARY_COUNT] =
{
    /* TODO: resistance_0, resistance_1, ... resistance_N */
-930,86300,172064,256533,339864,422198,503658,584354,664378,743807,//10						
822699,901100,979035,1056508,1133519,1210068,1286155,1361780,1436943,1511644,//20						
1585883,1659660,1732975,1805827,1878218,1950147,2021614,2092619,2163162,2233243,//30						
2302862,2372018,2440713,2508946,2576717,2644026,2710873,2777257,2843180,2908641,//40						
2973640,3038176,3102251,3165864,3229015,3291703,3353930,3415695,3476998,3537838,//50						
3598217,3658134,3717588,3776581,3835112,3893180,0,0,0,0,//60						

};

/*
 * TODO: USER TABLE DATA
 * shifted_resistance_milliohm = resistance_milliohm + input_shift_milliohm
 * temperature_mC = slope * shifted_resistance_milliohm / 100 + intercept_mC
 */
static const ProductRtdPt1000MeasurementCoefficient_t
    s_rtd_pt1000_measurement_coefficients[PRODUCT_RTD_PT1000_MEASUREMENT_SEGMENT_COUNT] =
{
    /* TODO: {slope_0, intercept_0}, ... {slope_N-1, intercept_N-1} */
{23,-219827},	{23,-219722},	{24,-221428},	{24,-221584},	{24,-221456},	//5
{25,-225758},	{25,-226018},	{25,-226100},	{25,-226028},	{25,-225822},	//10
{26,-234086},	{26,-234412},	{26,-234618},	{26,-234706},	{26,-234670},	//15
{26,-234516},	{26,-234241},	{27,-247822},	{27,-248057},	{27,-248167},	//20
{27,-248152},	{27,-248012},	{27,-247748},	{28,-265761},	{28,-265969},	//25
{28,-266052},	{28,-265997},	{28,-265817},	{29,-287472},	{29,-287732},	//30
{29,-287851},	{29,-287850},	{29,-287707},	{30,-312843},	{30,-313108},	//35
{30,-313235},	{30,-313223},	{30,-313073},	{31,-341527},	{31,-341752},	//40
{31,-341839},	{31,-341771},	{31,-341566},	{32,-373177},	{32,-373315},	//45
{32,-373305},	{32,-373148},	{33,-407290},	{33,-407448},	{33,-407453},	//50
{33,-407305},	{34,-443869},	{34,-444004},	{34,-443992},	{34,-443814},	//55

};

static PiecewiseLinearSegment_t
    s_rtd_pt1000_measurement_segments[PRODUCT_RTD_PT1000_MEASUREMENT_SEGMENT_COUNT];
static bool s_table_initialized;

static const PiecewiseLinearTable_t s_rtd_pt1000_measurement_table =
{
    s_rtd_pt1000_measurement_segments,
    PRODUCT_RTD_PT1000_MEASUREMENT_SEGMENT_COUNT,
    PRODUCT_RTD_PT1000_MEASUREMENT_COEFFICIENT_SCALE
};

static void InitializeTable(void)
{
    uint16_t index;

    if (s_table_initialized)
    {
        return;
    }

    for (index = 0U; index < PRODUCT_RTD_PT1000_MEASUREMENT_SEGMENT_COUNT; index++)
    {
        s_rtd_pt1000_measurement_segments[index].x_min =
            s_rtd_pt1000_measurement_boundaries_shifted_milliohm[index] -
            PRODUCT_RTD_PT1000_INPUT_SHIFT_MILLIOHM;
        s_rtd_pt1000_measurement_segments[index].x_max =
            s_rtd_pt1000_measurement_boundaries_shifted_milliohm[index + 1U] -
            PRODUCT_RTD_PT1000_INPUT_SHIFT_MILLIOHM;
        s_rtd_pt1000_measurement_segments[index].slope =
            s_rtd_pt1000_measurement_coefficients[index].slope;
        s_rtd_pt1000_measurement_segments[index].intercept =
            ((int64_t)s_rtd_pt1000_measurement_coefficients[index].slope *
             PRODUCT_RTD_PT1000_INPUT_SHIFT_MILLIOHM) +
            ((int64_t)s_rtd_pt1000_measurement_coefficients[index]
                 .intercept_millicelsius *
             PRODUCT_RTD_PT1000_MEASUREMENT_COEFFICIENT_SCALE);
    }

    s_table_initialized = true;
}

const PiecewiseLinearTable_t *ProductRtdPt1000Table_GetMeasurementTable(void)
{
    InitializeTable();
    return &s_rtd_pt1000_measurement_table;
}

bool ProductRtdPt1000Table_IsReady(void)
{
    InitializeTable();
    return PiecewiseLinearTable_IsValid(&s_rtd_pt1000_measurement_table);
}

#else

const PiecewiseLinearTable_t *ProductRtdPt1000Table_GetMeasurementTable(void)
{
    return NULL;
}

bool ProductRtdPt1000Table_IsReady(void)
{
    return false;
}

#endif /* PRODUCT_RTD_PT1000_TABLE_COMPLETE */
