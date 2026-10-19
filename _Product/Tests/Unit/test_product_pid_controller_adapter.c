#include "product_pid_controller_adapter.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

struct _CK_CONTROLLER
{
    ProductPidControllerMode_t mode;
    int32_t pv;
    int32_t sv;
    int32_t step_time;
    CK_CONTROL_ACTION action;
};

struct _CK_AUTO_TUNE
{
    int32_t sv;
    uint32_t calls;
};

static struct _CK_CONTROLLER g_controller;
static struct _CK_AUTO_TUNE g_auto_tune;
static ProductPidControllerInput_t g_sample;
static uint32_t g_create_count;
static uint32_t g_apply_count;
static uint32_t g_controller_control_count;
static uint32_t g_controller_reset_count;
static uint32_t g_auto_tune_reset_count;
static bool g_provider_available = true;

CK_CONTROLLER ckCreateDtOnOffController(void)
{
    (void)memset(&g_controller, 0, sizeof(g_controller));
    g_controller.mode = PRODUCT_PID_CONTROLLER_MODE_ON_OFF_CONTROL;
    g_create_count++;
    return &g_controller;
}

CK_CONTROLLER ckCreateDtPidController(void)
{
    (void)memset(&g_controller, 0, sizeof(g_controller));
    g_controller.mode = PRODUCT_PID_CONTROLLER_MODE_PID_CONTROL;
    g_create_count++;
    return &g_controller;
}

CK_CONTROLLER ckCreateDtPidFbController(void)
{
    (void)memset(&g_controller, 0, sizeof(g_controller));
    g_controller.mode = PRODUCT_PID_CONTROLLER_MODE_PID_FB_CONTROL;
    g_create_count++;
    return &g_controller;
}

CK_AUTO_TUNE ckCreateDtAutoTune(void)
{
    (void)memset(&g_auto_tune, 0, sizeof(g_auto_tune));
    return &g_auto_tune;
}

CK_ERROR ckControllerSetEnumParam(CK_CONTROLLER controller,
                                  CK_CONTROLLER_PARAM type,
                                  int32_t value)
{
    (void)type;
    controller->action = (CK_CONTROL_ACTION)value;
    return CK_NO_ERR;
}

CK_ERROR ckAutoTuneSetEnumParam(CK_AUTO_TUNE autotune,
                                CK_AUTO_TUNE_PARAM type,
                                int32_t value)
{
    (void)autotune;
    (void)type;
    (void)value;
    return CK_NO_ERR;
}

CK_ERROR ckControllerFree(CK_CONTROLLER controller)
{
    (void)controller;
    return CK_NO_ERR;
}

CK_ERROR ckAutoTuneFree(CK_AUTO_TUNE autotune)
{
    (void)autotune;
    return CK_NO_ERR;
}

CK_ERROR ckControllerReset(CK_CONTROLLER controller)
{
    g_controller_reset_count++;
    controller->pv = 0;
    controller->sv = 0;
    controller->step_time = 0;
    return CK_NO_ERR;
}

CK_ERROR ckAutoTuneReset(CK_AUTO_TUNE autotune)
{
    g_auto_tune_reset_count++;
    autotune->calls = 0U;
    return CK_NO_ERR;
}

CK_ERROR ckControllerSetPV(CK_CONTROLLER controller, int32_t value)
{
    controller->pv = value;
    return CK_NO_ERR;
}

CK_ERROR ckControllerSetSV(CK_CONTROLLER controller, int32_t value)
{
    controller->sv = value;
    return CK_NO_ERR;
}

CK_ERROR ckControllerSetStepTime(CK_CONTROLLER controller, int32_t value)
{
    controller->step_time = value;
    return CK_NO_ERR;
}

CK_ERROR ckControllerControl(CK_CONTROLLER controller, CK_OUTPUT *output)
{
    g_controller_control_count++;
    output->mv[0] = controller->pv + controller->sv;
    output->mv[1] = controller->step_time;
    output->mv_count = 1U;
    return CK_NO_ERR;
}

CK_ERROR ckAutoTuneSetSV(CK_AUTO_TUNE autotune, int32_t value)
{
    autotune->sv = value;
    return CK_NO_ERR;
}

CK_ERROR ckAutoTuneCollectAndSignal(CK_AUTO_TUNE autotune,
                                    control_value_t pv,
                                    CK_OUTPUT *output)
{
    (void)pv;
    autotune->calls++;
    output->mv[0] = 500;
    output->mv[1] = 0;
    output->mv_count = (autotune->calls == 1U) ? 1U : 0U;
    return CK_NO_ERR;
}

CK_ERROR ckAutoTuneApplyParams(CK_AUTO_TUNE autotune,
                               CK_CONTROLLER controller)
{
    (void)autotune;
    (void)controller;
    g_apply_count++;
    return CK_NO_ERR;
}

static bool ReadSample(void *context, ProductPidControllerInput_t *sample)
{
    (void)context;
    if (!g_provider_available || (sample == NULL))
    {
        return false;
    }
    *sample = g_sample;
    return true;
}

static ProductPidControllerAdapterConfig_t MakeConfig(
    ProductPidControllerMode_t mode)
{
    ProductPidControllerAdapterConfig_t config;

    (void)memset(&config, 0, sizeof(config));
    config.mode = mode;
    config.auto_tune_target_mode = PRODUCT_PID_CONTROLLER_MODE_PID_CONTROL;
    config.control_action = CK_CONTROL_ACTION_HEAT;
    config.max_sample_age_ms = 25U;
    config.auto_tune_period_ms = 20U;
    config.auto_tune_period_tolerance_ms = 2U;
    return config;
}

static void TestControllerMode(ProductPidControllerMode_t mode)
{
    ProductPidControllerAdapter_t adapter;
    ProductPidControllerAdapterConfig_t config = MakeConfig(mode);
    CK_OUTPUT output;
    bool valid;

    assert(ProductPidControllerAdapter_Init(&adapter, &config,
                                             ReadSample, NULL));
    assert(g_controller.mode == mode);
    assert(ProductPidControllerAdapter_GetControllerHandle(&adapter) != NULL);
    assert(ProductPidControllerAdapter_GetAutoTuneHandle(&adapter) == NULL);
    ProductPidControllerAdapter_Process(&adapter, 20U);
    assert(ProductPidControllerAdapter_GetOutput(&adapter, &output, &valid));
    assert(valid);
    assert(output.mv_count == 1U);
    assert(output.mv[0] == 550);
    assert(g_controller.pv == 250);
    assert(g_controller.sv == 300);
    assert(g_controller.step_time == 2);
    assert(adapter.last_status == PRODUCT_PID_CONTROLLER_ADAPTER_OK);
    assert(adapter.active);

    /* Reusing a sample is allowed while its configured maximum age is met. */
    ProductPidControllerAdapter_Process(&adapter, 20U);
    assert(adapter.output_valid);
    ProductPidControllerAdapter_Process(&adapter, 20U);
    assert(!adapter.output_valid);
    assert(adapter.last_status == PRODUCT_PID_CONTROLLER_ADAPTER_INPUT_STALE);
    assert(!adapter.active);
    assert(ProductPidControllerAdapter_Reset(&adapter));
    assert(!adapter.output_valid);
    ProductPidControllerAdapter_Deinit(&adapter);
    assert(!adapter.initialized);
}

static void TestAutoTuneMode(void)
{
    ProductPidControllerAdapter_t adapter;
    ProductPidControllerAdapterConfig_t config = MakeConfig(
        PRODUCT_PID_CONTROLLER_MODE_AUTO_TUNE);
    CK_OUTPUT output;
    bool valid;
    uint32_t reset_count;

    assert(ProductPidControllerAdapter_Init(&adapter, &config,
                                             ReadSample, NULL));
    assert(g_controller.mode == PRODUCT_PID_CONTROLLER_MODE_PID_CONTROL);
    assert(ProductPidControllerAdapter_GetAutoTuneHandle(&adapter) != NULL);
    assert(ProductPidControllerAdapter_GetControllerHandle(&adapter) != NULL);
    ProductPidControllerAdapter_Process(&adapter, 20U);
    assert(adapter.output_valid);
    assert(adapter.output.mv[0] == 500);
    assert(!adapter.auto_tune_completed);

    reset_count = g_auto_tune_reset_count;
    g_sample.enable = false;
    ProductPidControllerAdapter_Process(&adapter, 20U);
    assert(!adapter.active && !adapter.output_valid);
    assert(g_auto_tune_reset_count == (reset_count + 1U));
    ProductPidControllerAdapter_Process(&adapter, 20U);
    assert(g_auto_tune_reset_count == (reset_count + 1U));

    g_sample.enable = true;
    g_sample.sample_sequence++;
    ProductPidControllerAdapter_Process(&adapter, 20U);
    assert(adapter.active && adapter.output_valid);
    assert(adapter.output.mv[0] == 500);

    ProductPidControllerAdapter_Process(&adapter, 25U);
    assert(adapter.last_status ==
           PRODUCT_PID_CONTROLLER_ADAPTER_INVALID_TIMING);
    assert(!adapter.output_valid);

    g_sample.sample_sequence++;
    ProductPidControllerAdapter_Process(&adapter, 20U);
    assert(adapter.output_valid && !adapter.auto_tune_completed);
    g_sample.sample_sequence++;
    ProductPidControllerAdapter_Process(&adapter, 20U);
    assert(adapter.auto_tune_completed);
    assert(g_apply_count == 1U);
    assert(ProductPidControllerAdapter_GetOutput(&adapter, &output, &valid));
    assert(valid);
    assert(output.mv_count == 1U);
    assert(output.mv[0] == 550);

    assert(ProductPidControllerAdapter_Reset(&adapter));
    assert(!adapter.auto_tune_completed);
    ProductPidControllerAdapter_Deinit(&adapter);
}

static void TestInvalidInput(void)
{
    ProductPidControllerAdapter_t adapter;
    ProductPidControllerAdapterConfig_t config = MakeConfig(
        PRODUCT_PID_CONTROLLER_MODE_PID_FB_CONTROL);

    g_provider_available = false;
    assert(ProductPidControllerAdapter_Init(&adapter, &config,
                                             ReadSample, NULL));
    ProductPidControllerAdapter_Process(&adapter, 20U);
    assert(!adapter.output_valid);
    assert(adapter.last_status ==
           PRODUCT_PID_CONTROLLER_ADAPTER_INPUT_UNAVAILABLE);
    ProductPidControllerAdapter_Deinit(&adapter);
    g_provider_available = true;
}

static void TestEnableResetPins(void)
{
    ProductPidControllerAdapter_t adapter;
    ProductPidControllerAdapterConfig_t config = MakeConfig(
        PRODUCT_PID_CONTROLLER_MODE_PID_CONTROL);
    uint32_t control_count;
    uint32_t reset_count;

    g_sample.enable = true;
    g_sample.reset = false;
    g_sample.valid = true;
    g_sample.sample_sequence++;
    assert(ProductPidControllerAdapter_Init(&adapter, &config,
                                             ReadSample, NULL));
    ProductPidControllerAdapter_Process(&adapter, 20U);
    assert(adapter.active && adapter.output_valid);
    control_count = g_controller_control_count;
    reset_count = g_controller_reset_count;

    g_sample.enable = false;
    ProductPidControllerAdapter_Process(&adapter, 20U);
    assert(!adapter.active && !adapter.output_valid);
    assert(adapter.last_status == PRODUCT_PID_CONTROLLER_ADAPTER_DISABLED);
    assert(g_controller_control_count == control_count);
    assert(g_controller_reset_count == (reset_count + 1U));

    /* Disabled scans must not repeatedly reset or run the controller. */
    ProductPidControllerAdapter_Process(&adapter, 20U);
    assert(g_controller_reset_count == (reset_count + 1U));
    assert(g_controller_control_count == control_count);

    /* Re-enable requires a new sample and resumes from cleared PID state. */
    g_sample.enable = true;
    g_sample.sample_sequence++;
    ProductPidControllerAdapter_Process(&adapter, 20U);
    assert(adapter.active && adapter.output_valid);
    assert(g_controller_control_count == (control_count + 1U));

    /* RESET is rising-edge triggered and suppresses output for that scan. */
    g_sample.reset = true;
    ProductPidControllerAdapter_Process(&adapter, 20U);
    assert(adapter.active && !adapter.output_valid);
    assert(g_controller_reset_count == (reset_count + 2U));
    g_sample.reset = false;
    g_sample.sample_sequence++;
    ProductPidControllerAdapter_Process(&adapter, 20U);
    assert(adapter.active && adapter.output_valid);
    assert(g_controller_reset_count == (reset_count + 2U));
    ProductPidControllerAdapter_Deinit(&adapter);
}

int main(void)
{
    g_sample.sample_sequence = 1U;
    g_sample.pv = 250;
    g_sample.sv = 300;
    g_sample.enable = true;
    g_sample.valid = true;

    TestControllerMode(PRODUCT_PID_CONTROLLER_MODE_ON_OFF_CONTROL);
    TestControllerMode(PRODUCT_PID_CONTROLLER_MODE_PID_CONTROL);
    TestControllerMode(PRODUCT_PID_CONTROLLER_MODE_PID_FB_CONTROL);
    TestAutoTuneMode();
    TestEnableResetPins();
    TestInvalidInput();
    assert(g_create_count == 6U);
    assert(g_auto_tune_reset_count >= 2U);

    puts("PASS: PID adapter modes, ENABLE/RESET transitions, timing, stale input, and AutoTune handoff");
    return 0;
}
