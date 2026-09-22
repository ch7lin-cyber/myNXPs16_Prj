/*
 * Copyright 2026
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "product_modbus_register_adapter.h"
#include "product_temperature_input_types.h"

#include "EventService.h"
#include "FactoryCalibrationService.h"
#include "ModbusRegisterAdapter.h"

#include <stddef.h>
#include <string.h>

#define PRODUCT_TEMPERATURE_VALUE_MIN              (-99999.0F)
#define PRODUCT_TEMPERATURE_VALUE_MAX              (99999.0F)
#define PRODUCT_FILTER_TIME_CONSTANT_MIN_SECONDS   (0.0F)
#define PRODUCT_FILTER_TIME_CONSTANT_MAX_SECONDS   (60.0F)

#define PRODUCT_INPUT_ERROR_NONE                   (61U)

#define PRODUCT_TEMPERATURE_INPUT_EVENT_CHANNEL    (0U)

typedef struct _product_modbus_register_context
{
    product_temperature_input_monitor_t monitor;
    product_temperature_input_config_t activeConfig;
    product_temperature_input_config_t pendingConfig;
    uint16_t configurationRevision;
    bool pendingDirty;
} product_modbus_register_context_t;

static product_modbus_register_context_t s_registerContext =
{
    {0.0F, PRODUCT_INPUT_ERROR_NONE, 0.0F},
    {0.5F, PRODUCT_SENSOR_TYPE_OFF, PRODUCT_TC_LINEARIZATION_J},
    {0.5F, PRODUCT_SENSOR_TYPE_OFF, PRODUCT_TC_LINEARIZATION_J},
    0U,
    false
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
           (value == PRODUCT_SENSOR_TYPE_RTD_1000_OHM) ||
           (value == PRODUCT_SENSOR_TYPE_RTD_JPT100) ||
           (value == PRODUCT_SENSOR_TYPE_RTD_NI120) ||
           (value == PRODUCT_SENSOR_TYPE_RTD_CU50) ||
           (value == PRODUCT_SENSOR_TYPE_VOLTAGE_0_5V) ||
           (value == PRODUCT_SENSOR_TYPE_VOLTAGE_0_10V) ||
           (value == PRODUCT_SENSOR_TYPE_VOLTAGE_0_50MV) ||
           (value == PRODUCT_SENSOR_TYPE_CURRENT_0_20MA) ||
           (value == PRODUCT_SENSOR_TYPE_CURRENT_4_20MA);
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
           (value == PRODUCT_TC_LINEARIZATION_T) ||
           (value == PRODUCT_TC_LINEARIZATION_L) ||
           (value == PRODUCT_TC_LINEARIZATION_U) ||
           (value == PRODUCT_TC_LINEARIZATION_TXK);
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

static bool IsCommonSerialLineField(ModbusSerialRegisterOffset_t field)
{
    return ((field == MODBUS_SERIAL_REGISTER_BAUD_CODE) ||
            (field == MODBUS_SERIAL_REGISTER_DATA_BITS) ||
            (field == MODBUS_SERIAL_REGISTER_PARITY) ||
            (field == MODBUS_SERIAL_REGISTER_STOP_BITS) ||
            (field == MODBUS_SERIAL_REGISTER_PROTOCOL));
}

static ModbusExceptionCode_t WriteProductSerialRegister(
    uint16_t address,
    uint16_t value)
{
    ModbusSerialRegisterInfo_t information;
    uint16_t peer_address;
    ModbusExceptionCode_t result;

    if (!ModbusRegisterAdapter_ResolveSerialAddress(address, &information))
    {
        return MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS;
    }
    if ((information.field == MODBUS_SERIAL_REGISTER_ROLE) ||
        ((information.port == 1U) &&
         (information.field == MODBUS_SERIAL_REGISTER_UNIT_ID)) ||
        (information.access == MODBUS_REGISTER_ACCESS_READ_ONLY))
    {
        return MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS;
    }

    if ((information.field == MODBUS_SERIAL_REGISTER_PROTOCOL) &&
        (value != (uint16_t)SERIAL_PROTOCOL_MODBUS_RTU) &&
        (value != (uint16_t)SERIAL_PROTOCOL_MODBUS_ASCII))
    {
        return MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE;
    }

    if (information.field == MODBUS_SERIAL_REGISTER_APPLY)
    {
        result = ModbusRegisterAdapter_WriteSingleRegister(
            NULL, 0x1208U, value);
        if (result != MODBUS_EXCEPTION_NONE)
        {
            return result;
        }
        return ModbusRegisterAdapter_WriteSingleRegister(
            NULL, 0x1218U, value);
    }

    result = ModbusRegisterAdapter_WriteSingleRegister(
        NULL, address, value);
    if ((result != MODBUS_EXCEPTION_NONE) ||
        !IsCommonSerialLineField(information.field))
    {
        return result;
    }

    if (!ModbusRegisterAdapter_GetSerialAddress(
            (information.port == 0U) ? 1U : 0U,
            information.field,
            &peer_address))
    {
        return MODBUS_EXCEPTION_SERVER_DEVICE_FAILURE;
    }
    return ModbusRegisterAdapter_WriteSingleRegister(
        NULL, peer_address, value);
}

static bool IsFactoryCalibrationRangeValid(uint16_t startingAddress,
                                           uint16_t quantity)
{
    uint32_t endingAddress;
    if (quantity == 0U)
    {
        return false;
    }
    endingAddress = (uint32_t)startingAddress + quantity - 1UL;
    return (startingAddress >= PRODUCT_MODBUS_FACTORY_CAL_BASE_ADDRESS) &&
           (endingAddress <= PRODUCT_MODBUS_FACTORY_CAL_LAST_ADDRESS);
}

static void Int32ToRegisters(int32_t value, uint16_t *high, uint16_t *low)
{
    uint32_t bits = (uint32_t)value;
    *high = (uint16_t)(bits >> 16U);
    *low = (uint16_t)bits;
}

static void BuildFactoryCalibrationImage(uint16_t *registers)
{
    FactoryCalibrationSnapshot_t snapshot;
    (void)memset(registers, 0, 14U * sizeof(registers[0]));
    FactoryCalibrationService_GetSnapshot(&snapshot);
    registers[3] = snapshot.input;
    registers[4] = (uint16_t)snapshot.profile;
    registers[5] = (uint16_t)snapshot.state;
    registers[6] = (uint16_t)snapshot.error;
    Int32ToRegisters(snapshot.live_uv, &registers[7], &registers[8]);
    Int32ToRegisters(snapshot.pending_zero_uv,
                     &registers[9], &registers[10]);
    Int32ToRegisters(snapshot.pending_span_uv,
                     &registers[11], &registers[12]);
    registers[13] = snapshot.revision;
}

static void BuildRegisterImage(
    const product_modbus_register_context_t *registerContext,
    uint16_t *registers)
{
    FloatToRegisters(registerContext->monitor.unfilteredProcessValue,
                     &registers[0], &registers[1]);
    registers[2] = registerContext->monitor.inputError;
    FloatToRegisters(registerContext->activeConfig.filterTimeConstantSeconds,
                     &registers[3], &registers[4]);
    FloatToRegisters(registerContext->monitor.filteredProcessValue,
                     &registers[5], &registers[6]);
    registers[7] = registerContext->activeConfig.sensorType;
    registers[8] = registerContext->activeConfig.tcLinearization;
    /* The command key is never echoed back through the register image. */
    registers[9] = 0U;
}

static ModbusExceptionCode_t ReadRegisters(
    void *context,
    uint16_t starting_address,
    uint16_t quantity,
    uint16_t *values)
{
    product_modbus_register_context_t *registerContext =
        (product_modbus_register_context_t *)context;
    uint16_t registerImage[10];
    uint16_t factoryImage[14];
    uint16_t sourceOffset;
    ModbusSerialRegisterInfo_t serial_information;
    uint16_t index;

    if ((registerContext == NULL) || (values == NULL))
    {
        return MODBUS_EXCEPTION_SERVER_DEVICE_FAILURE;
    }

    if (ModbusRegisterAdapter_ResolveSerialAddress(
            starting_address, &serial_information))
    {
        for (index = 0U; index < quantity; index++)
        {
            ModbusExceptionCode_t result =
                ModbusRegisterAdapter_ReadSerialRegister(
                    (uint16_t)(starting_address + index), &values[index]);
            if (result != MODBUS_EXCEPTION_NONE)
            {
                return result;
            }
        }
        return MODBUS_EXCEPTION_NONE;
    }

    if (IsFactoryCalibrationRangeValid(starting_address, quantity))
    {
        BuildFactoryCalibrationImage(factoryImage);
        sourceOffset = (uint16_t)(starting_address -
                                 PRODUCT_MODBUS_FACTORY_CAL_BASE_ADDRESS);
        (void)memcpy(values, &factoryImage[sourceOffset],
                     (size_t)quantity * sizeof(values[0]));
        return MODBUS_EXCEPTION_NONE;
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

static ModbusExceptionCode_t ExecuteFactoryCalibrationCommand(uint16_t command)
{
    FactoryCalibrationSnapshot_t snapshot;
    bool success;

    FactoryCalibrationService_GetSnapshot(&snapshot);
    switch (command)
    {
        case PRODUCT_FACTORY_CAL_COMMAND_SELECT:
            success = FactoryCalibrationService_Select(snapshot.input,
                                                        snapshot.profile);
            break;
        case PRODUCT_FACTORY_CAL_COMMAND_CAPTURE_ZERO:
            success = FactoryCalibrationService_CaptureZero();
            break;
        case PRODUCT_FACTORY_CAL_COMMAND_CAPTURE_SPAN:
            success = FactoryCalibrationService_CaptureSpan();
            break;
        case PRODUCT_FACTORY_CAL_COMMAND_APPLY:
            success = FactoryCalibrationService_Apply();
            break;
        case PRODUCT_FACTORY_CAL_COMMAND_ABORT:
            FactoryCalibrationService_Abort();
            success = true;
            break;
        default:
            return MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE;
    }
    return success ? MODBUS_EXCEPTION_NONE :
                     MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE;
}

static ModbusExceptionCode_t ApplyPendingConfiguration(
    product_modbus_register_context_t *registerContext,
    uint16_t applyKey)
{
    product_temperature_input_config_t oldConfig;
    EventTemperatureInputConfiguration_t oldEventConfig;
    EventTemperatureInputConfiguration_t newEventConfig;
    uint32_t changedMask = 0U;
    uint16_t oldRevision;
    uint16_t newRevision;

    if (applyKey != PRODUCT_MODBUS_APPLY_KEY_VALUE)
    {
        return MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE;
    }

    if (!registerContext->pendingDirty)
    {
        return MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE;
    }

    if (registerContext->activeConfig.filterTimeConstantSeconds !=
        registerContext->pendingConfig.filterTimeConstantSeconds)
    {
        changedMask |=
            EVENT_TEMPERATURE_INPUT_CHANGE_FILTER_TIME_CONSTANT;
    }
    if (registerContext->activeConfig.sensorType !=
        registerContext->pendingConfig.sensorType)
    {
        changedMask |= EVENT_TEMPERATURE_INPUT_CHANGE_SENSOR_TYPE;
    }
    if (registerContext->activeConfig.tcLinearization !=
        registerContext->pendingConfig.tcLinearization)
    {
        changedMask |= EVENT_TEMPERATURE_INPUT_CHANGE_TC_LINEARIZATION;
    }

    if (changedMask == 0U)
    {
        registerContext->pendingDirty = false;
        return MODBUS_EXCEPTION_NONE;
    }

    if (EventService_IsTemperatureInputConfigurationChangedPending(
            PRODUCT_TEMPERATURE_INPUT_EVENT_CHANNEL))
    {
        return MODBUS_EXCEPTION_SERVER_DEVICE_FAILURE;
    }

    oldConfig = registerContext->activeConfig;
    oldRevision = registerContext->configurationRevision;
    oldEventConfig.filter_time_constant_seconds =
        oldConfig.filterTimeConstantSeconds;
    oldEventConfig.sensor_type = oldConfig.sensorType;
    oldEventConfig.tc_linearization = oldConfig.tcLinearization;
    newEventConfig.filter_time_constant_seconds =
        registerContext->pendingConfig.filterTimeConstantSeconds;
    newEventConfig.sensor_type = registerContext->pendingConfig.sensorType;
    newEventConfig.tc_linearization =
        registerContext->pendingConfig.tcLinearization;

    newRevision = (uint16_t)(oldRevision + 1U);
    if (newRevision == 0U)
    {
        newRevision = 1U;
    }

    registerContext->activeConfig = registerContext->pendingConfig;
    registerContext->configurationRevision = newRevision;
    if (!EventService_RaiseTemperatureInputConfigurationChanged(
            PRODUCT_TEMPERATURE_INPUT_EVENT_CHANNEL,
            newRevision,
            changedMask,
            &oldEventConfig,
            &newEventConfig,
            NULL))
    {
        registerContext->activeConfig = oldConfig;
        registerContext->configurationRevision = oldRevision;
        return MODBUS_EXCEPTION_SERVER_DEVICE_FAILURE;
    }

    registerContext->pendingDirty = false;
    return MODBUS_EXCEPTION_NONE;
}

static ModbusExceptionCode_t WriteSingleRegister(
    void *context,
    uint16_t address,
    uint16_t value)
{
    product_modbus_register_context_t *registerContext =
        (product_modbus_register_context_t *)context;
    ModbusSerialRegisterInfo_t serial_information;

    if (registerContext == NULL)
    {
        return MODBUS_EXCEPTION_SERVER_DEVICE_FAILURE;
    }

    if (ModbusRegisterAdapter_ResolveSerialAddress(
            address, &serial_information))
    {
        return WriteProductSerialRegister(address, value);
    }

    if (address == PRODUCT_MODBUS_FACTORY_CAL_UNLOCK1_ADDRESS)
    {
        FactoryCalibrationService_SetUnlockKey1(value);
        return MODBUS_EXCEPTION_NONE;
    }
    if (address == PRODUCT_MODBUS_FACTORY_CAL_UNLOCK2_ADDRESS)
    {
        FactoryCalibrationService_SetUnlockKey2(value);
        return MODBUS_EXCEPTION_NONE;
    }
    if (address == PRODUCT_MODBUS_FACTORY_CAL_INPUT_ADDRESS)
    {
        FactoryCalibrationSnapshot_t snapshot;
        FactoryCalibrationService_GetSnapshot(&snapshot);
        return FactoryCalibrationService_Select(
                   (uint8_t)value, snapshot.profile) ?
                   MODBUS_EXCEPTION_NONE : MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE;
    }
    if (address == PRODUCT_MODBUS_FACTORY_CAL_PROFILE_ADDRESS)
    {
        FactoryCalibrationSnapshot_t snapshot;
        FactoryCalibrationService_GetSnapshot(&snapshot);
        return FactoryCalibrationService_Select(
                   snapshot.input, (FactoryCalibrationProfile_t)value) ?
                   MODBUS_EXCEPTION_NONE : MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE;
    }
    if (address == PRODUCT_MODBUS_FACTORY_CAL_COMMAND_ADDRESS)
    {
        return ExecuteFactoryCalibrationCommand(value);
    }

    if (address == PRODUCT_MODBUS_SENSOR_TYPE_ADDRESS)
    {
        if (!IsSensorTypeValid(value))
        {
            return MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE;
        }
        registerContext->pendingConfig.sensorType = value;
        registerContext->pendingDirty = true;
        return MODBUS_EXCEPTION_NONE;
    }

    if (address == PRODUCT_MODBUS_TC_LINEARIZATION_ADDRESS)
    {
        if (!IsTcLinearizationValid(value))
        {
            return MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE;
        }
        registerContext->pendingConfig.tcLinearization = value;
        registerContext->pendingDirty = true;
        return MODBUS_EXCEPTION_NONE;
    }

    if (address == PRODUCT_MODBUS_APPLY_KEY_ADDRESS)
    {
        return ApplyPendingConfiguration(registerContext, value);
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
    ModbusSerialRegisterInfo_t serial_information;

    if ((registerContext == NULL) || (values == NULL))
    {
        return MODBUS_EXCEPTION_SERVER_DEVICE_FAILURE;
    }

    if (ModbusRegisterAdapter_ResolveSerialAddress(
            starting_address, &serial_information))
    {
        uint16_t index;
        for (index = 0U; index < quantity; index++)
        {
            ModbusSerialRegisterInfo_t item;
            uint16_t address = (uint16_t)(starting_address + index);
            if (!ModbusRegisterAdapter_ResolveSerialAddress(address, &item) ||
                (item.port != serial_information.port) ||
                (item.field == MODBUS_SERIAL_REGISTER_ROLE) ||
                ((item.port == 1U) &&
                 (item.field == MODBUS_SERIAL_REGISTER_UNIT_ID)) ||
                (item.access == MODBUS_REGISTER_ACCESS_READ_ONLY) ||
                ((item.field == MODBUS_SERIAL_REGISTER_PROTOCOL) &&
                 (values[index] !=
                  (uint16_t)SERIAL_PROTOCOL_MODBUS_RTU) &&
                 (values[index] !=
                  (uint16_t)SERIAL_PROTOCOL_MODBUS_ASCII)) ||
                !ModbusRegisterAdapter_IsSerialValueValid(
                    item.field, values[index]))
            {
                return MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE;
            }
        }
        for (index = 0U; index < quantity; index++)
        {
            ModbusExceptionCode_t result = WriteProductSerialRegister(
                (uint16_t)(starting_address + index), values[index]);
            if (result != MODBUS_EXCEPTION_NONE)
            {
                return result;
            }
        }
        return MODBUS_EXCEPTION_NONE;
    }

    pendingConfig = registerContext->pendingConfig;

    if ((starting_address == PRODUCT_MODBUS_FACTORY_CAL_UNLOCK1_ADDRESS) &&
        (quantity == 2U))
    {
        FactoryCalibrationService_SetUnlockKey1(values[0]);
        FactoryCalibrationService_SetUnlockKey2(values[1]);
        return MODBUS_EXCEPTION_NONE;
    }

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
    else if ((starting_address == PRODUCT_MODBUS_APPLY_KEY_ADDRESS) &&
             (quantity == 1U))
    {
        return ApplyPendingConfiguration(registerContext, values[0]);
    }
    else
    {
        return MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS;
    }

    registerContext->pendingConfig = pendingConfig;
    registerContext->pendingDirty = true;
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
        *config = s_registerContext.activeConfig;
    }
}

uint16_t ProductModbusRegisterAdapter_GetTemperatureInputConfigurationRevision(
    void)
{
    return s_registerContext.configurationRevision;
}

bool ProductModbusRegisterAdapter_RestoreTemperatureInputConfig(
    const product_temperature_input_config_t *config,
    uint16_t configuration_revision)
{
    if ((config == NULL) || (configuration_revision == 0U) ||
        !IsFilterTimeConstantValid(config->filterTimeConstantSeconds) ||
        !IsSensorTypeValid(config->sensorType) ||
        !IsTcLinearizationValid(config->tcLinearization))
    {
        return false;
    }
    s_registerContext.activeConfig = *config;
    s_registerContext.pendingConfig = *config;
    s_registerContext.configurationRevision = configuration_revision;
    s_registerContext.pendingDirty = false;
    return true;
}

bool ProductModbusRegisterAdapter_HasPendingTemperatureInputConfig(void)
{
    return s_registerContext.pendingDirty;
}

void ProductModbusRegisterAdapter_GetPendingTemperatureInputConfig(
    product_temperature_input_config_t *config)
{
    if (config != NULL)
    {
        *config = s_registerContext.pendingConfig;
    }
}

void ProductModbusRegisterAdapter_DiscardPendingTemperatureInputConfig(void)
{
    s_registerContext.pendingConfig = s_registerContext.activeConfig;
    s_registerContext.pendingDirty = false;
}
