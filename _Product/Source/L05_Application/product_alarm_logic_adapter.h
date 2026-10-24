#ifndef PRODUCT_ALARM_LOGIC_ADAPTER_H
#define PRODUCT_ALARM_LOGIC_ADAPTER_H

#include "FbAlarm.h"
#include "FbLogic.h"
#include "ProductAlarmLogicConfig.h"

typedef enum
{
    PRODUCT_AL_SOURCE_ZERO = 0,
    PRODUCT_AL_SOURCE_ONE,
    PRODUCT_AL_SOURCE_ALARM,
    PRODUCT_AL_SOURCE_LOGIC,
    PRODUCT_AL_SOURCE_EXTERNAL
} ProductAlSourceKind_t;

typedef struct
{
    ProductAlSourceKind_t kind;
    uint8_t channel; /* Alarm channel; ignored for other kinds */
    uint8_t index; /* alarm number, logic number or external Boolean source */
} ProductAlSource_t;

typedef struct
{
    int32_t pv; /* 0.001 C; provider must reject other engineering units */
    uint32_t sequence;
    bool valid;
} ProductAlSample_t;

typedef bool (*ProductAlReadSample_t)(void *, uint8_t, ProductAlSample_t *);
typedef bool (*ProductAlReadBoolean_t)(void *, uint8_t, bool *);
typedef void (*ProductAlPidStep_t)(void *, uint32_t);

typedef struct
{
    FbAlarm_t fb;
    FbAlarmConfig_t config;
    FbAlarmOutput_t output;
    bool enabled;
    bool reset_pending;
    bool clear_pending;
    bool reset_high;
    bool clear_high;
} ProductAlAlarm_t;

typedef struct
{
    FbLogic_t fb;
    FbLogicConfig_t config;
    FbLogicOutput_t output;
    ProductAlSource_t sources[FB_LOGIC_INPUT_COUNT];
    bool enabled;
    bool valid;
} ProductAlLogic_t;

typedef struct
{
    ProductAlAlarm_t alarms[PRODUCT_AL_CHANNEL_COUNT][PRODUCT_AL_ALARMS_PER_CHANNEL];
    ProductAlLogic_t logic[PRODUCT_AL_LOGIC_COUNT];
    int32_t setpoint[PRODUCT_AL_CHANNEL_COUNT];
    uint32_t sequence[PRODUCT_AL_CHANNEL_COUNT];
    uint32_t age_ms[PRODUCT_AL_CHANNEL_COUNT];
    bool has_sequence[PRODUCT_AL_CHANNEL_COUNT];
    ProductAlReadSample_t read_sample;
    ProductAlReadBoolean_t read_boolean;
    ProductAlPidStep_t pid_step;
    void *provider_context;
    void *pid_context;
    uint32_t cycle_count;
    bool initialized;
} ProductAlarmLogicAdapter_t;

bool ProductAlarmLogicAdapter_Init(ProductAlarmLogicAdapter_t *adapter,
    ProductAlReadSample_t read_sample, ProductAlReadBoolean_t read_boolean,
    void *provider_context);
bool ProductAlarmLogicAdapter_SetAlarm(ProductAlarmLogicAdapter_t *adapter,
    uint8_t channel, uint8_t alarm, const FbAlarmConfig_t *config, bool enabled);
bool ProductAlarmLogicAdapter_SetSetpoint(ProductAlarmLogicAdapter_t *adapter,
    uint8_t channel, int32_t sv);
/* Source/configuration transition only; rejects if any channel alarm enabled.
 * Clears runtime, peaks, queued commands and sample age; keeps config/SV. */
bool ProductAlarmLogicAdapter_ResetDisabledChannel(ProductAlarmLogicAdapter_t *adapter,
    uint8_t channel);
bool ProductAlarmLogicAdapter_Command(ProductAlarmLogicAdapter_t *adapter,
    uint8_t channel, uint8_t alarm, bool reset, bool clear_peak);
bool ProductAlarmLogicAdapter_SetLogic(ProductAlarmLogicAdapter_t *adapter,
    uint8_t logic, const FbLogicConfig_t *config,
    const ProductAlSource_t sources[FB_LOGIC_INPUT_COUNT], bool enabled);
bool ProductAlarmLogicAdapter_SetPidStep(ProductAlarmLogicAdapter_t *adapter,
    ProductAlPidStep_t callback, void *context);
void ProductAlarmLogicAdapter_Process(void *context, uint32_t elapsed_ms);
bool ProductAlarmLogicAdapter_GetAlarm(const ProductAlarmLogicAdapter_t *adapter,
    uint8_t channel, uint8_t alarm, FbAlarmOutput_t *output);
bool ProductAlarmLogicAdapter_GetLogic(const ProductAlarmLogicAdapter_t *adapter,
    uint8_t logic, FbLogicOutput_t *output, bool *valid);

/* Main loop only. Setters copy configuration. Source references are zero-based.
 * Higher-numbered Logic references use the previous cycle; lower ones use this
 * cycle. All instances start disabled. No GPIO/register/NVM writes occur here. */
#endif
