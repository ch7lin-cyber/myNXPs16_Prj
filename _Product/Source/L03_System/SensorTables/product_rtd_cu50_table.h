#ifndef PRODUCT_RTD_CU50_TABLE_H
#define PRODUCT_RTD_CU50_TABLE_H

#include "PiecewiseLinearTable.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PRODUCT_RTD_CU50_TABLE_COMPLETE                (1U)
#define PRODUCT_RTD_CU50_MIN_MILLICELSIUS              (-50000L)
#define PRODUCT_RTD_CU50_MAX_MILLICELSIUS              (150000L)
#define PRODUCT_RTD_CU50_EXTENDED_MARGIN_MC            (20000L)

/* TODO: Enter this sensor's resistance-to-temperature table parameters. */
#define PRODUCT_RTD_CU50_MEASUREMENT_SEGMENT_COUNT     (0U)
#define PRODUCT_RTD_CU50_MEASUREMENT_BOUNDARY_COUNT    \
    (PRODUCT_RTD_CU50_MEASUREMENT_SEGMENT_COUNT + 1U)
#define PRODUCT_RTD_CU50_MEASUREMENT_COEFFICIENT_SCALE (100UL)

const PiecewiseLinearTable_t *ProductRtdCu50Table_GetMeasurementTable(void);
bool ProductRtdCu50Table_IsReady(void);

#ifdef __cplusplus
}
#endif

#endif /* PRODUCT_RTD_CU50_TABLE_H */

