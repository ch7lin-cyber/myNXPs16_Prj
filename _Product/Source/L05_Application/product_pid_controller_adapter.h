#ifndef PRODUCT_PID_CONTROLLER_ADAPTER_H
#define PRODUCT_PID_CONTROLLER_ADAPTER_H

#include <stdbool.h>
#include <stdint.h>

#include "cklib/inc/ck_auto_tune.h"
#include "cklib/inc/ck_controller.h"
#include "cklib/inc/dt_auto_tune.h"
#include "cklib/inc/dt_on_off_controller.h"
#include "cklib/inc/dt_pid_controller.h"
#include "cklib/inc/dt_pid_fb_controller.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    PRODUCT_PID_CONTROLLER_MODE_AUTO_TUNE = 0,
    PRODUCT_PID_CONTROLLER_MODE_ON_OFF_CONTROL,
    PRODUCT_PID_CONTROLLER_MODE_PID_CONTROL,
    PRODUCT_PID_CONTROLLER_MODE_PID_FB_CONTROL
} ProductPidControllerMode_t;

typedef struct
{
    uint32_t sample_sequence;
    /* Temperature only; convert L03 milli-degrees C to 0.1 degree C in L05. */
    control_value_t pv;
    control_value_t sv;
    /* Function-block input pins; RESET is detected on its rising edge. */
    bool enable;
    /* Rising-edge command; it resets state and suppresses this scan's output. */
    bool reset;
    bool valid;
} ProductPidControllerInput_t;

typedef bool (*ProductPidControllerSampleProvider_t)(
    void *context,
    ProductPidControllerInput_t *input);

typedef struct
{
    ProductPidControllerMode_t mode;
    /* AUTO_TUNE applies the calculated parameters to this PID variant. */
    ProductPidControllerMode_t auto_tune_target_mode;
    CK_CONTROL_ACTION control_action;
    /* 0 disables age checking; otherwise stale samples are rejected. */
    uint32_t max_sample_age_ms;
    /* Required for AUTO_TUNE because CKLIB's auto-tune API has no dt input. */
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
    PRODUCT_PID_CONTROLLER_ADAPTER_CKLIB_ERROR
} ProductPidControllerAdapterStatus_t;

typedef struct
{
    CK_CONTROLLER controller;
    CK_AUTO_TUNE auto_tune;
    ProductPidControllerAdapterConfig_t config;
    ProductPidControllerSampleProvider_t sample_provider;
    void *sample_provider_context;
    CK_OUTPUT output;
    CK_ERROR last_ck_error;
    ProductPidControllerAdapterStatus_t last_status;
    uint32_t callback_count;
    uint32_t last_elapsed_ms;
    uint32_t sample_age_ms;
    uint32_t last_sample_sequence;
    bool initialized;
    bool active;
    bool state_reset;
    bool reset_input_previous;
    bool has_sample_sequence;
    bool output_valid;
    bool auto_tune_completed;
} ProductPidControllerAdapter_t;

typedef struct
{
    CK_OUTPUT output;
    CK_ERROR last_ck_error;
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

void ProductPidControllerAdapter_Deinit(
    ProductPidControllerAdapter_t *adapter);

bool ProductPidControllerAdapter_Reset(
    ProductPidControllerAdapter_t *adapter);

/* Suitable for ProductFbScheduleTask_t.callback. */
void ProductPidControllerAdapter_Process(void *context,
                                         uint32_t elapsed_ms);

bool ProductPidControllerAdapter_GetOutput(
    const ProductPidControllerAdapter_t *adapter,
    CK_OUTPUT *output,
    bool *valid);

bool ProductPidControllerAdapter_GetDiagnostics(
    const ProductPidControllerAdapter_t *adapter,
    ProductPidControllerAdapterDiagnostics_t *diagnostics);

/* Borrowed handles for setting CKLIB-specific parameters. Do not free them. */
CK_CONTROLLER ProductPidControllerAdapter_GetControllerHandle(
    ProductPidControllerAdapter_t *adapter);
CK_AUTO_TUNE ProductPidControllerAdapter_GetAutoTuneHandle(
    ProductPidControllerAdapter_t *adapter);

#ifdef __cplusplus
}
#endif

#endif /* PRODUCT_PID_CONTROLLER_ADAPTER_H */
