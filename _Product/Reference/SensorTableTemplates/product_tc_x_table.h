#ifndef PRODUCT_TC_X_TABLE_H
#define PRODUCT_TC_X_TABLE_H

#include "PiecewiseLinearTable.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * TODO: USER TABLE DATA
 *
 * 1. Copy this pair into:
 *      _Product/Source/L03_System/SensorTables/
 * 2. Replace every TcX/TC_X/tc_x with the sensor name, for example:
 *      TcJ/TC_J/tc_j
 * 3. Enter the sensor range and table dimensions below.
 */
#define PRODUCT_TC_X_TABLE_COMPLETE                (0U)

/* TODO: USER TABLE DATA - rated sensor range, unit = 0.001 degree C. */
#define PRODUCT_TC_X_MIN_MILLICELSIUS              (0L)
#define PRODUCT_TC_X_MAX_MILLICELSIUS              (0L)
#define PRODUCT_TC_X_EXTENDED_MARGIN_MC            (20000L)

/* TODO: USER TABLE DATA - measurement uV -> temperature table. */
#define PRODUCT_TC_X_MEASUREMENT_SEGMENT_COUNT     (0U)
#define PRODUCT_TC_X_MEASUREMENT_BOUNDARY_COUNT    \
    (PRODUCT_TC_X_MEASUREMENT_SEGMENT_COUNT + 1U)
#define PRODUCT_TC_X_INPUT_SHIFT_UV                (0L)
#define PRODUCT_TC_X_MEASUREMENT_COEFFICIENT_SCALE (100UL)

/* TODO: USER TABLE DATA - CJC temperature -> uV table. */
#define PRODUCT_TC_X_CJC_MIN_MILLICELSIUS          (-20000L)
#define PRODUCT_TC_X_CJC_MAX_MILLICELSIUS          (109000L)
#define PRODUCT_TC_X_CJC_INTERVAL_MC               (10000L)
#define PRODUCT_TC_X_CJC_SEGMENT_COUNT             (0U)
#define PRODUCT_TC_X_CJC_INPUT_SHIFT_DECICELSIUS   (200L)
#define PRODUCT_TC_X_CJC_COEFFICIENT_SCALE         (1000UL)

const PiecewiseLinearTable_t *ProductTcXTable_GetMeasurementTable(void);
const PiecewiseLinearTable_t *ProductTcXTable_GetCjcTable(void);
bool ProductTcXTable_IsReady(void);

#ifdef __cplusplus
}
#endif

#endif /* PRODUCT_TC_X_TABLE_H */
