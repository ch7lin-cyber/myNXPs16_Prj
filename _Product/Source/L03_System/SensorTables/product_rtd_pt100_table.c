#include "product_rtd_pt100_table.h"

#include <stddef.h>

#if PRODUCT_RTD_PT100_TABLE_COMPLETE

#if (PRODUCT_RTD_PT100_MEASUREMENT_SEGMENT_COUNT == 0U)
#error "Enter the PT100 measurement segment count"
#endif

typedef struct
{
    int32_t slope;
    int32_t intercept_millicelsius;
} ProductRtdPt100MeasurementCoefficient_t;

/*
 * TODO: USER TABLE DATA
 * Enter MEASUREMENT_SEGMENT_COUNT + 1 strictly increasing boundaries.
 * Each value is resistance_milliohm + PRODUCT_RTD_PT100_INPUT_SHIFT_MILLIOHM.
 */
static const int32_t s_rtd_pt100_measurement_boundaries_shifted_milliohm
    [PRODUCT_RTD_PT100_MEASUREMENT_BOUNDARY_COUNT] =
{
    /* TODO: resistance_0, resistance_1, ... resistance_N */
    100,  8720, 17300, 25740, 34080, 42310, 50460, 58530, 66530, 74470,//10
    82360, 90200, 97990, 105740, 113440, 121100, 128710, 136270, 143780, 151250, //20
    158680, 166060, 173390, 180670, 187910, 195100, 202250, 209350, 216410, 223410, //30
    230380, 237290, 244160, 250980, 257760, 264490, 271180, 277820, 284410, 290950, //40
    297450, 303910, 310320, 316680, 322990, 329260, 335480, 341660, 347790, 353870, //50
    359910, 365900, 371850, 377750, 383610, 389470,
};

/*
 * TODO: USER TABLE DATA
 * shifted_resistance_milliohm = resistance_milliohm + input_shift_milliohm
 * temperature_mC = slope * shifted_resistance_milliohm / 100 + intercept_mC
 */
static const ProductRtdPt100MeasurementCoefficient_t
    s_rtd_pt100_measurement_coefficients[PRODUCT_RTD_PT100_MEASUREMENT_SEGMENT_COUNT] =
{
    /* TODO: {slope_0, intercept_0}, ... {slope_N-1, intercept_N-1} */
    {232, -220242},  {233, -220337},  {237, -221028},  {240, -221800},  {243, -222828}, //5
    {245, -223645},  {248, -225164},  {250, -226325},  {252, -227670},  {253, -228397}, //10
    {255, -230026},  {257, -231835},  {258, -232821},  {260, -234942},  {261, -236094}, //15
    {263, -238505},  {265, -241096},  {266, -242486},  {268, -245357},  {269, -246865}, //20
    {271, -250021},  {273, -253355},  {275, -256836},  {276, -258655},  {278, -262386}, //25
    {280, -266292},  {282, -270368},  {283, -272441},  {286, -278954},  {287, -281196}, //30
    {289, -285780},  {291, -290532},  {293, -295396},  {295, -300415},  {297, -305546}, //35
    {299, -310838},  {301, -316238},  {303, -321779},  {306, -330318},  {308, -336147}, //40
    {310, -342116},  {312, -348218},  {314, -354390},  {317, -363878},  {319, -370346}, //45
    {322, -380240},  {324, -386987},  {326, -393808},  {329, -404238},  {331, -411317}, //50
    {334, -422124},  {336, -429431},  {339, -440555},  {341, -448119},  {341, -448101}, //55

};

static PiecewiseLinearSegment_t
    s_rtd_pt100_measurement_segments[PRODUCT_RTD_PT100_MEASUREMENT_SEGMENT_COUNT];
static bool s_table_initialized;

static const PiecewiseLinearTable_t s_rtd_pt100_measurement_table =
{
    s_rtd_pt100_measurement_segments,
    PRODUCT_RTD_PT100_MEASUREMENT_SEGMENT_COUNT,
    PRODUCT_RTD_PT100_MEASUREMENT_COEFFICIENT_SCALE
};

static void InitializeTable(void)
{
    uint16_t index;

    if (s_table_initialized)
    {
        return;
    }

    for (index = 0U; index < PRODUCT_RTD_PT100_MEASUREMENT_SEGMENT_COUNT; index++)
    {
        s_rtd_pt100_measurement_segments[index].x_min =
            s_rtd_pt100_measurement_boundaries_shifted_milliohm[index] -
            PRODUCT_RTD_PT100_INPUT_SHIFT_MILLIOHM;
        s_rtd_pt100_measurement_segments[index].x_max =
            s_rtd_pt100_measurement_boundaries_shifted_milliohm[index + 1U] -
            PRODUCT_RTD_PT100_INPUT_SHIFT_MILLIOHM;
        s_rtd_pt100_measurement_segments[index].slope =
            s_rtd_pt100_measurement_coefficients[index].slope;
        s_rtd_pt100_measurement_segments[index].intercept =
            ((int64_t)s_rtd_pt100_measurement_coefficients[index].slope *
             PRODUCT_RTD_PT100_INPUT_SHIFT_MILLIOHM) +
            ((int64_t)s_rtd_pt100_measurement_coefficients[index]
                 .intercept_millicelsius *
             PRODUCT_RTD_PT100_MEASUREMENT_COEFFICIENT_SCALE);
    }

    s_table_initialized = true;
}

const PiecewiseLinearTable_t *ProductRtdPt100Table_GetMeasurementTable(void)
{
    InitializeTable();
    return &s_rtd_pt100_measurement_table;
}

bool ProductRtdPt100Table_IsReady(void)
{
    InitializeTable();
    return PiecewiseLinearTable_IsValid(&s_rtd_pt100_measurement_table);
}

#else

const PiecewiseLinearTable_t *ProductRtdPt100Table_GetMeasurementTable(void)
{
    return NULL;
}

bool ProductRtdPt100Table_IsReady(void)
{
    return false;
}

#endif /* PRODUCT_RTD_PT100_TABLE_COMPLETE */
