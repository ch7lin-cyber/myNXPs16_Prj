#ifndef PRODUCT_PV_PROCESSING_ADAPTER_H
#define PRODUCT_PV_PROCESSING_ADAPTER_H
#include <stdbool.h>
#include <stdint.h>
#include "FbSignalLinearization.h"
#include "FbInputSwitchOver.h"
#ifdef __cplusplus
extern "C" {
#endif
#define PRODUCT_PV_CURVE_MAX_POINTS 64u
typedef enum { PRODUCT_PV_MILLICELSIUS=0, PRODUCT_PV_UNSUPPORTED_UNIT } ProductPvUnit;
typedef struct {
    int32_t value;
    uint32_t timestamp_ms, sequence;
    ProductPvUnit unit;
    bool valid;
} ProductPvSourceSample;
typedef bool (*ProductPvSourceProvider)(void *context, uint16_t source,
                                      ProductPvSourceSample *sample);
typedef struct {
    uint16_t source[2]; /* abstract IDs resolved only by provider */
    FbInputLinearizationConfig linearization[2]; /* all numeric units milli-C */
    bool switch_enabled; /* false uses source[0] only */
    FbInputSwitchOverConfig switch_config;
    uint32_t maximum_age_ms; /* 1..INT32_MAX; modular timestamp interval */
    uint32_t maximum_skew_ms; /* <=maximum_age, only when both healthy */
} ProductPvProcessingConfig;
typedef enum { PRODUCT_PV_SOURCE_OK=0, PRODUCT_PV_SOURCE_UNAVAILABLE,
    PRODUCT_PV_SOURCE_INVALID, PRODUCT_PV_SOURCE_UNIT, PRODUCT_PV_SOURCE_STALE,
    PRODUCT_PV_SOURCE_UNALIGNED, PRODUCT_PV_SOURCE_PRECISION,
    PRODUCT_PV_SOURCE_LINEARIZATION, PRODUCT_PV_SOURCE_RANGE } ProductPvSourceStatus;
typedef struct {
    ProductPvSourceSample raw;
    int32_t processed;
    uint32_t age_ms;
    ProductPvSourceStatus status;
    bool valid;
    FbLinearizationStatus linearization_status;
} ProductPvSourceDiagnostics;
typedef enum { PRODUCT_PV_OK=0, PRODUCT_PV_DISABLED, PRODUCT_PV_RESET,
    PRODUCT_PV_NOT_INITIALIZED, PRODUCT_PV_INVALID_ARGUMENT,
    PRODUCT_PV_INVALID_INPUT } ProductPvProcessingStatus;
typedef struct {
    int32_t pv;
    bool valid, degraded;
    FbSwitchSource selected;
    FbSwitchStatus switch_status;
    ProductPvSourceDiagnostics sources[2];
    uint32_t timestamp_ms; /* publication time, not source sample time */
    ProductPvProcessingStatus status;
} ProductPvProcessingOutput;
typedef struct {
    ProductPvProcessingConfig config;
    ProductPvSourceProvider provider;
    void *provider_context;
    FbInputSwitchOver switch_fb;
    ProductPvProcessingOutput output;
    bool initialized;
} ProductPvProcessingAdapter;
/* Main-loop owned, single processing chain. Borrowed curves stay immutable
 * until SetConfig succeeds; caller owns table lifetime. No Sensor Snapshot
 * writes, PID/CKLIB, NVM or hardware. No aliasing among arguments. */
bool ProductPvProcessing_ValidateConfig(const ProductPvProcessingConfig *config);
bool ProductPvProcessing_Init(ProductPvProcessingAdapter *adapter,
    const ProductPvProcessingConfig *config, ProductPvSourceProvider provider, void *context);
bool ProductPvProcessing_SetConfig(ProductPvProcessingAdapter *adapter,
    const ProductPvProcessingConfig *config); /* all-or-reject; resets history */
ProductPvProcessingStatus ProductPvProcessing_Process(ProductPvProcessingAdapter *adapter,
    uint32_t now_ms, bool enable, bool reset);
bool ProductPvProcessing_GetOutput(const ProductPvProcessingAdapter *adapter,
    ProductPvProcessingOutput *output);
#ifdef __cplusplus
}
#endif
#endif
