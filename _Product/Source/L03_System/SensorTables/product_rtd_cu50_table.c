#include "product_rtd_cu50_table.h"

#include <stddef.h>

#if PRODUCT_RTD_CU50_TABLE_COMPLETE

#if (PRODUCT_RTD_CU50_MEASUREMENT_SEGMENT_COUNT == 0U)
#error "Enter the CU50 measurement segment count"
#endif

typedef struct
{
    int32_t slope;
    int32_t intercept_millicelsius;
} ProductRtdCu50MeasurementCoefficient_t;

/*
 * TODO: USER TABLE DATA
 * Enter MEASUREMENT_SEGMENT_COUNT + 1 strictly increasing boundaries.
 * Each value is resistance_milliohm + PRODUCT_RTD_CU50_INPUT_SHIFT_MILLIOHM.
 */
static const int32_t s_rtd_cu50_measurement_boundaries_shifted_milliohm
    [PRODUCT_RTD_CU50_MEASUREMENT_BOUNDARY_COUNT] =
{
    /* TODO: resistance_0, resistance_1, ... resistance_N */
    19, 4300, 8580, 12860, 17140, 21420, 25700,
    29980, 34260, 38540, 42820, 47100, 51380,
};

/*
 * TODO: USER TABLE DATA
 * shifted_resistance_milliohm = resistance_milliohm + input_shift_milliohm
 * temperature_mC = slope * shifted_resistance_milliohm / 100 + intercept_mC
 */
static const ProductRtdCu50MeasurementCoefficient_t
    s_rtd_cu50_measurement_coefficients[PRODUCT_RTD_CU50_MEASUREMENT_SEGMENT_COUNT] =
{
    /* TODO: {slope_0, intercept_0}, ... {slope_N-1, intercept_N-1} */
    {467, -70088}, {467, -70075}, {467, -70063}, {467, -70050},
    {467, -70038}, {467, -70026}, {467, -70013}, {467, -70001},
    {467, -69988}, {467, -69976}, {467, -69967}, {467, -69951},
};

static PiecewiseLinearSegment_t
    s_rtd_cu50_measurement_segments[PRODUCT_RTD_CU50_MEASUREMENT_SEGMENT_COUNT];
static bool s_table_initialized;

static const PiecewiseLinearTable_t s_rtd_cu50_measurement_table =
{
    s_rtd_cu50_measurement_segments,
    PRODUCT_RTD_CU50_MEASUREMENT_SEGMENT_COUNT,
    PRODUCT_RTD_CU50_MEASUREMENT_COEFFICIENT_SCALE
};

static void InitializeTable(void)
{
    uint16_t index;

    if (s_table_initialized)
    {
        return;
    }

    for (index = 0U; index < PRODUCT_RTD_CU50_MEASUREMENT_SEGMENT_COUNT; index++)
    {
        s_rtd_cu50_measurement_segments[index].x_min =
            s_rtd_cu50_measurement_boundaries_shifted_milliohm[index] -
            PRODUCT_RTD_CU50_INPUT_SHIFT_MILLIOHM;
        s_rtd_cu50_measurement_segments[index].x_max =
            s_rtd_cu50_measurement_boundaries_shifted_milliohm[index + 1U] -
            PRODUCT_RTD_CU50_INPUT_SHIFT_MILLIOHM;
        s_rtd_cu50_measurement_segments[index].slope =
            s_rtd_cu50_measurement_coefficients[index].slope;
        s_rtd_cu50_measurement_segments[index].intercept =
            ((int64_t)s_rtd_cu50_measurement_coefficients[index].slope *
             PRODUCT_RTD_CU50_INPUT_SHIFT_MILLIOHM) +
            ((int64_t)s_rtd_cu50_measurement_coefficients[index]
                 .intercept_millicelsius *
             PRODUCT_RTD_CU50_MEASUREMENT_COEFFICIENT_SCALE);
    }

    s_table_initialized = true;
}

const PiecewiseLinearTable_t *ProductRtdCu50Table_GetMeasurementTable(void)
{
    InitializeTable();
    return &s_rtd_cu50_measurement_table;
}

bool ProductRtdCu50Table_IsReady(void)
{
    InitializeTable();
    return PiecewiseLinearTable_IsValid(&s_rtd_cu50_measurement_table);
}

#else

const PiecewiseLinearTable_t *ProductRtdCu50Table_GetMeasurementTable(void)
{
    return NULL;
}

bool ProductRtdCu50Table_IsReady(void)
{
    return false;
}

#endif /* PRODUCT_RTD_CU50_TABLE_COMPLETE */
