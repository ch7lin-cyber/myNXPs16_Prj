#ifndef PRODUCT_TC_R_TABLE_H
#define PRODUCT_TC_R_TABLE_H

#include "PiecewiseLinearTable.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PRODUCT_TC_R_TABLE_COMPLETE                (1U)
#define PRODUCT_TC_R_MIN_MILLICELSIUS              (0L)
#define PRODUCT_TC_R_MAX_MILLICELSIUS              (1700000L)
#define PRODUCT_TC_R_EXTENDED_MARGIN_MC            (20000L)

/* TODO: Enter this sensor's measurement-table parameters. */
#define PRODUCT_TC_R_MEASUREMENT_SEGMENT_COUNT     (87U)
#define PRODUCT_TC_R_MEASUREMENT_BOUNDARY_COUNT    \
    (PRODUCT_TC_R_MEASUREMENT_SEGMENT_COUNT + 1U)
#define PRODUCT_TC_R_INPUT_SHIFT_UV                (110L)
#define PRODUCT_TC_R_MEASUREMENT_COEFFICIENT_SCALE (100UL)

/* CJC covers -20.0 to 109.0 degree C in 10 degree C groups. */
#define PRODUCT_TC_R_CJC_MIN_MILLICELSIUS          (-20000L)
#define PRODUCT_TC_R_CJC_MAX_MILLICELSIUS          (109000L)
#define PRODUCT_TC_R_CJC_INTERVAL_MC               (10000L)
#define PRODUCT_TC_R_CJC_SEGMENT_COUNT             (13U)
#define PRODUCT_TC_R_CJC_INPUT_SHIFT_DECICELSIUS   (200L)
#define PRODUCT_TC_R_CJC_COEFFICIENT_SCALE         (1000UL)

const PiecewiseLinearTable_t *ProductTcRTable_GetMeasurementTable(void);
const PiecewiseLinearTable_t *ProductTcRTable_GetCjcTable(void);
bool ProductTcRTable_IsReady(void);

#ifdef __cplusplus
}
#endif

#endif /* PRODUCT_TC_R_TABLE_H */

