#ifndef PRODUCT_RTD_NI120_TABLE_H
#define PRODUCT_RTD_NI120_TABLE_H

#include "PiecewiseLinearTable.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PRODUCT_RTD_NI120_TABLE_COMPLETE                (0U)
#define PRODUCT_RTD_NI120_MIN_MILLICELSIUS              (-80000L)
#define PRODUCT_RTD_NI120_MAX_MILLICELSIUS              (300000L)
#define PRODUCT_RTD_NI120_EXTENDED_MARGIN_MC            (20000L)

/* TODO: Enter this sensor's resistance-to-temperature table parameters. */
#define PRODUCT_RTD_NI120_MEASUREMENT_SEGMENT_COUNT     (0U)
#define PRODUCT_RTD_NI120_MEASUREMENT_BOUNDARY_COUNT    \
    (PRODUCT_RTD_NI120_MEASUREMENT_SEGMENT_COUNT + 1U)
#define PRODUCT_RTD_NI120_MEASUREMENT_COEFFICIENT_SCALE (100UL)

const PiecewiseLinearTable_t *ProductRtdNi120Table_GetMeasurementTable(void);
bool ProductRtdNi120Table_IsReady(void);

#ifdef __cplusplus
}
#endif

#endif /* PRODUCT_RTD_NI120_TABLE_H */

