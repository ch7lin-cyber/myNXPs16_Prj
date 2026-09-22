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
 * Enter MEASUREMENT_SEGMENT_COUNT + 1 strictly increasing resistance
 * boundaries. Unit: milliohm.
 */
static const int32_t s_rtd_pt1000_measurement_boundaries_milliohm
    [PRODUCT_RTD_PT1000_MEASUREMENT_BOUNDARY_COUNT] =
{
    /* TODO: resistance_0, resistance_1, ... resistance_N */
};

/*
 * TODO: USER TABLE DATA
 * temperature_mC = slope * resistance_milliohm / 100 + intercept_mC
 */
static const ProductRtdPt1000MeasurementCoefficient_t
    s_rtd_pt1000_measurement_coefficients[PRODUCT_RTD_PT1000_MEASUREMENT_SEGMENT_COUNT] =
{
    /* TODO: {slope_0, intercept_0}, ... {slope_N-1, intercept_N-1} */
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
            s_rtd_pt1000_measurement_boundaries_milliohm[index];
        s_rtd_pt1000_measurement_segments[index].x_max =
            s_rtd_pt1000_measurement_boundaries_milliohm[index + 1U];
        s_rtd_pt1000_measurement_segments[index].slope =
            s_rtd_pt1000_measurement_coefficients[index].slope;
        s_rtd_pt1000_measurement_segments[index].intercept =
            (int64_t)s_rtd_pt1000_measurement_coefficients[index]
                .intercept_millicelsius *
            PRODUCT_RTD_PT1000_MEASUREMENT_COEFFICIENT_SCALE;
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

