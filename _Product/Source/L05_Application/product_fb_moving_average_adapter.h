#ifndef PRODUCT_FB_MOVING_AVERAGE_ADAPTER_H
#define PRODUCT_FB_MOVING_AVERAGE_ADAPTER_H

#include <stdbool.h>
#include <stdint.h>

#include "FbMovingAverage.h"
#include "product_fb_schedule.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PRODUCT_FB_MOVING_AVERAGE_WINDOW_SIZE (5U)

typedef struct
{
    uint32_t sample_sequence;
    int32_t engineering_value;
    bool valid;
} ProductFbMovingAverageSample_t;

typedef bool (*ProductFbMovingAverageSampleProvider_t)(
    void *context,
    ProductFbMovingAverageSample_t *sample);

typedef struct
{
    FbMovingAverageState fb_state;
    FbMovingAverageConfig fb_config;
    FbMovingAverageOutput output;
    double buffer[PRODUCT_FB_MOVING_AVERAGE_WINDOW_SIZE];
    uint32_t last_sample_sequence;
    uint32_t callback_count;
    uint32_t last_elapsed_ms;
    FbMovingAverageStatus last_status;
    bool has_last_sample_sequence;
    bool reset_pending;
    bool input_invalid;
    ProductFbMovingAverageSampleProvider_t sample_provider;
    void *sample_provider_context;
} ProductFbMovingAverageAdapter_t;

typedef struct
{
    uint32_t callback_count;
    uint32_t last_elapsed_ms;
    FbMovingAverageStatus last_status;
} ProductFbMovingAverageAdapterDiagnostics_t;

bool ProductFbMovingAverageAdapter_Init(
    ProductFbMovingAverageAdapter_t *adapter,
    ProductFbMovingAverageSampleProvider_t sample_provider,
    void *sample_provider_context);

/* Suitable for ProductFbScheduleTask_t.callback. The first version is CH0 PV. */
void ProductFbMovingAverageAdapter_Process(void *context,
                                           uint32_t elapsed_ms);

bool ProductFbMovingAverageAdapter_GetOutput(
    const ProductFbMovingAverageAdapter_t *adapter,
    FbMovingAverageOutput *output);
bool ProductFbMovingAverageAdapter_GetDiagnostics(
    const ProductFbMovingAverageAdapter_t *adapter,
    ProductFbMovingAverageAdapterDiagnostics_t *diagnostics);

#ifdef __cplusplus
}
#endif

#endif /* PRODUCT_FB_MOVING_AVERAGE_ADAPTER_H */
