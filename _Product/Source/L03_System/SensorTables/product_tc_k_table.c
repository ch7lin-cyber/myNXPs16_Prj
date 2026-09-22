#include "product_tc_k_table.h"

#include <stddef.h>

/*
 * K-type table storage.
 *
 * Measurement row units:
 *   {input_min_uV, input_max_uV, slope_q, intercept_q}
 *   temperature_mC = (slope_q * input_uV + intercept_q) / 1000000
 *
 * CJC row units:
 *   {input_min_mC, input_max_mC, slope_q, intercept_q}
 *   cjc_uV = (slope_q * input_mC + intercept_q) / 1000000
 *
 * The first matrix covers -200 to 1300 degree C in 20 degree C segments.
 * The second matrix covers the approved CJC range in 10 degree C segments.
 * Keep PRODUCT_TC_K_TABLE_READY at zero until all rows and boundary tests are
 * present. Returning NULL prevents incomplete calibration data being used.
 */
#define PRODUCT_TC_K_TABLE_READY (0U)

#if (PRODUCT_TC_K_TABLE_READY == 1U)
static const PiecewiseLinearSegment_t s_tc_k_measurement_segments[] =
{
    /* {input_min_uV, input_max_uV, slope_q, intercept_q}, */
};

static const PiecewiseLinearSegment_t s_tc_k_cjc_segments[] =
{
    /* {input_min_mC, input_max_mC, slope_q, intercept_q}, */
};

static const PiecewiseLinearTable_t s_tc_k_measurement_table =
{
    s_tc_k_measurement_segments,
    (uint16_t)(sizeof(s_tc_k_measurement_segments) /
               sizeof(s_tc_k_measurement_segments[0])),
    PRODUCT_TC_K_COEFFICIENT_SCALE
};

static const PiecewiseLinearTable_t s_tc_k_cjc_table =
{
    s_tc_k_cjc_segments,
    (uint16_t)(sizeof(s_tc_k_cjc_segments) /
               sizeof(s_tc_k_cjc_segments[0])),
    PRODUCT_TC_K_COEFFICIENT_SCALE
};
#endif

const PiecewiseLinearTable_t *ProductTcKTable_GetMeasurementTable(void)
{
#if (PRODUCT_TC_K_TABLE_READY == 1U)
    return &s_tc_k_measurement_table;
#else
    return NULL;
#endif
}

const PiecewiseLinearTable_t *ProductTcKTable_GetCjcTable(void)
{
#if (PRODUCT_TC_K_TABLE_READY == 1U)
    return &s_tc_k_cjc_table;
#else
    return NULL;
#endif
}

bool ProductTcKTable_IsReady(void)
{
    return (ProductTcKTable_GetMeasurementTable() != NULL) &&
           (ProductTcKTable_GetCjcTable() != NULL);
}
