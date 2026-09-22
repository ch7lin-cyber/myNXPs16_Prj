#include "product_rtd_ni120_table.h"

#include <stddef.h>

#if PRODUCT_RTD_NI120_TABLE_COMPLETE

#if (PRODUCT_RTD_NI120_MEASUREMENT_SEGMENT_COUNT == 0U)
#error "Enter the NI120 measurement segment count"
#endif

typedef struct
{
    int32_t slope;
    int32_t intercept_millicelsius;
} ProductRtdNi120MeasurementCoefficient_t;

/*
 * TODO: USER TABLE DATA
 * Enter MEASUREMENT_SEGMENT_COUNT + 1 strictly increasing boundaries.
 * Each value is resistance_milliohm + PRODUCT_RTD_NI120_INPUT_SHIFT_MILLIOHM.
 */
static const int32_t s_rtd_ni120_measurement_boundaries_shifted_milliohm
    [PRODUCT_RTD_NI120_MEASUREMENT_BOUNDARY_COUNT] =
{
    /* TODO: resistance_0, resistance_1, ... resistance_N */
    99, 13100, 26120, 39260, 52650, 66500, 81020, 96290, 112400, 129340, //10
    147140, 165790, 185350, 205800, 227270, 249960, 274030, 299640, 326810, 355570, //20
    385940, 416720,
};

/*
 * TODO: USER TABLE DATA
 * shifted_resistance_milliohm = resistance_milliohm + input_shift_milliohm
 * temperature_mC = slope * shifted_resistance_milliohm / 100 + intercept_mC
 */
static const ProductRtdNi120MeasurementCoefficient_t
    s_rtd_ni120_measurement_coefficients[PRODUCT_RTD_NI120_MEASUREMENT_SEGMENT_COUNT] =
{
    /* TODO: {slope_0, intercept_0}, ... {slope_N-1, intercept_N-1} */
    {154, -100163}, {154, -100199}, {152, -99676}, {149, -98448}, {144, -95742}, //5
    {138, -91739}, {131, -86061}, {124, -79326}, {118, -72557}, {112, -64786}, //10
    {107, -57365}, {102, -49037}, { 98, -41598}, { 93, -31318}, { 88, -19921}, //15
    { 83,  -7385}, { 78,   6340}, { 74, 18280}, { 70, 31223}, { 66, 45371}, //20
    { 65,  49152},
};

static PiecewiseLinearSegment_t
    s_rtd_ni120_measurement_segments[PRODUCT_RTD_NI120_MEASUREMENT_SEGMENT_COUNT];
static bool s_table_initialized;

static const PiecewiseLinearTable_t s_rtd_ni120_measurement_table =
{
    s_rtd_ni120_measurement_segments,
    PRODUCT_RTD_NI120_MEASUREMENT_SEGMENT_COUNT,
    PRODUCT_RTD_NI120_MEASUREMENT_COEFFICIENT_SCALE
};

static void InitializeTable(void)
{
    uint16_t index;

    if (s_table_initialized)
    {
        return;
    }

    for (index = 0U; index < PRODUCT_RTD_NI120_MEASUREMENT_SEGMENT_COUNT; index++)
    {
        s_rtd_ni120_measurement_segments[index].x_min =
            s_rtd_ni120_measurement_boundaries_shifted_milliohm[index] -
            PRODUCT_RTD_NI120_INPUT_SHIFT_MILLIOHM;
        s_rtd_ni120_measurement_segments[index].x_max =
            s_rtd_ni120_measurement_boundaries_shifted_milliohm[index + 1U] -
            PRODUCT_RTD_NI120_INPUT_SHIFT_MILLIOHM;
        s_rtd_ni120_measurement_segments[index].slope =
            s_rtd_ni120_measurement_coefficients[index].slope;
        s_rtd_ni120_measurement_segments[index].intercept =
            ((int64_t)s_rtd_ni120_measurement_coefficients[index].slope *
             PRODUCT_RTD_NI120_INPUT_SHIFT_MILLIOHM) +
            ((int64_t)s_rtd_ni120_measurement_coefficients[index]
                 .intercept_millicelsius *
             PRODUCT_RTD_NI120_MEASUREMENT_COEFFICIENT_SCALE);
    }

    s_table_initialized = true;
}

const PiecewiseLinearTable_t *ProductRtdNi120Table_GetMeasurementTable(void)
{
    InitializeTable();
    return &s_rtd_ni120_measurement_table;
}

bool ProductRtdNi120Table_IsReady(void)
{
    InitializeTable();
    return PiecewiseLinearTable_IsValid(&s_rtd_ni120_measurement_table);
}

#else

const PiecewiseLinearTable_t *ProductRtdNi120Table_GetMeasurementTable(void)
{
    return NULL;
}

bool ProductRtdNi120Table_IsReady(void)
{
    return false;
}

#endif /* PRODUCT_RTD_NI120_TABLE_COMPLETE */
