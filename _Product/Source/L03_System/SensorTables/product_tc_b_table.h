#ifndef PRODUCT_TC_B_TABLE_H
#define PRODUCT_TC_B_TABLE_H

#include "PiecewiseLinearTable.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PRODUCT_TC_B_TABLE_COMPLETE                (0U)
#define PRODUCT_TC_B_MIN_MILLICELSIUS              (100000L)
#define PRODUCT_TC_B_MAX_MILLICELSIUS              (1800000L)
#define PRODUCT_TC_B_EXTENDED_MARGIN_MC            (20000L)

/* TODO: Enter this sensor's measurement-table parameters. */
#define PRODUCT_TC_B_MEASUREMENT_SEGMENT_COUNT     (92U)
#define PRODUCT_TC_B_MEASUREMENT_BOUNDARY_COUNT    \
    (PRODUCT_TC_B_MEASUREMENT_SEGMENT_COUNT + 1U)
#define PRODUCT_TC_B_INPUT_SHIFT_UV                (100L)
#define PRODUCT_TC_B_MEASUREMENT_COEFFICIENT_SCALE (100UL)

/* CJC covers -20.0 to 109.0 degree C in 10 degree C groups. */
#define PRODUCT_TC_B_CJC_MIN_MILLICELSIUS          (-20000L)
#define PRODUCT_TC_B_CJC_MAX_MILLICELSIUS          (109000L)
#define PRODUCT_TC_B_CJC_INTERVAL_MC               (10000L)
#define PRODUCT_TC_B_CJC_SEGMENT_COUNT             (13U)
#define PRODUCT_TC_B_CJC_INPUT_SHIFT_DECICELSIUS   (200L)
#define PRODUCT_TC_B_CJC_COEFFICIENT_SCALE         (1000UL)

const PiecewiseLinearTable_t *ProductTcBTable_GetMeasurementTable(void);
const PiecewiseLinearTable_t *ProductTcBTable_GetCjcTable(void);
bool ProductTcBTable_IsReady(void);

#ifdef __cplusplus
}
#endif

#endif /* PRODUCT_TC_B_TABLE_H */

