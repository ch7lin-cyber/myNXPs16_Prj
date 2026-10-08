/*
 * Copyright 2026
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef PRODUCT_MODBUS_REGISTER_ADAPTER_H_
#define PRODUCT_MODBUS_REGISTER_ADAPTER_H_

#include <stdbool.h>
#include <stdint.h>

#include "ModbusSlave.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PRODUCT_MODBUS_TEMPERATURE_INPUT_BASE_ADDRESS        (0x1000U)
#define PRODUCT_MODBUS_TEMPERATURE_INPUT_COUNT                (4U)
#define PRODUCT_MODBUS_TEMPERATURE_INPUT_INSTANCE_STRIDE     (0x0010U)

#define PRODUCT_MODBUS_UNFILTERED_PV_ADDRESS                 (0x1000U)
#define PRODUCT_MODBUS_INPUT_ERROR_ADDRESS                   (0x1002U)
#define PRODUCT_MODBUS_FILTER_TIME_CONSTANT_ADDRESS          (0x1003U)
#define PRODUCT_MODBUS_FILTERED_PV_ADDRESS                   (0x1005U)
#define PRODUCT_MODBUS_SENSOR_TYPE_ADDRESS                   (0x1007U)
#define PRODUCT_MODBUS_SENSOR_RESERVED_ADDRESS               (0x1008U)
#define PRODUCT_MODBUS_APPLY_KEY_ADDRESS                     (0x1009U)
#define PRODUCT_MODBUS_APPLY_KEY_VALUE                       (0xA5A5U)
#define PRODUCT_MODBUS_TEMPERATURE_INPUT_LAST_ADDRESS        (0x1039U)

/* Four independent PWM outputs. FC06/FC10 writes stage Pending values. */
#define PRODUCT_MODBUS_PWM_BASE_ADDRESS                       (0x1300U)
#define PRODUCT_MODBUS_PWM_CHANNEL_COUNT                      (4U)
#define PRODUCT_MODBUS_PWM_CHANNEL_STRIDE                     (3U)
#define PRODUCT_MODBUS_PWM_PERIOD_OFFSET                      (0U)
#define PRODUCT_MODBUS_PWM_DUTY_OFFSET                        (1U)
#define PRODUCT_MODBUS_PWM_UPDATE_MODE_OFFSET                 (2U)
#define PRODUCT_MODBUS_PWM_APPLY_KEY_ADDRESS                  (0x130CU)
#define PRODUCT_MODBUS_PWM_REVISION_ADDRESS                   (0x130DU)
#define PRODUCT_MODBUS_PWM_PENDING_MASK_ADDRESS               (0x130EU)
#define PRODUCT_MODBUS_PWM_LAST_ADDRESS                       (0x130EU)
#define PRODUCT_MODBUS_PWM_APPLY_KEY_VALUE                    (0xA5A5U)

#define PRODUCT_MODBUS_PWM_UPDATE_IMMEDIATE                   (0U)
#define PRODUCT_MODBUS_PWM_UPDATE_NEXT_CYCLE                  (1U)

/* Four independent DAC8562 outputs. FC06/FC10 writes stage Pending codes. */
#define PRODUCT_MODBUS_DAC_BASE_ADDRESS                       (0x1400U)
#define PRODUCT_MODBUS_DAC_CHANNEL_COUNT                      (4U)
#define PRODUCT_MODBUS_DAC_APPLY_KEY_ADDRESS                  (0x1404U)
#define PRODUCT_MODBUS_DAC_REVISION_ADDRESS                   (0x1405U)
#define PRODUCT_MODBUS_DAC_PENDING_MASK_ADDRESS               (0x1406U)
#define PRODUCT_MODBUS_DAC_STATUS_ADDRESS                     (0x1407U)
#define PRODUCT_MODBUS_DAC_FAILED_CHANNEL_ADDRESS             (0x1408U)
#define PRODUCT_MODBUS_DAC_LAST_ADDRESS                       (0x1408U)
#define PRODUCT_MODBUS_DAC_APPLY_KEY_VALUE                    (0xA5A5U)

#define PRODUCT_MODBUS_DAC_STATUS_READY                       (0U)
#define PRODUCT_MODBUS_DAC_STATUS_APPLY_FAILED                (1U)
#define PRODUCT_MODBUS_DAC_STATUS_ROLLBACK_FAILED             (2U)
#define PRODUCT_MODBUS_DAC_FAILED_CHANNEL_NONE                (0xFFFFU)

/* Four digital inputs sampled by the application at 1 ms. */
#define PRODUCT_MODBUS_DI_BASE_ADDRESS                        (0x1500U)
#define PRODUCT_MODBUS_DI_CHANNEL_COUNT                       (4U)
#define PRODUCT_MODBUS_DI_STATE_MASK_ADDRESS                  (0x1500U)
#define PRODUCT_MODBUS_DI_REVISION_ADDRESS                    (0x1501U)
#define PRODUCT_MODBUS_DI_LAST_ADDRESS                        (0x1501U)

/* Four runtime digital outputs packed into one bit-mask register. */
#define PRODUCT_MODBUS_DO_BASE_ADDRESS                        (0x1510U)
#define PRODUCT_MODBUS_DO_CHANNEL_COUNT                       (4U)
#define PRODUCT_MODBUS_DO_STATE_MASK_ADDRESS                  (0x1510U)
#define PRODUCT_MODBUS_DO_APPLY_KEY_ADDRESS                   (0x1511U)
#define PRODUCT_MODBUS_DO_REVISION_ADDRESS                    (0x1512U)
#define PRODUCT_MODBUS_DO_PENDING_MASK_ADDRESS                (0x1513U)
#define PRODUCT_MODBUS_DO_STATUS_ADDRESS                      (0x1514U)
#define PRODUCT_MODBUS_DO_FAILED_CHANNEL_ADDRESS              (0x1515U)
#define PRODUCT_MODBUS_DO_LAST_ADDRESS                        (0x1515U)
#define PRODUCT_MODBUS_DO_APPLY_KEY_VALUE                     (0xA5A5U)
#define PRODUCT_MODBUS_DO_VALID_MASK                          (0x000FU)

#define PRODUCT_MODBUS_DO_STATUS_READY                        (0U)
#define PRODUCT_MODBUS_DO_STATUS_APPLY_FAILED                 (1U)
#define PRODUCT_MODBUS_DO_STATUS_ROLLBACK_FAILED              (2U)
#define PRODUCT_MODBUS_DO_FAILED_CHANNEL_NONE                 (0xFFFFU)

/* U5 eight-position DIP switch, sampled from FLEXCOMM8 SPI. */
#define PRODUCT_MODBUS_DIP_SWITCH_BASE_ADDRESS                (0x1520U)
#define PRODUCT_MODBUS_DIP_SWITCH_COUNT                       (8U)
#define PRODUCT_MODBUS_DIP_SWITCH_LOGICAL_MASK_ADDRESS        (0x1520U)
#define PRODUCT_MODBUS_DIP_SWITCH_RAW_VALUE_ADDRESS           (0x1521U)
#define PRODUCT_MODBUS_DIP_SWITCH_REVISION_ADDRESS            (0x1522U)
#define PRODUCT_MODBUS_DIP_SWITCH_STATUS_ADDRESS              (0x1523U)
#define PRODUCT_MODBUS_DIP_SWITCH_LAST_ADDRESS                (0x1523U)

#define PRODUCT_MODBUS_DIP_SWITCH_STATUS_READY                (0U)
#define PRODUCT_MODBUS_DIP_SWITCH_STATUS_IO_ERROR             (1U)
#define PRODUCT_MODBUS_DIP_SWITCH_STATUS_NOT_INITIALIZED      (2U)

/* External low-voltage detector monitor. All fields are read-only. */
#define PRODUCT_MODBUS_LOW_VOLTAGE_BASE_ADDRESS               (0x1530U)
#define PRODUCT_MODBUS_LOW_VOLTAGE_RAW_ADDRESS                (0x1530U)
#define PRODUCT_MODBUS_LOW_VOLTAGE_CONFIRMED_ADDRESS          (0x1531U)
#define PRODUCT_MODBUS_LOW_VOLTAGE_FAULT_ADDRESS              (0x1532U)
#define PRODUCT_MODBUS_LOW_VOLTAGE_REVISION_ADDRESS           (0x1533U)
#define PRODUCT_MODBUS_LOW_VOLTAGE_INTERRUPT_COUNT_ADDRESS    (0x1534U)
#define PRODUCT_MODBUS_LOW_VOLTAGE_STATUS_ADDRESS             (0x1536U)
#define PRODUCT_MODBUS_LOW_VOLTAGE_LAST_ADDRESS               (0x1536U)

#define PRODUCT_MODBUS_LOW_VOLTAGE_STATUS_READY               (0U)
#define PRODUCT_MODBUS_LOW_VOLTAGE_STATUS_NOT_INITIALIZED     (2U)

/* SW2 four-bit rotary switch. All fields are read-only. */
#define PRODUCT_MODBUS_ROTARY_SWITCH_BASE_ADDRESS             (0x1540U)
#define PRODUCT_MODBUS_ROTARY_SWITCH_POSITION_ADDRESS         (0x1540U)
#define PRODUCT_MODBUS_ROTARY_SWITCH_RAW_VALUE_ADDRESS        (0x1541U)
#define PRODUCT_MODBUS_ROTARY_SWITCH_REVISION_ADDRESS         (0x1542U)
#define PRODUCT_MODBUS_ROTARY_SWITCH_STATUS_ADDRESS           (0x1543U)
#define PRODUCT_MODBUS_ROTARY_SWITCH_LAST_ADDRESS             (0x1543U)

#define PRODUCT_MODBUS_ROTARY_SWITCH_STATUS_READY             (0U)
#define PRODUCT_MODBUS_ROTARY_SWITCH_STATUS_NOT_INITIALIZED   (2U)

typedef struct _product_pwm_output_config
{
    uint16_t periodMs;
    uint16_t dutyPermille;
    uint16_t updateMode;
} product_pwm_output_config_t;

typedef struct _product_low_voltage_monitor
{
    uint32_t interruptCount;
    uint16_t revision;
    uint16_t status;
    bool rawActive;
    bool confirmedActive;
    bool faultActive;
} product_low_voltage_monitor_t;

/* Reserve 0x2000..0x20FF for product version information. */
#define PRODUCT_MODBUS_VERSION_BASE_ADDRESS                   (0x2000U)
#define PRODUCT_MODBUS_FW_VERSION_ADDRESS                     (0x2000U)
#define PRODUCT_MODBUS_FW_VERSION_SUB1_ADDRESS                (0x2001U)
#define PRODUCT_MODBUS_FW_VERSION_SUB2_ADDRESS                (0x2002U)
#define PRODUCT_MODBUS_COMPATIBLE_FW_MIN_ADDRESS              (0x2003U)
#define PRODUCT_MODBUS_COMPATIBLE_FW_MAX_ADDRESS              (0x2004U)
#define PRODUCT_MODBUS_COMPATIBLE_PARAMETER_MIN_ADDRESS       (0x2005U)
#define PRODUCT_MODBUS_COMPATIBLE_PARAMETER_MAX_ADDRESS       (0x2006U)
#define PRODUCT_MODBUS_COMPATIBLE_SOFTWARE_MIN_ADDRESS        (0x2007U)
#define PRODUCT_MODBUS_COMPATIBLE_SOFTWARE_MAX_ADDRESS        (0x2008U)
#define PRODUCT_MODBUS_VERSION_LAST_USED_ADDRESS              (0x2008U)
#define PRODUCT_MODBUS_VERSION_RESERVED_LAST_ADDRESS          (0x20FFU)

#define PRODUCT_MODBUS_FACTORY_CAL_BASE_ADDRESS               (0x4700U)
#define PRODUCT_MODBUS_FACTORY_CAL_UNLOCK1_ADDRESS            (0x4700U)
#define PRODUCT_MODBUS_FACTORY_CAL_UNLOCK2_ADDRESS            (0x4701U)
#define PRODUCT_MODBUS_FACTORY_CAL_COMMAND_ADDRESS            (0x4702U)
#define PRODUCT_MODBUS_FACTORY_CAL_INPUT_ADDRESS              (0x4703U)
#define PRODUCT_MODBUS_FACTORY_CAL_PROFILE_ADDRESS            (0x4704U)
#define PRODUCT_MODBUS_FACTORY_CAL_STATE_ADDRESS              (0x4705U)
#define PRODUCT_MODBUS_FACTORY_CAL_ERROR_ADDRESS              (0x4706U)
#define PRODUCT_MODBUS_FACTORY_CAL_LIVE_UV_ADDRESS            (0x4707U)
#define PRODUCT_MODBUS_FACTORY_CAL_ZERO_UV_ADDRESS            (0x4709U)
#define PRODUCT_MODBUS_FACTORY_CAL_SPAN_UV_ADDRESS            (0x470BU)
#define PRODUCT_MODBUS_FACTORY_CAL_REVISION_ADDRESS           (0x470DU)
#define PRODUCT_MODBUS_FACTORY_CAL_LAST_ADDRESS               (0x470DU)

#define PRODUCT_FACTORY_CAL_COMMAND_SELECT                    (1U)
#define PRODUCT_FACTORY_CAL_COMMAND_CAPTURE_ZERO              (2U)
#define PRODUCT_FACTORY_CAL_COMMAND_CAPTURE_SPAN              (3U)
#define PRODUCT_FACTORY_CAL_COMMAND_APPLY                     (4U)
#define PRODUCT_FACTORY_CAL_COMMAND_ABORT                     (5U)

/* Destructive factory FRAM checkerboard bank test. */
#define PRODUCT_MODBUS_FACTORY_FRAM_BASE_ADDRESS              (0x4710U)
#define PRODUCT_MODBUS_FACTORY_FRAM_COMMAND_ADDRESS           (0x4710U)
#define PRODUCT_MODBUS_FACTORY_FRAM_STATE_ADDRESS             (0x4711U)
#define PRODUCT_MODBUS_FACTORY_FRAM_ERROR_ADDRESS             (0x4712U)
#define PRODUCT_MODBUS_FACTORY_FRAM_CURRENT_BANK_ADDRESS      (0x4713U)
#define PRODUCT_MODBUS_FACTORY_FRAM_COMPLETE_MASK_ADDRESS     (0x4714U)
#define PRODUCT_MODBUS_FACTORY_FRAM_FAILED_MASK_ADDRESS       (0x4715U)
#define PRODUCT_MODBUS_FACTORY_FRAM_PROGRESS_ADDRESS          (0x4716U)
#define PRODUCT_MODBUS_FACTORY_FRAM_CURRENT_ADDRESS           (0x4717U)
#define PRODUCT_MODBUS_FACTORY_FRAM_FAILURE_ADDRESS           (0x4719U)
#define PRODUCT_MODBUS_FACTORY_FRAM_EXPECTED_ADDRESS          (0x471BU)
#define PRODUCT_MODBUS_FACTORY_FRAM_ACTUAL_ADDRESS            (0x471CU)
#define PRODUCT_MODBUS_FACTORY_FRAM_LAST_ADDRESS              (0x471CU)

#define PRODUCT_FACTORY_FRAM_COMMAND_START                    (1U)
#define PRODUCT_FACTORY_FRAM_COMMAND_ABORT                    (2U)

/* Reserve 0x4800..0x48FF for product-wide diagnostic registers. */
#define PRODUCT_MODBUS_DIAGNOSTICS_BASE_ADDRESS               (0x4800U)
#define PRODUCT_MODBUS_DIAGNOSTICS_ACTIVE_FAULT_COUNT_ADDRESS (0x4800U)
#define PRODUCT_MODBUS_DIAGNOSTICS_FAULT_INDEX_ADDRESS        (0x4801U)
#define PRODUCT_MODBUS_DIAGNOSTICS_FAULT_CODE_ADDRESS         (0x4802U)
#define PRODUCT_MODBUS_DIAGNOSTICS_FAULT_DETAIL_ADDRESS       (0x4803U)
#define PRODUCT_MODBUS_DIAGNOSTICS_FAULT_REVISION_ADDRESS     (0x4804U)
#define PRODUCT_MODBUS_DIAGNOSTICS_EVENT_ID_ADDRESS           (0x4805U)
#define PRODUCT_MODBUS_DIAGNOSTICS_OCCURRENCE_COUNT_ADDRESS   (0x4807U)
#define PRODUCT_MODBUS_DIAGNOSTICS_RESET_CODE_ADDRESS         (0x4809U)
#define PRODUCT_MODBUS_DIAGNOSTICS_RESET_KEY_ADDRESS          (0x480AU)
#define PRODUCT_MODBUS_DIAGNOSTICS_RESET_RESULT_ADDRESS       (0x480BU)
#define PRODUCT_MODBUS_DIAGNOSTICS_RESET_LAST_CODE_ADDRESS    (0x480CU)
#define PRODUCT_MODBUS_DIAGNOSTICS_RESET_TIME_ADDRESS         (0x480DU)
#define PRODUCT_MODBUS_DIAGNOSTICS_LAST_USED_ADDRESS          (0x480EU)
#define PRODUCT_MODBUS_DIAGNOSTICS_RESERVED_LAST_ADDRESS      (0x48FFU)

/* Backward-compatible names; Clear now uses the validated Reset path. */
#define PRODUCT_MODBUS_DIAGNOSTICS_CLEAR_CODE_ADDRESS \
    PRODUCT_MODBUS_DIAGNOSTICS_RESET_CODE_ADDRESS
#define PRODUCT_MODBUS_DIAGNOSTICS_CLEAR_KEY_ADDRESS \
    PRODUCT_MODBUS_DIAGNOSTICS_RESET_KEY_ADDRESS

/* Four read-only AD7124 diagnostic blocks, 0x40 registers per device. */
#define PRODUCT_MODBUS_ADC_DIAGNOSTICS_BASE_ADDRESS           (0x4820U)
#define PRODUCT_MODBUS_ADC_DIAGNOSTICS_DEVICE_STRIDE          (0x0040U)
#define PRODUCT_MODBUS_ADC_DIAGNOSTICS_REGISTER_COUNT         (0x0040U)
#define PRODUCT_MODBUS_ADC0_DIAGNOSTICS_BASE_ADDRESS          (0x4820U)
#define PRODUCT_MODBUS_ADC1_DIAGNOSTICS_BASE_ADDRESS          (0x4860U)
#define PRODUCT_MODBUS_ADC2_DIAGNOSTICS_BASE_ADDRESS          (0x48A0U)
#define PRODUCT_MODBUS_ADC3_DIAGNOSTICS_BASE_ADDRESS          (0x48E0U)
#define PRODUCT_MODBUS_ADC_DIAGNOSTICS_LAST_ADDRESS           (0x491FU)

/* Read-only configure tracing; CH0..3 bases 4A00/4A40/4A80/4AC0.
 * Offsets: 0 source, 1 stage, 2 HAL result, 3 discard pending;
 * 4..11 startup/event/recovery/unknown uint32 counts; 12..13 event ID,
 * 14 revision, 15 apply result, 16..17 event configure attempts,
 * 18..19 apply failures, 20..21 ACK attempts, 22..23 first discards,
 * 24..25 fault discards, 26 online, 27 consecutive driver errors,
 * 28..29 driver errors, 30..31 configure successes, 32..33 ACK failures,
 * 34 event stage; 35 read stage, 36 HAL read result, 37 retained failure stage,
 * 38 failure HAL result, 39 failure gain, 40..41 failure reference uV,
 * 42 failure channel, 43 channel count, 44 setup count, 45..46 failure raw,
 * 47..48 read failures, 49 config valid, 50..51 failure ADI status (int32).
 * Remaining offsets reserved (zero). uint32 is high/low.
 */
#define PRODUCT_MODBUS_ADC_TRACE_BASE_ADDRESS                 (0x4A00U)
#define PRODUCT_MODBUS_ADC_TRACE_DEVICE_STRIDE                (0x0040U)
#define PRODUCT_MODBUS_ADC_TRACE_LAST_ADDRESS                 (0x4AFFU)

/* SystemRoutine live summary. All values are read-only. */
#define PRODUCT_MODBUS_SYSTEM_STATUS_BASE_ADDRESS             (0x4810U)
#define PRODUCT_MODBUS_WARNING_MASK_ADDRESS                   (0x4810U)
#define PRODUCT_MODBUS_SAFETY_ACTIVE_MASK_ADDRESS             (0x4812U)
#define PRODUCT_MODBUS_SAFETY_LATCHED_MASK_ADDRESS            (0x4814U)
#define PRODUCT_MODBUS_SAFETY_TRIP_MASK_ADDRESS               (0x4816U)
#define PRODUCT_MODBUS_SYSTEM_EVENT_COUNT_ADDRESS             (0x4818U)
#define PRODUCT_MODBUS_SNAPSHOT_COUNT_ADDRESS                 (0x4819U)
#define PRODUCT_MODBUS_LATEST_EVENT_SEQUENCE_ADDRESS          (0x481AU)
#define PRODUCT_MODBUS_LATEST_SNAPSHOT_SEQUENCE_ADDRESS       (0x481CU)
#define PRODUCT_MODBUS_SAFETY_STATE_ADDRESS                   (0x481EU)
#define PRODUCT_MODBUS_SAFETY_INHIBITED_ADDRESS               (0x481FU)
#define PRODUCT_MODBUS_SYSTEM_STATUS_LAST_ADDRESS             (0x481FU)

/* Runtime Event browser; selection index zero is the oldest retained event. */
#define PRODUCT_MODBUS_SYSTEM_EVENT_BASE_ADDRESS              (0x4920U)
#define PRODUCT_MODBUS_SYSTEM_EVENT_INDEX_ADDRESS             (0x4920U)
#define PRODUCT_MODBUS_SYSTEM_EVENT_SEQUENCE_ADDRESS          (0x4921U)
#define PRODUCT_MODBUS_SYSTEM_EVENT_TIMESTAMP_ADDRESS         (0x4923U)
#define PRODUCT_MODBUS_SYSTEM_EVENT_DOMAIN_ADDRESS            (0x4925U)
#define PRODUCT_MODBUS_SYSTEM_EVENT_STATE_ADDRESS             (0x4926U)
#define PRODUCT_MODBUS_SYSTEM_EVENT_CODE_ADDRESS              (0x4927U)
#define PRODUCT_MODBUS_SYSTEM_EVENT_DETAIL_ADDRESS            (0x4928U)
#define PRODUCT_MODBUS_SYSTEM_EVENT_REVISION_ADDRESS          (0x4929U)
#define PRODUCT_MODBUS_SYSTEM_EVENT_CORRELATION_ADDRESS       (0x492AU)
#define PRODUCT_MODBUS_SYSTEM_EVENT_LAST_ADDRESS              (0x492BU)

/* Snapshot browser; selection index zero is the oldest retained snapshot. */
#define PRODUCT_MODBUS_SNAPSHOT_BASE_ADDRESS                  (0x4940U)
#define PRODUCT_MODBUS_SNAPSHOT_INDEX_ADDRESS                 (0x4940U)
#define PRODUCT_MODBUS_SNAPSHOT_SEQUENCE_ADDRESS              (0x4941U)
#define PRODUCT_MODBUS_SNAPSHOT_TIMESTAMP_ADDRESS             (0x4943U)
#define PRODUCT_MODBUS_SNAPSHOT_SOURCE_ADDRESS                (0x4945U)
#define PRODUCT_MODBUS_SNAPSHOT_CODE_ADDRESS                  (0x4946U)
#define PRODUCT_MODBUS_SNAPSHOT_DETAIL_ADDRESS                (0x4947U)
#define PRODUCT_MODBUS_SNAPSHOT_REVISION_ADDRESS              (0x4948U)
#define PRODUCT_MODBUS_SNAPSHOT_EVENT_ID_ADDRESS              (0x4949U)
#define PRODUCT_MODBUS_SNAPSHOT_VALUES_ADDRESS                (0x494BU)
#define PRODUCT_MODBUS_SNAPSHOT_LAST_ADDRESS                  (0x495AU)

#define PRODUCT_DIAGNOSTICS_CLEAR_KEY_VALUE                   (0xC1EAU)
#define PRODUCT_DIAGNOSTICS_RESET_RESULT_READY                (0U)
#define PRODUCT_DIAGNOSTICS_RESET_RESULT_SUCCESS              (1U)
#define PRODUCT_DIAGNOSTICS_RESET_RESULT_NOT_ACTIVE           (2U)
#define PRODUCT_DIAGNOSTICS_RESET_RESULT_CONDITION_ACTIVE     (3U)
#define PRODUCT_DIAGNOSTICS_RESET_RESULT_INVALID_CODE         (4U)
#define PRODUCT_DIAGNOSTICS_RESET_RESULT_FAILED               (5U)
#define PRODUCT_DIAGNOSTICS_RESET_RESULT_INVALID_KEY          (6U)

typedef struct _product_temperature_input_monitor
{
    float unfilteredProcessValue;
    uint16_t inputError;
    float filteredProcessValue;
} product_temperature_input_monitor_t;

typedef struct _product_temperature_input_config
{
    float filterTimeConstantSeconds;
    uint16_t sensorType;
} product_temperature_input_config_t;

/* Build the product register callback table used by ModbusSlave. */
void ProductModbusRegisterAdapter_GetInterface(
    ModbusSlaveRegisterInterface_t *interface);

/* Supplies the monotonic application time used by diagnostic commands. */
void ProductModbusRegisterAdapter_SetSystemTimestamp(uint32_t timestamp_ms);

/* Update read-only monitor registers from the L3 sensor service. */
bool ProductModbusRegisterAdapter_SetTemperatureInputMonitor(
    const product_temperature_input_monitor_t *monitor);
bool ProductModbusRegisterAdapter_SetTemperatureInputMonitorForChannel(
    uint8_t channel,
    const product_temperature_input_monitor_t *monitor);

/* Read the Active configuration currently used by the product. */
void ProductModbusRegisterAdapter_GetTemperatureInputConfig(
    product_temperature_input_config_t *config);
bool ProductModbusRegisterAdapter_GetTemperatureInputConfigForChannel(
    uint8_t channel,
    product_temperature_input_config_t *config);

/* Revision increases after each effective Apply that publishes an event. */
uint16_t ProductModbusRegisterAdapter_GetTemperatureInputConfigurationRevision(
    void);
uint16_t
ProductModbusRegisterAdapter_GetTemperatureInputConfigurationRevisionForChannel(
    uint8_t channel);

/* Restore a CRC-verified configuration during startup before Modbus runs. */
bool ProductModbusRegisterAdapter_RestoreTemperatureInputConfig(
    const product_temperature_input_config_t *config,
    uint16_t configuration_revision);
bool ProductModbusRegisterAdapter_RestoreTemperatureInputConfigForChannel(
    uint8_t channel,
    const product_temperature_input_config_t *config,
    uint16_t configuration_revision);

/* Query and read values staged by FC06/FC10 but not applied yet. */
bool ProductModbusRegisterAdapter_HasPendingTemperatureInputConfig(void);
bool ProductModbusRegisterAdapter_HasPendingTemperatureInputConfigForChannel(
    uint8_t channel);
void ProductModbusRegisterAdapter_GetPendingTemperatureInputConfig(
    product_temperature_input_config_t *config);
bool ProductModbusRegisterAdapter_GetPendingTemperatureInputConfigForChannel(
    uint8_t channel,
    product_temperature_input_config_t *config);

/* Cancel all staged writes and restore Pending from Active. */
void ProductModbusRegisterAdapter_DiscardPendingTemperatureInputConfig(void);
void ProductModbusRegisterAdapter_DiscardPendingTemperatureInputConfigForChannel(
    uint8_t channel);

/* PWM Active/Pending inspection used by diagnostics and later NVM support. */
bool ProductModbusRegisterAdapter_GetPwmConfig(
    uint8_t channel,
    product_pwm_output_config_t *config);
bool ProductModbusRegisterAdapter_GetPendingPwmConfig(
    uint8_t channel,
    product_pwm_output_config_t *config);
uint16_t ProductModbusRegisterAdapter_GetPwmConfigurationRevision(void);
uint16_t ProductModbusRegisterAdapter_GetPwmPendingMask(void);
void ProductModbusRegisterAdapter_DiscardPendingPwmConfig(void);

/* DAC Active/Pending inspection. Output commands are intentionally not NVM. */
bool ProductModbusRegisterAdapter_GetDacCode(
    uint8_t channel,
    uint16_t *code);
bool ProductModbusRegisterAdapter_GetPendingDacCode(
    uint8_t channel,
    uint16_t *code);
uint16_t ProductModbusRegisterAdapter_GetDacConfigurationRevision(void);
uint16_t ProductModbusRegisterAdapter_GetDacPendingMask(void);
uint16_t ProductModbusRegisterAdapter_GetDacStatus(void);
uint16_t ProductModbusRegisterAdapter_GetDacFailedChannel(void);
void ProductModbusRegisterAdapter_DiscardPendingDacCodes(void);

void ProductModbusRegisterAdapter_SetLowVoltageMonitor(
    const product_low_voltage_monitor_t *monitor);

#ifdef __cplusplus
}
#endif

#endif /* PRODUCT_MODBUS_REGISTER_ADAPTER_H_ */
