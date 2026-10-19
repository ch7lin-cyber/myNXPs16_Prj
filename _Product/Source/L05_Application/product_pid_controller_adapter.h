#ifndef PRODUCT_PID_CONTROLLER_ADAPTER_H
#define PRODUCT_PID_CONTROLLER_ADAPTER_H

#include <stdbool.h>
#include <stdint.h>

#include "FbPid.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef FbPidMode_t ProductPidControllerMode_t;
#define PRODUCT_PID_CONTROLLER_MODE_AUTO_TUNE       FB_PID_MODE_AUTO_TUNE
#define PRODUCT_PID_CONTROLLER_MODE_ON_OFF_CONTROL   FB_PID_MODE_ON_OFF
#define PRODUCT_PID_CONTROLLER_MODE_PID_CONTROL     FB_PID_MODE_PID
#define PRODUCT_PID_CONTROLLER_MODE_PID_FB_CONTROL  FB_PID_MODE_PID_FB

typedef struct
{
    uint32_t sample_sequence;
    int32_t pv; /* normalized to CKLIB unit: 0.1 degree C by the provider */
    int32_t sv;
    bool enable;
    bool reset;
    bool valid;
} ProductPidControllerInput_t;

typedef bool (*ProductPidControllerSampleProvider_t)(
    void *context, ProductPidControllerInput_t *input);

typedef struct
{
    FbPidMode_t mode;
    FbPidMode_t auto_tune_target_mode;
    FbPidControlAction_t control_action;
    uint32_t max_sample_age_ms; /* zero disables age rejection */
    uint32_t auto_tune_period_ms;
    uint32_t auto_tune_period_tolerance_ms;
} ProductPidControllerAdapterConfig_t;

typedef enum
{
    PRODUCT_PID_CONTROLLER_ADAPTER_OK = 0,
    PRODUCT_PID_CONTROLLER_ADAPTER_DISABLED,
    PRODUCT_PID_CONTROLLER_ADAPTER_INVALID_ARGUMENT,
    PRODUCT_PID_CONTROLLER_ADAPTER_NOT_INITIALIZED,
    PRODUCT_PID_CONTROLLER_ADAPTER_INPUT_UNAVAILABLE,
    PRODUCT_PID_CONTROLLER_ADAPTER_INPUT_INVALID,
    PRODUCT_PID_CONTROLLER_ADAPTER_INPUT_STALE,
    PRODUCT_PID_CONTROLLER_ADAPTER_INVALID_TIMING,
    PRODUCT_PID_CONTROLLER_ADAPTER_FB_ERROR
} ProductPidControllerAdapterStatus_t;

typedef struct
{
    FbPid_t fb_pid;
    ProductPidControllerAdapterConfig_t config;
    ProductPidControllerSampleProvider_t sample_provider;
    void *sample_provider_context;
    FbPidOutput_t output;
    ProductPidControllerAdapterStatus_t last_status;
    uint32_t callback_count;
    uint32_t last_elapsed_ms;
    uint32_t sample_age_ms;
    uint32_t last_sample_sequence;
    bool initialized;
    bool has_sample_sequence;
} ProductPidControllerAdapter_t;

typedef struct
{
    FbPidOutput_t output;
    ProductPidControllerAdapterStatus_t last_status;
    uint32_t callback_count;
    uint32_t last_elapsed_ms;
    uint32_t sample_age_ms;
    bool active;
    bool output_valid;
    bool auto_tune_completed;
} ProductPidControllerAdapterDiagnostics_t;

bool ProductPidControllerAdapter_Init(
    ProductPidControllerAdapter_t *adapter,
    const ProductPidControllerAdapterConfig_t *config,
    ProductPidControllerSampleProvider_t sample_provider,
    void *sample_provider_context);
void ProductPidControllerAdapter_Deinit(ProductPidControllerAdapter_t *adapter);
bool ProductPidControllerAdapter_Reset(ProductPidControllerAdapter_t *adapter);
FbPidStatus_t ProductPidControllerAdapter_SetParameter(
    ProductPidControllerAdapter_t *adapter,
    FbPidParameter_t parameter,
    int32_t value);
void ProductPidControllerAdapter_Process(void *context, uint32_t elapsed_ms);
bool ProductPidControllerAdapter_GetOutput(
    const ProductPidControllerAdapter_t *adapter,
    FbPidOutput_t *output,
    bool *valid);
bool ProductPidControllerAdapter_GetDiagnostics(
    const ProductPidControllerAdapter_t *adapter,
    ProductPidControllerAdapterDiagnostics_t *diagnostics);

#ifdef __cplusplus
}
#endif

#endif /* PRODUCT_PID_CONTROLLER_ADAPTER_H */
