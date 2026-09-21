/*
 * Copyright 2026
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "product_modbus_register_adapter.h"

#include <stddef.h>
#include <string.h>

#define PRODUCT_TEMPERATURE_VALUE_MIN              (-99999.0F)
#define PRODUCT_TEMPERATURE_VALUE_MAX              (99999.0F)
#define PRODUCT_FILTER_TIME_CONSTANT_MIN_SECONDS   (0.0F)
#define PRODUCT_FILTER_TIME_CONSTANT_MAX_SECONDS   (60.0F)

#define PRODUCT_INPUT_ERROR_NONE                   (61U)

#define PRODUCT_SENSOR_TYPE_OFF                    (62U)
#define PRODUCT_SENSOR_TYPE_THERMOCOUPLE           (95U)
#define PRODUCT_SENSOR_TYPE_RTD_100_OHM            (113U)
#define PRODUCT_SENSOR_TYPE_RTD_1000_OHM           (114U)

#define PRODUCT_TC_LINEARIZATION_B                 (11U)
#define PRODUCT_TC_LINEARIZATION_C                 (15U)
#define PRODUCT_TC_LINEARIZATION_D                 (23U)
#define PRODUCT_TC_LINEARIZATION_E                 (26U)
#define PRODUCT_TC_LINEARIZATION_J                 (46U)
#define PRODUCT_TC_LINEARIZATION_K                 (48U)
#define PRODUCT_TC_LINEARIZATION_N                 (58U)
#define PRODUCT_TC_LINEARIZATION_R                 (80U)
#define PRODUCT_TC_LINEARIZATION_S                 (84U)
#define PRODUCT_TC_LINEARIZATION_T                 (93U)

typedef struct _product_modbus_register_context
{
    product_temperature_input_monitor_t monitor;
    product_temperature_input_config_t config;
} product_modbus_register_context_t;

static product_modbus_register_context_t s_registerContext =
{
    {0.0F, PRODUCT_INPUT_ERROR_NONE, 0.0F},
    {0.5F, PRODUCT_SENSOR_TYPE_OFF, PRODUCT_TC_LINEARIZATION_J}
};

static void FloatToRegisters(float value, uint16_t *highWord, uint16_t *lowWord)
{
    uint32_t bits;

    (void)memcpy(&bits, &value, sizeof(bits));
    *highWord = (uint16_t)(bits >> 16U);
    *lowWord = (uint16_t)(bits & 0xFFFFU);
}

static float RegistersToFloat(uint16_t highWord, uint16_t lowWord)
{
    uint32_t bits = ((uint32_t)highWord << 16U) | (uint32_t)lowWord;
    float value;

    (void)memcpy(&value, &bits, sizeof(value));
    return value;
}

static bool IsTemperatureValueValid(float value)
{
    /* NaN fails both ordered comparisons; infinities exceed the limits. */
    return (value >= PRODUCT_TEMPERATURE_VALUE_MIN) &&
           (value <= PRODUCT_TEMPERATURE_VALUE_MAX);
}

static bool IsFilterTimeConstantValid(float value)
{
    /* Keep this freestanding: do not introduce a libm __isfinitef symbol. */
    return (value >= PRODUCT_FILTER_TIME_CONSTANT_MIN_SECONDS) &&
           (value <= PRODUCT_FILTER_TIME_CONSTANT_MAX_SECONDS);
}

static bool IsSensorTypeValid(uint16_t value)
{
    return (value == PRODUCT_SENSOR_TYPE_OFF) ||
           (value == PRODUCT_SENSOR_TYPE_THERMOCOUPLE) ||
           (value == PRODUCT_SENSOR_TYPE_RTD_100_OHM) ||
           (value == PRODUCT_SENSOR_TYPE_RTD_1000_OHM);
}

static bool IsTcLinearizationValid(uint16_t value)
{
    return (value == PRODUCT_TC_LINEARIZATION_B) ||
           (value == PRODUCT_TC_LINEARIZATION_C) ||
           (value == PRODUCT_TC_LINEARIZATION_D) ||
           (value == PRODUCT_TC_LINEARIZATION_E) ||
           (value == PRODUCT_TC_LINEARIZATION_J) ||
           (value == PRODUCT_TC_LINEARIZATION_K) ||
           (value == PRODUCT_TC_LINEARIZATION_N) ||
           (value == PRODUCT_TC_LINEARIZATION_R) ||
           (value == PRODUCT_TC_LINEARIZATION_S) ||
           (value == PRODUCT_TC_LINEARIZATION_T);
}

static bool IsRegisterRangeValid(uint16_t startingAddress, uint16_t quantity)
{
    uint32_t endingAddress;

    if (quantity == 0U)
    {
        return false;
    }

    endingAddress = (uint32_t)startingAddress + (uint32_t)quantity - 1UL;
    return (startingAddress >= PRODUCT_MODBUS_TEMPERATURE_INPUT_BASE_ADDRESS) &&
           (endingAddress <= PRODUCT_MODBUS_TEMPERATURE_INPUT_LAST_ADDRESS);
}

static void BuildRegisterImage(
    const product_modbus_register_context_t *registerContext,
    uint16_t *registers)
{
    FloatToRegisters(registerContext->monitor.unfilteredProcessValue,
                     &registers[0], &registers[1]);
    registers[2] = registerContext->monitor.inputError;
    FloatToRegisters(registerContext->config.filterTimeConstantSeconds,
                     &registers[3], &registers[4]);
    FloatToRegisters(registerContext->monitor.filteredProcessValue,
                     &registers[5], &registers[6]);
    registers[7] = registerContext->config.sensorType;
    registers[8] = registerContext->config.tcLinearization;
}

static ModbusExceptionCode_t ReadRegisters(
    void *context,
    uint16_t starting_address,
    uint16_t quantity,
    uint16_t *values)
{
    product_modbus_register_context_t *registerContext =
        (product_modbus_register_context_t *)context;
    uint16_t registerImage[9];
    uint16_t sourceOffset;

    if ((registerContext == NULL) || (values == NULL))
    {
        return MODBUS_EXCEPTION_SERVER_DEVICE_FAILURE;
    }

    if (!IsRegisterRangeValid(starting_address, quantity))
    {
        return MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS;
    }

    BuildRegisterImage(registerContext, registerImage);
    sourceOffset = (uint16_t)(starting_address -
                             PRODUCT_MODBUS_TEMPERATURE_INPUT_BASE_ADDRESS);
    (void)memcpy(values, &registerImage[sourceOffset],
                 (size_t)quantity * sizeof(values[0]));
    return MODBUS_EXCEPTION_NONE;
}

static ModbusExceptionCode_t WriteSingleRegister(
    void *context,
    uint16_t address,
    uint16_t value)
{
    product_modbus_register_context_t *registerContext =
        (product_modbus_register_context_t *)context;

    if (registerContext == NULL)
    {
        return MODBUS_EXCEPTION_SERVER_DEVICE_FAILURE;
    }

    if (address == PRODUCT_MODBUS_SENSOR_TYPE_ADDRESS)
    {
        if (!IsSensorTypeValid(value))
        {
            return MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE;
        }
        registerContext->config.sensorType = value;
        return MODBUS_EXCEPTION_NONE;
    }

    if (address == PRODUCT_MODBUS_TC_LINEARIZATION_ADDRESS)
    {
        if (!IsTcLinearizationValid(value))
        {
            return MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE;
        }
        registerContext->config.tcLinearization = value;
        return MODBUS_EXCEPTION_NONE;
    }

    return MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS;
}

static ModbusExceptionCode_t WriteMultipleRegisters(
    void *context,
    uint16_t starting_address,
    const uint16_t *values,
    uint16_t quantity)
{
    product_modbus_register_context_t *registerContext =
        (product_modbus_register_context_t *)context;
    product_temperature_input_config_t pendingConfig;
    float filterTimeConstant;

    if ((registerContext == NULL) || (values == NULL))
    {
        return MODBUS_EXCEPTION_SERVER_DEVICE_FAILURE;
    }

    pendingConfig = registerContext->config;

    if ((starting_address == PRODUCT_MODBUS_FILTER_TIME_CONSTANT_ADDRESS) &&
        (quantity == 2U))
    {
        filterTimeConstant = RegistersToFloat(values[0], values[1]);
        if (!IsFilterTimeConstantValid(filterTimeConstant))
        {
            return MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE;
        }
        pendingConfig.filterTimeConstantSeconds = filterTimeConstant;
    }
    else if ((starting_address == PRODUCT_MODBUS_SENSOR_TYPE_ADDRESS) &&
             (quantity == 1U))
    {
        if (!IsSensorTypeValid(values[0]))
        {
            return MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE;
        }
        pendingConfig.sensorType = values[0];
    }
    else if ((starting_address == PRODUCT_MODBUS_TC_LINEARIZATION_ADDRESS) &&
             (quantity == 1U))
    {
        if (!IsTcLinearizationValid(values[0]))
        {
            return MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE;
        }
        pendingConfig.tcLinearization = values[0];
    }
    else if ((starting_address == PRODUCT_MODBUS_SENSOR_TYPE_ADDRESS) &&
             (quantity == 2U))
    {
        if (!IsSensorTypeValid(values[0]) ||
            !IsTcLinearizationValid(values[1]))
        {
            return MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE;
        }
        pendingConfig.sensorType = values[0];
        pendingConfig.tcLinearization = values[1];
    }
    else
    {
        return MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS;
    }

    registerContext->config = pendingConfig;
    return MODBUS_EXCEPTION_NONE;
}

void ProductModbusRegisterAdapter_GetInterface(
    ModbusSlaveRegisterInterface_t *interface)
{
    if (interface != NULL)
    {
        interface->read_holding_registers = ReadRegisters;
        interface->read_input_registers = ReadRegisters;
        interface->write_single_register = WriteSingleRegister;
        interface->write_multiple_registers = WriteMultipleRegisters;
        interface->context = &s_registerContext;
    }
}

bool ProductModbusRegisterAdapter_SetTemperatureInputMonitor(
    const product_temperature_input_monitor_t *monitor)
{
    if ((monitor == NULL) ||
        !IsTemperatureValueValid(monitor->unfilteredProcessValue) ||
        !IsTemperatureValueValid(monitor->filteredProcessValue))
    {
        return false;
    }

    s_registerContext.monitor = *monitor;
    return true;
}

void ProductModbusRegisterAdapter_GetTemperatureInputConfig(
    product_temperature_input_config_t *config)
{
    if (config != NULL)
    {
        *config = s_registerContext.config;
    }
}
