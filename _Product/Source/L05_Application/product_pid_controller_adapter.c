#include "product_pid_controller_adapter.h"

#include <limits.h>
#include <stddef.h>
#include <string.h>

static bool IsPidMode(ProductPidControllerMode_t mode)
{
    return (mode == PRODUCT_PID_CONTROLLER_MODE_PID_CONTROL) ||
           (mode == PRODUCT_PID_CONTROLLER_MODE_PID_FB_CONTROL);
}

static CK_CONTROLLER CreateController(ProductPidControllerMode_t mode)
{
    switch (mode)
    {
        case PRODUCT_PID_CONTROLLER_MODE_ON_OFF_CONTROL:
            return ckCreateDtOnOffController();
        case PRODUCT_PID_CONTROLLER_MODE_PID_CONTROL:
            return ckCreateDtPidController();
        case PRODUCT_PID_CONTROLLER_MODE_PID_FB_CONTROL:
            return ckCreateDtPidFbController();
        default:
            return NULL;
    }
}

static CK_ERROR SetControllerAction(CK_CONTROLLER controller,
                                    ProductPidControllerMode_t mode,
                                    CK_CONTROL_ACTION action)
{
    CK_CONTROLLER_PARAM parameter;

    switch (mode)
    {
        case PRODUCT_PID_CONTROLLER_MODE_ON_OFF_CONTROL:
            parameter = DT_ON_OFF_PARAM_CONTROL_ACTION;
            break;
        case PRODUCT_PID_CONTROLLER_MODE_PID_CONTROL:
        case PRODUCT_PID_CONTROLLER_MODE_PID_FB_CONTROL:
            parameter = DT_PID_PARAM_CONTROL_ACTION;
            break;
        default:
            return CK_ERROR_INVALID_PARAMETER;
    }
    return ckControllerSetEnumParam(controller, parameter, (int32_t)action);
}

static void ClearOutput(ProductPidControllerAdapter_t *adapter)
{
    adapter->output.mv[0] = 0;
    adapter->output.mv[1] = 0;
    adapter->output.mv_count = 0U;
    adapter->output_valid = false;
}

static void Fail(ProductPidControllerAdapter_t *adapter,
                 ProductPidControllerAdapterStatus_t status,
                 CK_ERROR ck_error)
{
    ClearOutput(adapter);
    adapter->last_status = status;
    adapter->last_ck_error = ck_error;
}

static bool ResetRuntimeState(ProductPidControllerAdapter_t *adapter)
{
    CK_ERROR error = ckControllerReset(adapter->controller);

    if (CK_SUCCESS(error) && (adapter->auto_tune != NULL))
    {
        error = ckAutoTuneReset(adapter->auto_tune);
    }
    if (!CK_SUCCESS(error))
    {
        adapter->active = false;
        Fail(adapter, PRODUCT_PID_CONTROLLER_ADAPTER_CKLIB_ERROR, error);
        return false;
    }
    adapter->auto_tune_completed = false;
    adapter->state_reset = true;
    ClearOutput(adapter);
    return true;
}

static void AddSampleAge(ProductPidControllerAdapter_t *adapter,
                         uint32_t elapsed_ms)
{
    if (UINT32_MAX - adapter->sample_age_ms < elapsed_ms)
    {
        adapter->sample_age_ms = UINT32_MAX;
    }
    else
    {
        adapter->sample_age_ms += elapsed_ms;
    }
}

static void UpdateSampleAge(ProductPidControllerAdapter_t *adapter,
                            const ProductPidControllerInput_t *sample,
                            uint32_t elapsed_ms)
{
    if (!adapter->has_sample_sequence ||
        (sample->sample_sequence != adapter->last_sample_sequence))
    {
        adapter->has_sample_sequence = true;
        adapter->last_sample_sequence = sample->sample_sequence;
        adapter->sample_age_ms = 0U;
    }
    else
    {
        AddSampleAge(adapter, elapsed_ms);
    }
}

static void Deactivate(ProductPidControllerAdapter_t *adapter,
                       ProductPidControllerAdapterStatus_t reason)
{
    if (adapter->active && !ResetRuntimeState(adapter))
    {
        return;
    }
    adapter->active = false;
    adapter->state_reset = true;
    Fail(adapter, reason, CK_NO_ERR);
}

static bool IsValidConfig(const ProductPidControllerAdapterConfig_t *config,
                          ProductPidControllerSampleProvider_t provider)
{
    if ((config == NULL) || (provider == NULL) ||
        (config->control_action > CK_CONTROL_ACTION_HEAT_COOL))
    {
        return false;
    }
    switch (config->mode)
    {
        case PRODUCT_PID_CONTROLLER_MODE_AUTO_TUNE:
            return IsPidMode(config->auto_tune_target_mode) &&
                   (config->auto_tune_period_ms >= 10U);
        case PRODUCT_PID_CONTROLLER_MODE_ON_OFF_CONTROL:
        case PRODUCT_PID_CONTROLLER_MODE_PID_CONTROL:
        case PRODUCT_PID_CONTROLLER_MODE_PID_FB_CONTROL:
            return true;
        default:
            return false;
    }
}

bool ProductPidControllerAdapter_Init(
    ProductPidControllerAdapter_t *adapter,
    const ProductPidControllerAdapterConfig_t *config,
    ProductPidControllerSampleProvider_t sample_provider,
    void *sample_provider_context)
{
    CK_ERROR error;

    if ((adapter == NULL) || !IsValidConfig(config, sample_provider))
    {
        return false;
    }
    (void)memset(adapter, 0, sizeof(*adapter));
    adapter->config = *config;
    adapter->sample_provider = sample_provider;
    adapter->sample_provider_context = sample_provider_context;
    adapter->last_status = PRODUCT_PID_CONTROLLER_ADAPTER_NOT_INITIALIZED;
    adapter->last_ck_error = CK_NO_ERR;

    if (config->mode == PRODUCT_PID_CONTROLLER_MODE_AUTO_TUNE)
    {
        adapter->auto_tune = ckCreateDtAutoTune();
        adapter->controller = CreateController(config->auto_tune_target_mode);
        if ((adapter->auto_tune == NULL) || (adapter->controller == NULL))
        {
            ProductPidControllerAdapter_Deinit(adapter);
            return false;
        }
        error = ckAutoTuneSetEnumParam(
            adapter->auto_tune, DT_AUTO_TUNE_PARAM_CONTROL_ACTION,
            (int32_t)config->control_action);
        if (CK_SUCCESS(error))
        {
            error = SetControllerAction(adapter->controller,
                                        config->auto_tune_target_mode,
                                        config->control_action);
        }
    }
    else
    {
        adapter->controller = CreateController(config->mode);
        if (adapter->controller == NULL)
        {
            ProductPidControllerAdapter_Deinit(adapter);
            return false;
        }
        error = SetControllerAction(adapter->controller, config->mode,
                                    config->control_action);
    }

    if (!CK_SUCCESS(error))
    {
        adapter->last_ck_error = error;
        ProductPidControllerAdapter_Deinit(adapter);
        return false;
    }
    adapter->initialized = true;
    adapter->state_reset = true;
    adapter->last_status = PRODUCT_PID_CONTROLLER_ADAPTER_OK;
    return true;
}

bool ProductPidControllerAdapter_Reset(
    ProductPidControllerAdapter_t *adapter)
{
    if ((adapter == NULL) || !adapter->initialized)
    {
        return false;
    }
    if (!ResetRuntimeState(adapter))
    {
        return false;
    }
    adapter->active = false;
    adapter->reset_input_previous = false;
    adapter->last_ck_error = CK_NO_ERR;
    adapter->last_status = PRODUCT_PID_CONTROLLER_ADAPTER_OK;
    return true;
}

void ProductPidControllerAdapter_Deinit(
    ProductPidControllerAdapter_t *adapter)
{
    if (adapter == NULL)
    {
        return;
    }
    if (adapter->auto_tune != NULL)
    {
        (void)ckAutoTuneFree(adapter->auto_tune);
    }
    if (adapter->controller != NULL)
    {
        (void)ckControllerFree(adapter->controller);
    }
    adapter->auto_tune = NULL;
    adapter->controller = NULL;
    adapter->initialized = false;
    adapter->active = false;
    adapter->state_reset = true;
    adapter->output_valid = false;
    adapter->output.mv_count = 0U;
    adapter->last_status = PRODUCT_PID_CONTROLLER_ADAPTER_NOT_INITIALIZED;
}

static CK_ERROR RunController(ProductPidControllerAdapter_t *adapter,
                              const ProductPidControllerInput_t *sample,
                              uint32_t elapsed_ms)
{
    CK_ERROR error;
    uint32_t step_time_10ms;

    if (elapsed_ms < 5U)
    {
        return CK_ERROR_INVALID_PARAMETER;
    }
    step_time_10ms = (elapsed_ms / 10U) +
                     (((elapsed_ms % 10U) >= 5U) ? 1U : 0U);
    if ((step_time_10ms == 0U) || (step_time_10ms > (uint32_t)INT32_MAX))
    {
        return CK_ERROR_INVALID_PARAMETER;
    }
    error = ckControllerSetPV(adapter->controller, sample->pv);
    if (CK_SUCCESS(error))
    {
        error = ckControllerSetSV(adapter->controller, sample->sv);
    }
    if (CK_SUCCESS(error))
    {
        error = ckControllerSetStepTime(adapter->controller,
                                        (int32_t)step_time_10ms);
    }
    if (CK_SUCCESS(error))
    {
        error = ckControllerControl(adapter->controller, &adapter->output);
    }
    return error;
}

static bool AutoTunePeriodValid(const ProductPidControllerAdapter_t *adapter,
                                uint32_t elapsed_ms)
{
    uint32_t expected = adapter->config.auto_tune_period_ms;
    uint32_t tolerance = adapter->config.auto_tune_period_tolerance_ms;
    uint32_t difference = (elapsed_ms >= expected) ?
                          (elapsed_ms - expected) :
                          (expected - elapsed_ms);
    return difference <= tolerance;
}

static CK_ERROR RunAutoTune(ProductPidControllerAdapter_t *adapter,
                            const ProductPidControllerInput_t *sample,
                            uint32_t elapsed_ms)
{
    CK_ERROR error;

    if (!AutoTunePeriodValid(adapter, elapsed_ms))
    {
        return CK_ERROR_INVALID_PARAMETER;
    }
    error = ckAutoTuneSetSV(adapter->auto_tune, sample->sv);
    if (CK_SUCCESS(error))
    {
        error = ckAutoTuneCollectAndSignal(
            adapter->auto_tune, sample->pv, &adapter->output);
    }
    if (CK_SUCCESS(error) && (adapter->output.mv_count == 0U))
    {
        error = ckAutoTuneApplyParams(adapter->auto_tune,
                                      adapter->controller);
        if (CK_SUCCESS(error))
        {
            adapter->auto_tune_completed = true;
            error = RunController(adapter, sample, elapsed_ms);
        }
    }
    return error;
}

void ProductPidControllerAdapter_Process(void *context,
                                         uint32_t elapsed_ms)
{
    ProductPidControllerAdapter_t *adapter =
        (ProductPidControllerAdapter_t *)context;
    ProductPidControllerInput_t sample;
    CK_ERROR error;

    if (adapter == NULL)
    {
        return;
    }
    adapter->callback_count++;
    adapter->last_elapsed_ms = elapsed_ms;
    if (!adapter->initialized)
    {
        Fail(adapter, PRODUCT_PID_CONTROLLER_ADAPTER_NOT_INITIALIZED,
             CK_ERROR_INVALID_PARAMETER);
        return;
    }
    if (!adapter->sample_provider(adapter->sample_provider_context, &sample))
    {
        AddSampleAge(adapter, elapsed_ms);
        Deactivate(adapter,
                   PRODUCT_PID_CONTROLLER_ADAPTER_INPUT_UNAVAILABLE);
        return;
    }
    UpdateSampleAge(adapter, &sample, elapsed_ms);

    if (sample.reset && !adapter->reset_input_previous)
    {
        adapter->reset_input_previous = true;
        if (!ResetRuntimeState(adapter))
        {
            return;
        }
        adapter->active = sample.enable;
        adapter->state_reset = !sample.enable;
        adapter->last_status = sample.enable ?
            PRODUCT_PID_CONTROLLER_ADAPTER_OK :
            PRODUCT_PID_CONTROLLER_ADAPTER_DISABLED;
        adapter->last_ck_error = CK_NO_ERR;
        return;
    }
    adapter->reset_input_previous = sample.reset;

    if (!sample.enable)
    {
        Deactivate(adapter, PRODUCT_PID_CONTROLLER_ADAPTER_DISABLED);
        return;
    }
    if (!sample.valid)
    {
        Deactivate(adapter,
                   PRODUCT_PID_CONTROLLER_ADAPTER_INPUT_INVALID);
        return;
    }
    if ((adapter->config.max_sample_age_ms != 0U) &&
        (adapter->sample_age_ms > adapter->config.max_sample_age_ms))
    {
        Deactivate(adapter, PRODUCT_PID_CONTROLLER_ADAPTER_INPUT_STALE);
        return;
    }
    if (!adapter->active)
    {
        if (!adapter->state_reset && !ResetRuntimeState(adapter))
        {
            return;
        }
        adapter->active = true;
    }
    adapter->state_reset = false;
    ClearOutput(adapter);
    if ((adapter->config.mode == PRODUCT_PID_CONTROLLER_MODE_AUTO_TUNE) &&
        !adapter->auto_tune_completed)
    {
        error = RunAutoTune(adapter, &sample, elapsed_ms);
    }
    else
    {
        error = RunController(adapter, &sample, elapsed_ms);
    }
    if (!CK_SUCCESS(error))
    {
        ProductPidControllerAdapterStatus_t status =
            ((adapter->config.mode ==
              PRODUCT_PID_CONTROLLER_MODE_AUTO_TUNE) &&
             !adapter->auto_tune_completed &&
             !AutoTunePeriodValid(adapter, elapsed_ms)) ?
                PRODUCT_PID_CONTROLLER_ADAPTER_INVALID_TIMING :
                PRODUCT_PID_CONTROLLER_ADAPTER_CKLIB_ERROR;
        Deactivate(adapter, status);
        if (adapter->last_status != PRODUCT_PID_CONTROLLER_ADAPTER_CKLIB_ERROR)
        {
            adapter->last_status = status;
            adapter->last_ck_error = error;
        }
        return;
    }
    if (adapter->output.mv_count > 2U)
    {
        Deactivate(adapter, PRODUCT_PID_CONTROLLER_ADAPTER_CKLIB_ERROR);
        adapter->last_status = PRODUCT_PID_CONTROLLER_ADAPTER_CKLIB_ERROR;
        adapter->last_ck_error = CK_ERROR_INVALID_PARAMETER;
        return;
    }
    adapter->output_valid = (adapter->output.mv_count > 0U);
    adapter->last_status = PRODUCT_PID_CONTROLLER_ADAPTER_OK;
    adapter->last_ck_error = CK_NO_ERR;
}

bool ProductPidControllerAdapter_GetOutput(
    const ProductPidControllerAdapter_t *adapter,
    CK_OUTPUT *output,
    bool *valid)
{
    if ((adapter == NULL) || (output == NULL) || (valid == NULL))
    {
        return false;
    }
    *output = adapter->output;
    *valid = adapter->output_valid;
    return true;
}

bool ProductPidControllerAdapter_GetDiagnostics(
    const ProductPidControllerAdapter_t *adapter,
    ProductPidControllerAdapterDiagnostics_t *diagnostics)
{
    if ((adapter == NULL) || (diagnostics == NULL))
    {
        return false;
    }
    diagnostics->output = adapter->output;
    diagnostics->last_ck_error = adapter->last_ck_error;
    diagnostics->last_status = adapter->last_status;
    diagnostics->callback_count = adapter->callback_count;
    diagnostics->last_elapsed_ms = adapter->last_elapsed_ms;
    diagnostics->sample_age_ms = adapter->sample_age_ms;
    diagnostics->active = adapter->active;
    diagnostics->output_valid = adapter->output_valid;
    diagnostics->auto_tune_completed = adapter->auto_tune_completed;
    return true;
}

CK_CONTROLLER ProductPidControllerAdapter_GetControllerHandle(
    ProductPidControllerAdapter_t *adapter)
{
    return (adapter == NULL) ? NULL : adapter->controller;
}

CK_AUTO_TUNE ProductPidControllerAdapter_GetAutoTuneHandle(
    ProductPidControllerAdapter_t *adapter)
{
    return (adapter == NULL) ? NULL : adapter->auto_tune;
}
