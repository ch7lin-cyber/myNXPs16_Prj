#include "product_fb_moving_average_adapter.h"

#include <stddef.h>

bool ProductFbMovingAverageAdapter_Init(
    ProductFbMovingAverageAdapter_t *adapter,
    ProductFbMovingAverageSampleProvider_t sample_provider,
    void *sample_provider_context)
{
    if ((adapter == NULL) || (sample_provider == NULL))
    {
        return false;
    }

    adapter->fb_config.window_size = PRODUCT_FB_MOVING_AVERAGE_WINDOW_SIZE;
    adapter->output.average = 0.0;
    adapter->output.sample_count = 0U;
    adapter->output.valid = 0;
    adapter->last_sample_sequence = 0U;
    adapter->callback_count = 0U;
    adapter->last_elapsed_ms = 0U;
    adapter->last_status = FB_MOVING_AVERAGE_INVALID_ARGUMENT;
    adapter->has_last_sample_sequence = false;
    adapter->reset_pending = true;
    adapter->input_invalid = false;
    adapter->sample_provider = sample_provider;
    adapter->sample_provider_context = sample_provider_context;
    return FbMovingAverage_Init(&adapter->fb_state,
                                adapter->buffer,
                                PRODUCT_FB_MOVING_AVERAGE_WINDOW_SIZE) ==
           FB_MOVING_AVERAGE_OK;
}

void ProductFbMovingAverageAdapter_Process(void *context,
                                           uint32_t elapsed_ms)
{
    ProductFbMovingAverageAdapter_t *adapter =
        (ProductFbMovingAverageAdapter_t *)context;
    ProductFbMovingAverageSample_t sample;
    FbMovingAverageInput input = {0};

    if (adapter == NULL)
    {
        return;
    }

    adapter->callback_count++;
    adapter->last_elapsed_ms = elapsed_ms;
    input.sample = 0.0;
    input.enable = 1;
    input.sample_valid = 0;
    input.reset = 0;

    if (!adapter->sample_provider(
            adapter->sample_provider_context, &sample) || !sample.valid)
    {
        input.reset = adapter->input_invalid ? 0 : 1;
        adapter->input_invalid = true;
        adapter->has_last_sample_sequence = false;
        adapter->reset_pending = true;
    }
    else
    {
        adapter->input_invalid = false;
        if (!adapter->has_last_sample_sequence ||
            (sample.sample_sequence != adapter->last_sample_sequence))
        {
            input.sample = (double)sample.engineering_value;
            input.sample_valid = 1;
            input.reset = adapter->reset_pending ? 1 : 0;
            adapter->last_sample_sequence = sample.sample_sequence;
            adapter->has_last_sample_sequence = true;
            adapter->reset_pending = false;
        }
    }

    adapter->last_status = FbMovingAverage_Execute(
        &adapter->fb_config, &input, &adapter->fb_state, &adapter->output);
    if (adapter->last_status != FB_MOVING_AVERAGE_OK)
    {
        adapter->output.average = 0.0;
        adapter->output.sample_count = 0U;
        adapter->output.valid = 0;
        adapter->has_last_sample_sequence = false;
        adapter->reset_pending = true;
        adapter->input_invalid = true;
    }
}

bool ProductFbMovingAverageAdapter_GetOutput(
    const ProductFbMovingAverageAdapter_t *adapter,
    FbMovingAverageOutput *output)
{
    if ((adapter == NULL) || (output == NULL))
    {
        return false;
    }
    *output = adapter->output;
    return true;
}

bool ProductFbMovingAverageAdapter_GetDiagnostics(
    const ProductFbMovingAverageAdapter_t *adapter,
    ProductFbMovingAverageAdapterDiagnostics_t *diagnostics)
{
    if ((adapter == NULL) || (diagnostics == NULL))
    {
        return false;
    }
    diagnostics->callback_count = adapter->callback_count;
    diagnostics->last_elapsed_ms = adapter->last_elapsed_ms;
    diagnostics->last_status = adapter->last_status;
    return true;
}
