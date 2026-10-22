#include "product_fb_schedule_integration.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "product_sensor_measurement_service.h"
#include "DigitalInputService.h"

static ProductSensorMeasurementSnapshot_t g_snapshot;
static bool g_snapshot_available = true;

bool DigitalInputService_GetSnapshot(DigitalInputSnapshot_t *snapshot)
{
    (void)snapshot;
    return false;
}

bool ProductSensorMeasurementService_GetSnapshot(
    uint8_t channel,
    ProductSensorMeasurementSnapshot_t *snapshot)
{
    if ((channel != 0U) || (snapshot == NULL) || !g_snapshot_available)
    {
        return false;
    }
    *snapshot = g_snapshot;
    return true;
}

static FbMovingAverageOutput GetOutput(void)
{
    FbMovingAverageOutput output;
    assert(ProductFbScheduleIntegration_GetCh0MovingAverage(&output));
    return output;
}

static ProductFbMovingAverageAdapterDiagnostics_t GetDiagnostics(void)
{
    ProductFbMovingAverageAdapterDiagnostics_t diagnostics;
    assert(ProductFbScheduleIntegration_GetCh0Diagnostics(&diagnostics));
    return diagnostics;
}

int main(void)
{
    FbMovingAverageOutput output;
    ProductFbMovingAverageAdapterDiagnostics_t diagnostics;

    assert(ProductFbScheduleIntegration_Init(100U));
    g_snapshot.sample_sequence = 1U;
    g_snapshot.engineering_value = 100;
    g_snapshot.valid = true;

    ProductFbScheduleIntegration_Process(119U);
    diagnostics = GetDiagnostics();
    assert(diagnostics.callback_count == 0U);
    output = GetOutput();
    assert((output.valid == 0) && (output.sample_count == 0U));

    ProductFbScheduleIntegration_Process(120U);
    diagnostics = GetDiagnostics();
    output = GetOutput();
    assert(diagnostics.callback_count == 1U);
    assert(diagnostics.last_elapsed_ms == 20U);
    assert(diagnostics.last_status == FB_MOVING_AVERAGE_OK);
    assert((output.valid != 0) && (output.sample_count == 1U));
    assert(output.average == 100.0);

    /* A scheduled callback with the same sequence must not consume PV twice. */
    ProductFbScheduleIntegration_Process(140U);
    diagnostics = GetDiagnostics();
    output = GetOutput();
    assert(diagnostics.callback_count == 2U);
    assert((output.sample_count == 1U) && (output.average == 100.0));

    g_snapshot.sample_sequence = 2U;
    g_snapshot.engineering_value = 200;
    ProductFbScheduleIntegration_Process(160U);
    output = GetOutput();
    assert((output.sample_count == 2U) && (output.average == 150.0));

    /* Invalid input clears old history; repeated invalid cycles stay empty. */
    g_snapshot.valid = false;
    ProductFbScheduleIntegration_Process(180U);
    output = GetOutput();
    assert((output.valid == 0) && (output.sample_count == 0U));
    ProductFbScheduleIntegration_Process(200U);
    output = GetOutput();
    assert((output.valid == 0) && (output.sample_count == 0U));

    /* Recovery restarts the window, including when the sequence is reused. */
    g_snapshot.valid = true;
    g_snapshot.engineering_value = 250;
    ProductFbScheduleIntegration_Process(220U);
    output = GetOutput();
    assert((output.valid != 0) && (output.sample_count == 1U));
    assert(output.average == 250.0);

    /* Late calls coalesce to one callback and report actual elapsed time. */
    ProductFbScheduleIntegration_Process(280U);
    diagnostics = GetDiagnostics();
    output = GetOutput();
    assert(diagnostics.callback_count == 7U);
    assert(diagnostics.last_elapsed_ms == 60U);
    assert((output.sample_count == 1U) && (output.average == 250.0));

    /* A missing snapshot is also invalid; a later valid sample recovers. */
    g_snapshot_available = false;
    ProductFbScheduleIntegration_Process(300U);
    output = GetOutput();
    assert((output.valid == 0) && (output.sample_count == 0U));
    g_snapshot_available = true;
    g_snapshot.sample_sequence = 3U;
    g_snapshot.engineering_value = -40;
    ProductFbScheduleIntegration_Process(320U);
    output = GetOutput();
    assert((output.valid != 0) && (output.sample_count == 1U));
    assert(output.average == -40.0);

    /* Reinitialization resets scheduler phase and all FB history. */
    assert(ProductFbScheduleIntegration_Init(500U));
    output = GetOutput();
    assert((output.valid == 0) && (output.sample_count == 0U));
    g_snapshot.sample_sequence = 4U;
    g_snapshot.engineering_value = 40;
    ProductFbScheduleIntegration_Process(519U);
    diagnostics = GetDiagnostics();
    assert(diagnostics.callback_count == 0U);
    ProductFbScheduleIntegration_Process(520U);
    diagnostics = GetDiagnostics();
    output = GetOutput();
    assert(diagnostics.callback_count == 1U);
    assert(diagnostics.last_elapsed_ms == 20U);
    assert((output.sample_count == 1U) && (output.average == 40.0));

    puts("PASS: L05 CH0 MovingAverage integration timing, sample gating, invalid recovery, and reset");
    return 0;
}
