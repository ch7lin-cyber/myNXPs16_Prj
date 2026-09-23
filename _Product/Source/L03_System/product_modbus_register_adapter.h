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
#define PRODUCT_MODBUS_TEMPERATURE_INPUT_INSTANCE_STRIDE     (0x0010U)

#define PRODUCT_MODBUS_UNFILTERED_PV_ADDRESS                 (0x1000U)
#define PRODUCT_MODBUS_INPUT_ERROR_ADDRESS                   (0x1002U)
#define PRODUCT_MODBUS_FILTER_TIME_CONSTANT_ADDRESS          (0x1003U)
#define PRODUCT_MODBUS_FILTERED_PV_ADDRESS                   (0x1005U)
#define PRODUCT_MODBUS_SENSOR_TYPE_ADDRESS                   (0x1007U)
#define PRODUCT_MODBUS_TC_LINEARIZATION_ADDRESS              (0x1008U)
#define PRODUCT_MODBUS_APPLY_KEY_ADDRESS                     (0x1009U)
#define PRODUCT_MODBUS_APPLY_KEY_VALUE                       (0xA5A5U)
#define PRODUCT_MODBUS_TEMPERATURE_INPUT_LAST_ADDRESS        (0x1009U)

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

/* Reserve 0x4800..0x48FF for product-wide diagnostic registers. */
#define PRODUCT_MODBUS_DIAGNOSTICS_BASE_ADDRESS               (0x4800U)
#define PRODUCT_MODBUS_DIAGNOSTICS_ACTIVE_FAULT_COUNT_ADDRESS (0x4800U)
#define PRODUCT_MODBUS_DIAGNOSTICS_FAULT_INDEX_ADDRESS        (0x4801U)
#define PRODUCT_MODBUS_DIAGNOSTICS_FAULT_CODE_ADDRESS         (0x4802U)
#define PRODUCT_MODBUS_DIAGNOSTICS_FAULT_DETAIL_ADDRESS       (0x4803U)
#define PRODUCT_MODBUS_DIAGNOSTICS_FAULT_REVISION_ADDRESS     (0x4804U)
#define PRODUCT_MODBUS_DIAGNOSTICS_EVENT_ID_ADDRESS           (0x4805U)
#define PRODUCT_MODBUS_DIAGNOSTICS_OCCURRENCE_COUNT_ADDRESS   (0x4807U)
#define PRODUCT_MODBUS_DIAGNOSTICS_CLEAR_CODE_ADDRESS         (0x4809U)
#define PRODUCT_MODBUS_DIAGNOSTICS_CLEAR_KEY_ADDRESS          (0x480AU)
#define PRODUCT_MODBUS_DIAGNOSTICS_LAST_USED_ADDRESS          (0x480AU)
#define PRODUCT_MODBUS_DIAGNOSTICS_RESERVED_LAST_ADDRESS      (0x48FFU)

#define PRODUCT_DIAGNOSTICS_CLEAR_KEY_VALUE                   (0xC1EAU)

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
    uint16_t tcLinearization;
} product_temperature_input_config_t;

/* Build the product register callback table used by ModbusSlave. */
void ProductModbusRegisterAdapter_GetInterface(
    ModbusSlaveRegisterInterface_t *interface);

/* Update read-only monitor registers from the L3 sensor service. */
bool ProductModbusRegisterAdapter_SetTemperatureInputMonitor(
    const product_temperature_input_monitor_t *monitor);

/* Read the Active configuration currently used by the product. */
void ProductModbusRegisterAdapter_GetTemperatureInputConfig(
    product_temperature_input_config_t *config);

/* Revision increases after each effective Apply that publishes an event. */
uint16_t ProductModbusRegisterAdapter_GetTemperatureInputConfigurationRevision(
    void);

/* Restore a CRC-verified configuration during startup before Modbus runs. */
bool ProductModbusRegisterAdapter_RestoreTemperatureInputConfig(
    const product_temperature_input_config_t *config,
    uint16_t configuration_revision);

/* Query and read values staged by FC06/FC10 but not applied yet. */
bool ProductModbusRegisterAdapter_HasPendingTemperatureInputConfig(void);
void ProductModbusRegisterAdapter_GetPendingTemperatureInputConfig(
    product_temperature_input_config_t *config);

/* Cancel all staged writes and restore Pending from Active. */
void ProductModbusRegisterAdapter_DiscardPendingTemperatureInputConfig(void);

#ifdef __cplusplus
}
#endif

#endif /* PRODUCT_MODBUS_REGISTER_ADAPTER_H_ */
