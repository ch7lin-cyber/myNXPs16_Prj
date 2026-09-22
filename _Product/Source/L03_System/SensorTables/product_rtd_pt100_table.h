#ifndef PRODUCT_RTD_PT100_TABLE_H
#define PRODUCT_RTD_PT100_TABLE_H

#include "PiecewiseLinearTable.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PRODUCT_RTD_PT100_TABLE_COMPLETE                (0U)
#define PRODUCT_RTD_PT100_MIN_MILLICELSIUS              (-200000L)
#define PRODUCT_RTD_PT100_MAX_MILLICELSIUS              (850000L)
#define PRODUCT_RTD_PT100_EXTENDED_MARGIN_MC            (20000L)

/* TODO: Enter this sensor's resistance-to-temperature table parameters. */
#define PRODUCT_RTD_PT100_MEASUREMENT_SEGMENT_COUNT     (0U)
#define PRODUCT_RTD_PT100_MEASUREMENT_BOUNDARY_COUNT    \
    (PRODUCT_RTD_PT100_MEASUREMENT_SEGMENT_COUNT + 1U)
#define PRODUCT_RTD_PT100_MEASUREMENT_COEFFICIENT_SCALE (100UL)

const PiecewiseLinearTable_t *ProductRtdPt100Table_GetMeasurementTable(void);
bool ProductRtdPt100Table_IsReady(void);

#ifdef __cplusplus
}
#endif

#endif /* PRODUCT_RTD_PT100_TABLE_H */

