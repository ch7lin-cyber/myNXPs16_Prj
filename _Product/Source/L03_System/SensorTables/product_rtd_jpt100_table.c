#include "product_rtd_jpt100_table.h"

#include <stddef.h>

#if PRODUCT_RTD_JPT100_TABLE_COMPLETE

#if (PRODUCT_RTD_JPT100_MEASUREMENT_SEGMENT_COUNT == 0U)
#error "The JPT100 measurement segment count must not be zero"
#endif

typedef struct
{
    int32_t slope;
    int32_t intercept_millicelsius;
} ProductRtdJpt100MeasurementCoefficient_t;

/* Values are resistance_milliohm + PRODUCT_RTD_JPT100_INPUT_SHIFT_MILLIOHM. */
static const int32_t s_rtd_jpt100_measurement_boundaries_shifted_milliohm
    [PRODUCT_RTD_JPT100_MEASUREMENT_BOUNDARY_COUNT] =
{
    636, 9301, 17918, 26409, 34785, 43067, 51272, 59412, 67494, 75522,
    83500, 91428, 99309, 107140, 114924, 122660, 130348, 137988, 145582, 153128,
    160628, 168081, 175487, 182847, 190160, 197427, 204647, 211820, 218947, 226027,
    233060, 240048, 246985, 253876, 260720, 267517, 274266, 280967, 287618, 294222,
    300777,
};

/* temperature_mC = slope * shifted_resistance_milliohm / scale + intercept_mC. */
static const ProductRtdJpt100MeasurementCoefficient_t
    s_rtd_jpt100_measurement_coefficients[PRODUCT_RTD_JPT100_MEASUREMENT_SEGMENT_COUNT] =
{
    {231, -201470}, {232, -201594}, {236, -202318}, {239, -203139}, {241, -203817},
    {244, -205104}, {246, -206150}, {247, -206735}, {249, -208063}, {251, -209576},
    {252, -210416}, {254, -212242}, {255, -213229}, {257, -215360}, {259, -217673},
    {260, -218917}, {262, -221525}, {263, -222900}, {265, -225800}, {267, -228869},
    {268, -230464}, {270, -233827}, {272, -237349}, {273, -239160}, {275, -242939},
    {277, -246881}, {279, -250977}, {281, -255231}, {282, -257417}, {284, -261909},
    {286, -266550}, {288, -271332}, {290, -276257}, {292, -281320}, {294, -286517},
    {296, -291846}, {298, -297302}, {301, -305726}, {303, -311497}, {305, -317384},
};

static PiecewiseLinearSegment_t
    s_rtd_jpt100_measurement_segments[PRODUCT_RTD_JPT100_MEASUREMENT_SEGMENT_COUNT];
static bool s_table_initialized;

static const PiecewiseLinearTable_t s_rtd_jpt100_measurement_table =
{
    s_rtd_jpt100_measurement_segments,
    PRODUCT_RTD_JPT100_MEASUREMENT_SEGMENT_COUNT,
    PRODUCT_RTD_JPT100_MEASUREMENT_COEFFICIENT_SCALE
};

static void InitializeTable(void)
{
    uint16_t index;

    if (s_table_initialized)
    {
        return;
    }

    for (index = 0U; index < PRODUCT_RTD_JPT100_MEASUREMENT_SEGMENT_COUNT; index++)
    {
        int32_t slope = s_rtd_jpt100_measurement_coefficients[index].slope;
        s_rtd_jpt100_measurement_segments[index].x_min =
            s_rtd_jpt100_measurement_boundaries_shifted_milliohm[index] -
            PRODUCT_RTD_JPT100_INPUT_SHIFT_MILLIOHM;
        s_rtd_jpt100_measurement_segments[index].x_max =
            s_rtd_jpt100_measurement_boundaries_shifted_milliohm[index + 1U] -
            PRODUCT_RTD_JPT100_INPUT_SHIFT_MILLIOHM;
        s_rtd_jpt100_measurement_segments[index].slope = slope;
        s_rtd_jpt100_measurement_segments[index].intercept =
            ((int64_t)slope * PRODUCT_RTD_JPT100_INPUT_SHIFT_MILLIOHM) +
            ((int64_t)s_rtd_jpt100_measurement_coefficients[index]
                 .intercept_millicelsius *
             PRODUCT_RTD_JPT100_MEASUREMENT_COEFFICIENT_SCALE);
    }

    s_table_initialized = true;
}

const PiecewiseLinearTable_t *ProductRtdJpt100Table_GetMeasurementTable(void)
{
    InitializeTable();
    return &s_rtd_jpt100_measurement_table;
}

bool ProductRtdJpt100Table_IsReady(void)
{
    InitializeTable();
    return PiecewiseLinearTable_IsValid(&s_rtd_jpt100_measurement_table);
}

#else

const PiecewiseLinearTable_t *ProductRtdJpt100Table_GetMeasurementTable(void)
{
    return NULL;
}

bool ProductRtdJpt100Table_IsReady(void)
{
    return false;
}

#endif /* PRODUCT_RTD_JPT100_TABLE_COMPLETE */
