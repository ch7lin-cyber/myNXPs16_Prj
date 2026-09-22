#ifndef PRODUCT_TC_K_TABLE_H
#define PRODUCT_TC_K_TABLE_H

#include "PiecewiseLinearTable.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PRODUCT_TC_K_MIN_MILLICELSIUS          (-200000L)
#define PRODUCT_TC_K_MAX_MILLICELSIUS          (1300000L)
#define PRODUCT_TC_K_EXTENDED_MARGIN_MC        (20000L)
#define PRODUCT_TC_K_MEASUREMENT_INTERVAL_MC   (20000L)
#define PRODUCT_TC_K_CJC_INTERVAL_MC           (10000L)
#define PRODUCT_TC_K_COEFFICIENT_SCALE         (1000000UL)

/* x = calibrated thermocouple uV, y = temperature in 0.001 degree C. */
const PiecewiseLinearTable_t *ProductTcKTable_GetMeasurementTable(void);

/* x = cold-junction temperature in 0.001 degree C, y = CJC uV. */
const PiecewiseLinearTable_t *ProductTcKTable_GetCjcTable(void);

/* False until both coefficient matrices have been populated and reviewed. */
bool ProductTcKTable_IsReady(void);

#ifdef __cplusplus
}
#endif

#endif /* PRODUCT_TC_K_TABLE_H */
