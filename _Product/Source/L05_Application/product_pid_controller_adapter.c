#include "product_pid_controller_adapter.h"

#include <limits.h>
#include <stddef.h>
#include <string.h>

static void AddAge(ProductPidControllerAdapter_t *adapter, uint32_t elapsed_ms)
{
    adapter->sample_age_ms = (UINT32_MAX - adapter->sample_age_ms < elapsed_ms) ?
                             UINT32_MAX : adapter->sample_age_ms + elapsed_ms;
}

static void UpdateAge(ProductPidControllerAdapter_t *adapter,
                      const ProductPidControllerInput_t *sample,
                      uint32_t elapsed_ms)
{
    if (!adapter->has_sample_sequence ||
        sample->sample_sequence != adapter->last_sample_sequence)
    {
        adapter->has_sample_sequence = true;
        adapter->last_sample_sequence = sample->sample_sequence;
        adapter->sample_age_ms = 0U;
    }
    else AddAge(adapter, elapsed_ms);
}

static ProductPidControllerAdapterStatus_t MapStatus(FbPidStatus_t status)
{
    switch (status)
    {
        case FB_PID_STATUS_OK:
        case FB_PID_STATUS_RESET: return PRODUCT_PID_CONTROLLER_ADAPTER_OK;
        case FB_PID_STATUS_DISABLED: return PRODUCT_PID_CONTROLLER_ADAPTER_DISABLED;
        case FB_PID_STATUS_INVALID_INPUT: return PRODUCT_PID_CONTROLLER_ADAPTER_INPUT_INVALID;
        case FB_PID_STATUS_INVALID_TIMING: return PRODUCT_PID_CONTROLLER_ADAPTER_INVALID_TIMING;
        case FB_PID_STATUS_NOT_INITIALIZED: return PRODUCT_PID_CONTROLLER_ADAPTER_NOT_INITIALIZED;
        default: return PRODUCT_PID_CONTROLLER_ADAPTER_FB_ERROR;
    }
}

bool ProductPidControllerAdapter_Init(
    ProductPidControllerAdapter_t *adapter,
    const ProductPidControllerAdapterConfig_t *config,
    ProductPidControllerSampleProvider_t sample_provider,
    void *sample_provider_context)
{
    FbPidConfig_t fb_config;
    FbPidStatus_t status;
    if (adapter == NULL || config == NULL || sample_provider == NULL)
        return false;
    (void)memset(adapter, 0, sizeof(*adapter));
    fb_config.mode = config->mode;
    fb_config.auto_tune_target_mode = config->auto_tune_target_mode;
    fb_config.control_action = config->control_action;
    fb_config.auto_tune_period_ms = config->auto_tune_period_ms;
    fb_config.auto_tune_period_tolerance_ms = config->auto_tune_period_tolerance_ms;
    status = FbPid_Init(&adapter->fb_pid, &fb_config);
    if (status != FB_PID_STATUS_OK)
    {
        adapter->last_status = PRODUCT_PID_CONTROLLER_ADAPTER_FB_ERROR;
        return false;
    }
    adapter->config = *config;
    adapter->sample_provider = sample_provider;
    adapter->sample_provider_context = sample_provider_context;
    adapter->initialized = true;
    adapter->last_status = PRODUCT_PID_CONTROLLER_ADAPTER_OK;
    return true;
}

void ProductPidControllerAdapter_Deinit(ProductPidControllerAdapter_t *adapter)
{
    if (adapter == NULL) return;
    FbPid_Deinit(&adapter->fb_pid);
    adapter->initialized = false;
    adapter->output.valid = false;
    adapter->output.mv_count = 0U;
    adapter->last_status = PRODUCT_PID_CONTROLLER_ADAPTER_NOT_INITIALIZED;
}

bool ProductPidControllerAdapter_Reset(ProductPidControllerAdapter_t *adapter)
{
    FbPidStatus_t status;
    if (adapter == NULL || !adapter->initialized) return false;
    status = FbPid_Reset(&adapter->fb_pid);
    adapter->output = adapter->fb_pid.output;
    adapter->last_status = MapStatus(status);
    return status == FB_PID_STATUS_RESET;
}

FbPidStatus_t ProductPidControllerAdapter_SetParameter(
    ProductPidControllerAdapter_t *adapter,
    FbPidParameter_t parameter,
    int32_t value)
{
    if (adapter == NULL || !adapter->initialized)
        return FB_PID_STATUS_NOT_INITIALIZED;
    return FbPid_SetParameter(&adapter->fb_pid, parameter, value);
}

void ProductPidControllerAdapter_Process(void *context, uint32_t elapsed_ms)
{
    ProductPidControllerAdapter_t *adapter = (ProductPidControllerAdapter_t *)context;
    ProductPidControllerInput_t sample;
    FbPidInput_t fb_input;
    FbPidStatus_t fb_status;
    if (adapter == NULL) return;
    adapter->callback_count++;
    adapter->last_elapsed_ms = elapsed_ms;
    if (!adapter->initialized)
    {
        adapter->last_status = PRODUCT_PID_CONTROLLER_ADAPTER_NOT_INITIALIZED;
        return;
    }
    (void)memset(&sample, 0, sizeof(sample));
    if (!adapter->sample_provider(adapter->sample_provider_context, &sample))
    {
        AddAge(adapter, elapsed_ms);
        fb_input.pv = 0;
        fb_input.sv = 0;
        fb_input.elapsed_ms = elapsed_ms;
        fb_input.enable = false;
        fb_input.reset = false;
        fb_input.valid = false;
        fb_status = FbPid_Execute(&adapter->fb_pid, &fb_input, &adapter->output);
        adapter->last_status = PRODUCT_PID_CONTROLLER_ADAPTER_INPUT_UNAVAILABLE;
        (void)fb_status;
        return;
    }
    UpdateAge(adapter, &sample, elapsed_ms);
    fb_input.pv = sample.pv;
    fb_input.sv = sample.sv;
    fb_input.elapsed_ms = elapsed_ms;
    fb_input.enable = sample.enable;
    fb_input.reset = sample.reset;
    fb_input.valid = sample.valid;
    if (sample.valid && adapter->config.max_sample_age_ms != 0U &&
        adapter->sample_age_ms > adapter->config.max_sample_age_ms)
        fb_input.valid = false;
    fb_status = FbPid_Execute(&adapter->fb_pid, &fb_input, &adapter->output);
    if (sample.valid && !fb_input.valid)
        adapter->last_status = PRODUCT_PID_CONTROLLER_ADAPTER_INPUT_STALE;
    else adapter->last_status = MapStatus(fb_status);
}

bool ProductPidControllerAdapter_GetOutput(
    const ProductPidControllerAdapter_t *adapter,
    FbPidOutput_t *output,
    bool *valid)
{
    if (adapter == NULL || output == NULL || valid == NULL) return false;
    *output = adapter->output;
    *valid = adapter->output.valid;
    return true;
}

bool ProductPidControllerAdapter_GetDiagnostics(
    const ProductPidControllerAdapter_t *adapter,
    ProductPidControllerAdapterDiagnostics_t *diagnostics)
{
    if (adapter == NULL || diagnostics == NULL) return false;
    diagnostics->output = adapter->output;
    diagnostics->last_status = adapter->last_status;
    diagnostics->callback_count = adapter->callback_count;
    diagnostics->last_elapsed_ms = adapter->last_elapsed_ms;
    diagnostics->sample_age_ms = adapter->sample_age_ms;
    diagnostics->active = adapter->fb_pid.active;
    diagnostics->output_valid = adapter->output.valid;
    diagnostics->auto_tune_completed = adapter->fb_pid.auto_tune_completed;
    return true;
}
