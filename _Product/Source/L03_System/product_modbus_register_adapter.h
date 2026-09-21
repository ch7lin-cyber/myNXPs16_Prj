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
#define PRODUCT_MODBUS_TEMPERATURE_INPUT_LAST_ADDRESS        (0x1008U)

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
