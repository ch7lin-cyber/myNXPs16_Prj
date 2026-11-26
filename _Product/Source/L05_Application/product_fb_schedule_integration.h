#ifndef PRODUCT_FB_SCHEDULE_INTEGRATION_H
#define PRODUCT_FB_SCHEDULE_INTEGRATION_H

#include <stdbool.h>
#include <stdint.h>

#include "FbMovingAverage.h"
#include "product_fb_moving_average_adapter.h"
#include "product_alarm_logic_adapter.h"
#include "product_pv_processing_adapter.h"

#ifndef PRODUCT_PID_CH0_TEST_ENABLED
#define PRODUCT_PID_CH0_TEST_ENABLED 0
#endif
#if PRODUCT_PID_CH0_TEST_ENABLED
#include "product_pid_controller_adapter.h"
#if !FB_PID_TEST_BACKEND
#error "CH0 test requires build-wide FB_PID_TEST_BACKEND=1"
#endif
typedef struct {
    ProductPidControllerAdapterDiagnostics_t controller;
    ProductPidControllerInput_t input;
    bool fixed_mv_test_backend;
} ProductCh0PidTestDiagnostics;
/* SWD only: edit controls while halted; resume then halt to read the snapshot. */
extern volatile uint32_t g_ch0_pid_test_enable, g_ch0_pid_test_reset;
extern volatile int32_t g_ch0_pid_test_sv, g_ch0_pid_test_mv;
/* SV is a request shadow processed through SetChannelSv; result is ProductChannelSvResult. */
extern volatile uint32_t g_ch0_pid_test_sv_result;
extern volatile ProductCh0PidTestDiagnostics g_ch0_pid_test_watch;
/* Main-loop APIs; no Register or physical output. SV uses 0.001 C, MV 0.1%. */
#ifdef __cplusplus
extern "C" {
#endif
bool ProductFbScheduleIntegration_ConfigureCh0PidTest(bool enable, int32_t sv, int32_t mv);
bool ProductFbScheduleIntegration_ResetCh0PidTest(void);
bool ProductFbScheduleIntegration_GetCh0PidTest(ProductCh0PidTestDiagnostics *out);
#ifdef __cplusplus
}
#endif
#endif

/* Debug-only observation; disabled builds allocate no trace storage. */
#ifndef PRODUCT_FB_CH0_TRACE_ENABLED
#define PRODUCT_FB_CH0_TRACE_ENABLED 0
#endif
#if PRODUCT_FB_CH0_TRACE_ENABLED
#define PRODUCT_FB_CH0_TRACE_CAPACITY 512U
typedef struct
{
    uint32_t now_ms, elapsed_ms, lateness_ms;
    uint32_t sequence;
    int32_t pv;
    uint32_t provider_ok, input_valid, accepted;
    uint32_t sample_count, output_valid;
    double average;
    uint32_t status, cycles, cycles_valid;
} ProductFbCh0TraceRecord_t;
typedef struct
{
    uint32_t schema_version, init_ms;
    uint32_t count;
    uint32_t capture; /* Set to 0 through SWD before export. Resume only if not full. */
    ProductFbCh0TraceRecord_t records[PRODUCT_FB_CH0_TRACE_CAPACITY];
} ProductFbCh0Trace_t;
extern volatile ProductFbCh0Trace_t g_product_fb_ch0_trace;
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    PRODUCT_CHANNEL_SV_OK=0, PRODUCT_CHANNEL_SV_UNCHANGED,
    PRODUCT_CHANNEL_SV_NOT_INITIALIZED, PRODUCT_CHANNEL_SV_INVALID_ARGUMENT,
    PRODUCT_CHANNEL_SV_RANGE_UNAVAILABLE, PRODUCT_CHANNEL_SV_OUT_OF_RANGE,
    PRODUCT_CHANNEL_SV_ALARM_RANGE, PRODUCT_CHANNEL_SV_AT_BUSY,
    PRODUCT_CHANNEL_SV_CONSUMER_REJECTED, PRODUCT_CHANNEL_SV_BUSY
} ProductChannelSvResult;
typedef enum { PRODUCT_CHANNEL_SV_PID=0, PRODUCT_CHANNEL_SV_RAMP,
               PRODUCT_CHANNEL_SV_CONSUMER_COUNT } ProductChannelSvConsumerSlot;
typedef struct { int32_t target; uint32_t revision; bool configured; } ProductChannelSv;
/* Validate is read-only; return OK or AT_BUSY/CONSUMER_REJECTED.
 * Commit cannot fail, allocate, reset histories, reenter, or operate hardware.
 * All callbacks/API calls run serially in the main loop, never ISR context. */
typedef ProductChannelSvResult (*ProductChannelSvValidate)(void *, int32_t);
typedef void (*ProductChannelSvCommit)(void *, int32_t);
typedef bool (*ProductChannelSvAtRunning)(void *);
typedef struct {
    ProductChannelSvValidate validate;
    ProductChannelSvCommit commit;
    ProductChannelSvAtRunning at_running; /* mandatory for a PID consumer */
    void *context;
} ProductChannelSvConsumer;
bool ProductFbScheduleIntegration_Init(uint32_t now_ms);
/* Temperature target in 0.001 C; rejects all-or-nothing. No Reset or PV invalidation. */
ProductChannelSvResult ProductFbScheduleIntegration_SetChannelSv(uint8_t channel, int32_t sv);
bool ProductFbScheduleIntegration_GetChannelSv(uint8_t channel, ProductChannelSv *out);
/* Bind while stopped; NULL unbinds. Existing accepted target is validated/applied
 * to a new consumer. Built-in CH0 PID test slot cannot be replaced/unbound. */
bool ProductFbScheduleIntegration_BindChannelSvConsumer(uint8_t channel,
    ProductChannelSvConsumerSlot slot, const ProductChannelSvConsumer *consumer);
bool ProductFbScheduleIntegration_ConfigureCh0PvProcessing(const ProductPvProcessingConfig *config);
bool ProductFbScheduleIntegration_GetCh0ProcessedPv(ProductPvProcessingOutput *output);
typedef enum { PRODUCT_CH0_ALARM_PV_RAW=0,
               PRODUCT_CH0_ALARM_PV_PROCESSED } ProductCh0AlarmPvSource;
/* Main loop only; changing source requires all CH0 alarms disabled. */
bool ProductFbScheduleIntegration_SetCh0AlarmPvSource(ProductCh0AlarmPvSource source);
ProductCh0AlarmPvSource ProductFbScheduleIntegration_GetCh0AlarmPvSource(void);
typedef struct {
    uint32_t configuration_revision, notification_count;
    bool moving_average_reset, pv_reset, pid_reset;
    uint32_t alarm_channels_reset; /* bit per logical Alarm channel */
} ProductChannelConfigurationNotification;
/* Last delivered notification persists across FB release periods. Main loop only. */
bool ProductFbScheduleIntegration_GetChannelConfigurationNotification(
    uint8_t channel, ProductChannelConfigurationNotification *out);
bool ProductFbScheduleIntegration_IsCh0PvConfigurationBlocked(void);
void ProductFbScheduleIntegration_Process(uint32_t now_ms);
/* Main-loop configuration and command APIs are on this product-owned adapter. */
ProductAlarmLogicAdapter_t *ProductFbScheduleIntegration_GetAlarmLogicAdapter(void);
bool ProductFbScheduleIntegration_GetCh0MovingAverage(
    FbMovingAverageOutput *output);
bool ProductFbScheduleIntegration_GetCh0Diagnostics(
    ProductFbMovingAverageAdapterDiagnostics_t *diagnostics);

#ifdef __cplusplus
}
#endif

#endif /* PRODUCT_FB_SCHEDULE_INTEGRATION_H */
