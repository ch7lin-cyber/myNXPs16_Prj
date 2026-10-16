#ifndef PRODUCT_FB_SCHEDULE_INTEGRATION_H
#define PRODUCT_FB_SCHEDULE_INTEGRATION_H

#include <stdbool.h>
#include <stdint.h>

#include "FbMovingAverage.h"
#include "product_fb_moving_average_adapter.h"

#ifdef __cplusplus
extern "C" {
#endif

bool ProductFbScheduleIntegration_Init(uint32_t now_ms);
void ProductFbScheduleIntegration_Process(uint32_t now_ms);
bool ProductFbScheduleIntegration_GetCh0MovingAverage(
    FbMovingAverageOutput *output);
bool ProductFbScheduleIntegration_GetCh0Diagnostics(
    ProductFbMovingAverageAdapterDiagnostics_t *diagnostics);

#ifdef __cplusplus
}
#endif

#endif /* PRODUCT_FB_SCHEDULE_INTEGRATION_H */
