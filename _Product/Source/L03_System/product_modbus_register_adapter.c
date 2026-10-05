/*
 * Copyright 2026
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "product_modbus_register_adapter.h"
#include "product_temperature_input_types.h"
#include "ProductConfig.h"

#include "EventService.h"
#include "FactoryCalibrationService.h"
#include "FaultService.h"
#include "ModbusRegisterAdapter.h"
#include "PwmOutputService.h"
#include "DigitalInputService.h"
#include "DigitalOutputService.h"
#include "bsp_analog_output.h"
#include "product_fram_bank_test.h"
#include "product_dip_switch_driver.h"
#include "product_adc_driver.h"
#include "product_rotary_switch_driver.h"
#include "product_temperature_range_resolver.h"

#include <stddef.h>
#include <string.h>

#define PRODUCT_TEMPERATURE_VALUE_MIN              (-99999.0F)
#define PRODUCT_TEMPERATURE_VALUE_MAX              (99999.0F)
#define PRODUCT_FILTER_TIME_CONSTANT_MIN_SECONDS   (0.0F)
#define PRODUCT_FILTER_TIME_CONSTANT_MAX_SECONDS   (60.0F)

#define PRODUCT_INPUT_ERROR_NONE                   (61U)

typedef struct _product_modbus_register_context
{
    product_temperature_input_monitor_t
        monitor[PRODUCT_MODBUS_TEMPERATURE_INPUT_COUNT];
    product_temperature_input_config_t
        activeConfig[PRODUCT_MODBUS_TEMPERATURE_INPUT_COUNT];
    product_temperature_input_config_t
        pendingConfig[PRODUCT_MODBUS_TEMPERATURE_INPUT_COUNT];
    uint16_t
        configurationRevision[PRODUCT_MODBUS_TEMPERATURE_INPUT_COUNT];
    uint16_t diagnosticFaultIndex;
    bool pendingDirty[PRODUCT_MODBUS_TEMPERATURE_INPUT_COUNT];
    product_pwm_output_config_t
        activePwmConfig[PRODUCT_MODBUS_PWM_CHANNEL_COUNT];
    product_pwm_output_config_t
        pendingPwmConfig[PRODUCT_MODBUS_PWM_CHANNEL_COUNT];
    uint16_t pwmConfigurationRevision;
    uint16_t pwmPendingMask;
    uint16_t activeDacCode[PRODUCT_MODBUS_DAC_CHANNEL_COUNT];
    uint16_t pendingDacCode[PRODUCT_MODBUS_DAC_CHANNEL_COUNT];
    uint16_t dacConfigurationRevision;
    uint16_t dacPendingMask;
    uint16_t dacStatus;
    uint16_t dacFailedChannel;
    uint16_t pendingDigitalOutput[PRODUCT_MODBUS_DO_CHANNEL_COUNT];
    uint16_t digitalOutputPendingMask;
    product_low_voltage_monitor_t lowVoltageMonitor;
} product_modbus_register_context_t;

static product_modbus_register_context_t s_registerContext =
{
    .monitor =
    {
        {0.0F, PRODUCT_INPUT_ERROR_NONE, 0.0F},
        {0.0F, PRODUCT_INPUT_ERROR_NONE, 0.0F},
        {0.0F, PRODUCT_INPUT_ERROR_NONE, 0.0F},
        {0.0F, PRODUCT_INPUT_ERROR_NONE, 0.0F}
    },
    .activeConfig =
    {
        {0.5F, PRODUCT_SENSOR_TYPE_TC_K},
        {0.5F, PRODUCT_SENSOR_TYPE_TC_K},
        {0.5F, PRODUCT_SENSOR_TYPE_TC_K},
        {0.5F, PRODUCT_SENSOR_TYPE_TC_K}
    },
    .pendingConfig =
    {
        {0.5F, PRODUCT_SENSOR_TYPE_TC_K},
        {0.5F, PRODUCT_SENSOR_TYPE_TC_K},
        {0.5F, PRODUCT_SENSOR_TYPE_TC_K},
        {0.5F, PRODUCT_SENSOR_TYPE_TC_K}
    },
    .activePwmConfig =
    {
        {PWM_OUTPUT_PERIOD_DEFAULT_MS, 0U,
         PRODUCT_MODBUS_PWM_UPDATE_NEXT_CYCLE},
        {PWM_OUTPUT_PERIOD_DEFAULT_MS, 0U,
         PRODUCT_MODBUS_PWM_UPDATE_NEXT_CYCLE},
        {PWM_OUTPUT_PERIOD_DEFAULT_MS, 0U,
         PRODUCT_MODBUS_PWM_UPDATE_NEXT_CYCLE},
        {PWM_OUTPUT_PERIOD_DEFAULT_MS, 0U,
         PRODUCT_MODBUS_PWM_UPDATE_NEXT_CYCLE}
    },
    .pendingPwmConfig =
    {
        {PWM_OUTPUT_PERIOD_DEFAULT_MS, 0U,
         PRODUCT_MODBUS_PWM_UPDATE_NEXT_CYCLE},
        {PWM_OUTPUT_PERIOD_DEFAULT_MS, 0U,
         PRODUCT_MODBUS_PWM_UPDATE_NEXT_CYCLE},
        {PWM_OUTPUT_PERIOD_DEFAULT_MS, 0U,
         PRODUCT_MODBUS_PWM_UPDATE_NEXT_CYCLE},
        {PWM_OUTPUT_PERIOD_DEFAULT_MS, 0U,
         PRODUCT_MODBUS_PWM_UPDATE_NEXT_CYCLE}
    },
    .pwmConfigurationRevision = 0U,
    .pwmPendingMask = 0U,
    .activeDacCode = {0U, 0U, 0U, 0U},
    .pendingDacCode = {0U, 0U, 0U, 0U},
    .dacConfigurationRevision = 0U,
    .dacPendingMask = 0U,
    .dacStatus = PRODUCT_MODBUS_DAC_STATUS_READY,
    .dacFailedChannel = PRODUCT_MODBUS_DAC_FAILED_CHANNEL_NONE,
    .pendingDigitalOutput = {0U, 0U, 0U, 0U},
    .digitalOutputPendingMask = 0U,
    .lowVoltageMonitor =
    {
        0U, 0U, PRODUCT_MODBUS_LOW_VOLTAGE_STATUS_NOT_INITIALIZED,
        false, false, false
    }
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
           (value == PRODUCT_SENSOR_TYPE_TC_B) ||
           (value == PRODUCT_SENSOR_TYPE_TC_C) ||
           (value == PRODUCT_SENSOR_TYPE_TC_D) ||
           (value == PRODUCT_SENSOR_TYPE_TC_E) ||
           (value == PRODUCT_SENSOR_TYPE_TC_J) ||
           (value == PRODUCT_SENSOR_TYPE_TC_K) ||
           (value == PRODUCT_SENSOR_TYPE_TC_N) ||
           (value == PRODUCT_SENSOR_TYPE_TC_R) ||
           (value == PRODUCT_SENSOR_TYPE_TC_S) ||
           (value == PRODUCT_SENSOR_TYPE_TC_T) ||
           (value == PRODUCT_SENSOR_TYPE_TC_L) ||
           (value == PRODUCT_SENSOR_TYPE_TC_U) ||
           (value == PRODUCT_SENSOR_TYPE_TC_TXK) ||
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

static bool ResolveTemperatureRegisterRange(
    uint16_t startingAddress,
    uint16_t quantity,
    uint8_t *channel,
    uint16_t *offset)
{
    uint16_t relative;
    uint16_t localOffset;
    uint8_t input;

    if ((quantity == 0U) ||
        (startingAddress < PRODUCT_MODBUS_TEMPERATURE_INPUT_BASE_ADDRESS) ||
        (startingAddress > PRODUCT_MODBUS_TEMPERATURE_INPUT_LAST_ADDRESS))
    {
        return false;
    }
    relative = (uint16_t)(startingAddress -
                          PRODUCT_MODBUS_TEMPERATURE_INPUT_BASE_ADDRESS);
    input = (uint8_t)(relative /
                      PRODUCT_MODBUS_TEMPERATURE_INPUT_INSTANCE_STRIDE);
    localOffset = (uint16_t)(relative %
                             PRODUCT_MODBUS_TEMPERATURE_INPUT_INSTANCE_STRIDE);
    if ((input >= PRODUCT_MODBUS_TEMPERATURE_INPUT_COUNT) ||
        (localOffset > 9U) ||
        (((uint32_t)localOffset + quantity) > 10UL))
    {
        return false;
    }
    if (channel != NULL)
    {
        *channel = input;
    }
    if (offset != NULL)
    {
        *offset = localOffset;
    }
    return true;
}

static bool IsPwmRangeValid(uint16_t startingAddress, uint16_t quantity)
{
    uint32_t endingAddress;

    if (quantity == 0U)
    {
        return false;
    }
    endingAddress = (uint32_t)startingAddress + (uint32_t)quantity - 1UL;
    return (startingAddress >= PRODUCT_MODBUS_PWM_BASE_ADDRESS) &&
           (endingAddress <= PRODUCT_MODBUS_PWM_LAST_ADDRESS);
}

static bool IsDacRangeValid(uint16_t startingAddress, uint16_t quantity)
{
    uint32_t endingAddress;

    if (quantity == 0U)
    {
        return false;
    }
    endingAddress = (uint32_t)startingAddress + (uint32_t)quantity - 1UL;
    return (startingAddress >= PRODUCT_MODBUS_DAC_BASE_ADDRESS) &&
           (endingAddress <= PRODUCT_MODBUS_DAC_LAST_ADDRESS);
}

static bool IsDigitalInputRangeValid(uint16_t startingAddress,
                                     uint16_t quantity)
{
    uint32_t endingAddress;

    if (quantity == 0U)
    {
        return false;
    }
    endingAddress = (uint32_t)startingAddress + (uint32_t)quantity - 1UL;
    return (startingAddress >= PRODUCT_MODBUS_DI_BASE_ADDRESS) &&
           (endingAddress <= PRODUCT_MODBUS_DI_LAST_ADDRESS);
}

static bool IsDigitalOutputRangeValid(uint16_t startingAddress,
                                      uint16_t quantity)
{
    uint32_t endingAddress;

    if (quantity == 0U)
    {
        return false;
    }
    endingAddress = (uint32_t)startingAddress + (uint32_t)quantity - 1UL;
    return (startingAddress >= PRODUCT_MODBUS_DO_BASE_ADDRESS) &&
           (endingAddress <= PRODUCT_MODBUS_DO_LAST_ADDRESS);
}

static bool IsDipSwitchRangeValid(uint16_t startingAddress,
                                  uint16_t quantity)
{
    uint32_t endingAddress;

    if (quantity == 0U)
    {
        return false;
    }
    endingAddress = (uint32_t)startingAddress + (uint32_t)quantity - 1UL;
    return (startingAddress >= PRODUCT_MODBUS_DIP_SWITCH_BASE_ADDRESS) &&
           (endingAddress <= PRODUCT_MODBUS_DIP_SWITCH_LAST_ADDRESS);
}

static bool IsLowVoltageRangeValid(uint16_t startingAddress,
                                   uint16_t quantity)
{
    uint32_t endingAddress;

    if (quantity == 0U)
    {
        return false;
    }
    endingAddress = (uint32_t)startingAddress + (uint32_t)quantity - 1UL;
    return (startingAddress >= PRODUCT_MODBUS_LOW_VOLTAGE_BASE_ADDRESS) &&
           (endingAddress <= PRODUCT_MODBUS_LOW_VOLTAGE_LAST_ADDRESS);
}

static bool IsRotarySwitchRangeValid(uint16_t startingAddress,
                                     uint16_t quantity)
{
    uint32_t endingAddress;

    if (quantity == 0U)
    {
        return false;
    }
    endingAddress = (uint32_t)startingAddress + (uint32_t)quantity - 1UL;
    return (startingAddress >= PRODUCT_MODBUS_ROTARY_SWITCH_BASE_ADDRESS) &&
           (endingAddress <= PRODUCT_MODBUS_ROTARY_SWITCH_LAST_ADDRESS);
}

static bool IsPwmConfigValueValid(uint16_t field, uint16_t value)
{
    if (field == PRODUCT_MODBUS_PWM_PERIOD_OFFSET)
    {
        return (value >= PWM_OUTPUT_PERIOD_MIN_MS) &&
               (value <= PWM_OUTPUT_PERIOD_MAX_MS);
    }
    if (field == PRODUCT_MODBUS_PWM_DUTY_OFFSET)
    {
        return value <= HAL_PWM_DUTY_MAX_PERMILLE;
    }
    if (field == PRODUCT_MODBUS_PWM_UPDATE_MODE_OFFSET)
    {
        return (value == PRODUCT_MODBUS_PWM_UPDATE_IMMEDIATE) ||
               (value == PRODUCT_MODBUS_PWM_UPDATE_NEXT_CYCLE);
    }
    return false;
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

static bool IsVersionRangeValid(uint16_t startingAddress, uint16_t quantity)
{
    uint32_t endingAddress;
    if (quantity == 0U)
    {
        return false;
    }
    endingAddress = (uint32_t)startingAddress + quantity - 1UL;
    return (startingAddress >= PRODUCT_MODBUS_VERSION_BASE_ADDRESS) &&
           (endingAddress <= PRODUCT_MODBUS_VERSION_LAST_USED_ADDRESS);
}

static bool IsFactoryFramRangeValid(uint16_t startingAddress,
                                    uint16_t quantity)
{
    uint32_t endingAddress;
    if (quantity == 0U)
    {
        return false;
    }
    endingAddress = (uint32_t)startingAddress + quantity - 1UL;
    return (startingAddress >= PRODUCT_MODBUS_FACTORY_FRAM_BASE_ADDRESS) &&
           (endingAddress <= PRODUCT_MODBUS_FACTORY_FRAM_LAST_ADDRESS);
}

static bool IsDiagnosticsRangeValid(uint16_t startingAddress,
                                    uint16_t quantity)
{
    uint32_t endingAddress;
    if (quantity == 0U)
    {
        return false;
    }
    endingAddress = (uint32_t)startingAddress + quantity - 1UL;
    return (startingAddress >= PRODUCT_MODBUS_DIAGNOSTICS_BASE_ADDRESS) &&
           (endingAddress <=
            PRODUCT_MODBUS_DIAGNOSTICS_LAST_USED_ADDRESS);
}

static bool IsAdcDiagnosticsRangeValid(uint16_t startingAddress,
                                       uint16_t quantity)
{
    uint32_t endingAddress;
    if (quantity == 0U)
    {
        return false;
    }
    endingAddress = (uint32_t)startingAddress + quantity - 1UL;
    return (startingAddress >=
            PRODUCT_MODBUS_ADC_DIAGNOSTICS_BASE_ADDRESS) &&
           (endingAddress <=
            PRODUCT_MODBUS_ADC_DIAGNOSTICS_LAST_ADDRESS);
}

static void Uint32ToRegisters(uint32_t value, uint16_t *high, uint16_t *low)
{
    *high = (uint16_t)(value >> 16U);
    *low = (uint16_t)value;
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

static void BuildDiagnosticsImage(
    const product_modbus_register_context_t *registerContext,
    uint16_t *registers)
{
    FaultRecord_t fault;

    (void)memset(registers, 0, 11U * sizeof(registers[0]));
    registers[0] = FaultService_GetActiveCount();
    registers[1] = registerContext->diagnosticFaultIndex;

    if (FaultService_GetActiveByIndex(
            registerContext->diagnosticFaultIndex, &fault))
    {
        registers[2] = (uint16_t)fault.code;
        registers[3] = fault.last_detail;
        registers[4] = fault.last_configuration_revision;
        Uint32ToRegisters(fault.last_event_id,
                          &registers[5], &registers[6]);
        Uint32ToRegisters(fault.occurrence_count,
                          &registers[7], &registers[8]);
    }
    /* Clear Code and Clear Key are write-only and always read as zero. */
}

static uint16_t ReadAdcDiagnosticValue(
    const ProductAdcDriverDiagnostics_t *diagnostics,
    uint16_t offset)
{
    uint16_t high;
    uint16_t low;

    switch (offset)
    {
        case 0U: return diagnostics->initialized ? 1U : 0U;
        case 1U: return diagnostics->device_id;
        case 2U: return diagnostics->last_status_register;
        case 3U: return diagnostics->active_fault_categories;
        case 4U: return diagnostics->last_fault_categories;
        case 5U:
        case 6U:
            Int32ToRegisters(diagnostics->last_driver_status, &high, &low);
            return (offset == 5U) ? high : low;
        case 7U:
        case 8U:
            Uint32ToRegisters(diagnostics->initial_error_register, &high, &low);
            return (offset == 7U) ? high : low;
        case 9U:
        case 10U:
            Uint32ToRegisters(diagnostics->first_error_register, &high, &low);
            return (offset == 9U) ? high : low;
        case 11U:
        case 12U:
            Uint32ToRegisters(diagnostics->last_error_register, &high, &low);
            return (offset == 11U) ? high : low;
        case 13U:
        case 14U:
            Uint32ToRegisters(diagnostics->latched_error_register, &high, &low);
            return (offset == 13U) ? high : low;
        case 15U:
        case 16U:
            Uint32ToRegisters(diagnostics->initialization_attempts, &high, &low);
            return (offset == 15U) ? high : low;
        case 17U:
        case 18U:
            Uint32ToRegisters(diagnostics->successful_samples, &high, &low);
            return (offset == 17U) ? high : low;
        case 19U:
        case 20U:
            Uint32ToRegisters(diagnostics->discarded_samples, &high, &low);
            return (offset == 19U) ? high : low;
        case 21U:
        case 22U:
            Uint32ToRegisters(diagnostics->not_ready_polls, &high, &low);
            return (offset == 21U) ? high : low;
        case 23U:
        case 24U:
            Uint32ToRegisters(diagnostics->error_register_reads, &high, &low);
            return (offset == 23U) ? high : low;
        case 25U:
        case 26U:
            Uint32ToRegisters(
                diagnostics->error_register_read_failures, &high, &low);
            return (offset == 25U) ? high : low;
        case 27U:
        case 28U:
            Uint32ToRegisters(diagnostics->crc_errors, &high, &low);
            return (offset == 27U) ? high : low;
        case 29U:
        case 30U:
            Uint32ToRegisters(diagnostics->transport_errors, &high, &low);
            return (offset == 29U) ? high : low;
        case 31U:
        case 32U:
            Uint32ToRegisters(diagnostics->device_errors, &high, &low);
            return (offset == 31U) ? high : low;
        case 33U:
        case 34U:
            Uint32ToRegisters(diagnostics->communication_faults, &high, &low);
            return (offset == 33U) ? high : low;
        case 35U:
        case 36U:
            Uint32ToRegisters(diagnostics->integrity_faults, &high, &low);
            return (offset == 35U) ? high : low;
        case 37U:
        case 38U:
            Uint32ToRegisters(diagnostics->reference_faults, &high, &low);
            return (offset == 37U) ? high : low;
        case 39U:
        case 40U:
            Uint32ToRegisters(diagnostics->conversion_faults, &high, &low);
            return (offset == 39U) ? high : low;
        case 41U:
        case 42U:
            Uint32ToRegisters(diagnostics->input_voltage_faults, &high, &low);
            return (offset == 41U) ? high : low;
        case 43U:
        case 44U:
            Uint32ToRegisters(diagnostics->internal_faults, &high, &low);
            return (offset == 43U) ? high : low;
        case 45U:
        case 46U:
            Uint32ToRegisters(diagnostics->unexpected_por_faults, &high, &low);
            return (offset == 45U) ? high : low;
        case 47U:
            return (uint16_t)(
                ((uint16_t)diagnostics->consecutive_transaction_errors << 8U) |
                diagnostics->consecutive_clean_samples);
        default:
            return 0U;
    }
}

static ModbusExceptionCode_t ReadAdcDiagnostics(
    uint16_t startingAddress, uint16_t quantity, uint16_t *values)
{
    ProductAdcDriverDiagnostics_t diagnostics;
    uint8_t cachedDevice = UINT8_MAX;
    uint16_t index;

    for (index = 0U; index < quantity; index++)
    {
        uint16_t relativeAddress = (uint16_t)(
            startingAddress + index -
            PRODUCT_MODBUS_ADC_DIAGNOSTICS_BASE_ADDRESS);
        uint8_t device = (uint8_t)(relativeAddress /
            PRODUCT_MODBUS_ADC_DIAGNOSTICS_DEVICE_STRIDE);
        uint16_t offset = (uint16_t)(relativeAddress %
            PRODUCT_MODBUS_ADC_DIAGNOSTICS_DEVICE_STRIDE);

        if (device != cachedDevice)
        {
            if (!ProductAdcDriver_GetDiagnostics(device, &diagnostics))
            {
                return MODBUS_EXCEPTION_SERVER_DEVICE_FAILURE;
            }
            cachedDevice = device;
        }
        values[index] = ReadAdcDiagnosticValue(&diagnostics, offset);
    }
    return MODBUS_EXCEPTION_NONE;
}

static void BuildFactoryFramImage(uint16_t *registers)
{
    ProductFramBankTestSnapshot_t snapshot;

    (void)memset(registers, 0, 13U * sizeof(registers[0]));
    ProductFramBankTest_GetSnapshot(&snapshot);
    registers[1] = (uint16_t)snapshot.state;
    registers[2] = (uint16_t)snapshot.error;
    registers[3] = snapshot.current_bank;
    registers[4] = snapshot.completed_bank_mask;
    registers[5] = snapshot.failed_bank_mask;
    registers[6] = snapshot.progress_permille;
    Uint32ToRegisters(snapshot.current_address,
                      &registers[7], &registers[8]);
    Uint32ToRegisters(snapshot.failure_address,
                      &registers[9], &registers[10]);
    registers[11] = snapshot.expected_value;
    registers[12] = snapshot.actual_value;
}

static void BuildVersionImage(uint16_t *registers)
{
    registers[0] = (uint16_t)PRODUCT_FIRMWARE_VERSION_U16;
    registers[1] = (uint16_t)PRODUCT_FIRMWARE_VERSION_SUB1_U16;
    registers[2] = (uint16_t)PRODUCT_FIRMWARE_VERSION_SUB2_U16;
    registers[3] =
        (uint16_t)PRODUCT_COMPATIBLE_FIRMWARE_VERSION_MIN_U16;
    registers[4] =
        (uint16_t)PRODUCT_COMPATIBLE_FIRMWARE_VERSION_MAX_U16;
    registers[5] =
        (uint16_t)PRODUCT_COMPATIBLE_PARAMETER_VERSION_MIN_U16;
    registers[6] =
        (uint16_t)PRODUCT_COMPATIBLE_PARAMETER_VERSION_MAX_U16;
    registers[7] =
        (uint16_t)PRODUCT_COMPATIBLE_SOFTWARE_VERSION_MIN_U16;
    registers[8] =
        (uint16_t)PRODUCT_COMPATIBLE_SOFTWARE_VERSION_MAX_U16;
}

static void BuildRegisterImage(
    const product_modbus_register_context_t *registerContext,
    uint8_t channel,
    uint16_t *registers)
{
    FloatToRegisters(registerContext->monitor[channel].unfilteredProcessValue,
                     &registers[0], &registers[1]);
    registers[2] = registerContext->monitor[channel].inputError;
    FloatToRegisters(
        registerContext->activeConfig[channel].filterTimeConstantSeconds,
                     &registers[3], &registers[4]);
    FloatToRegisters(registerContext->monitor[channel].filteredProcessValue,
                     &registers[5], &registers[6]);
    registers[7] = registerContext->activeConfig[channel].sensorType;
    /* Reserved to preserve the published register layout. */
    registers[8] = 0U;
    /* The command key is never echoed back through the register image. */
    registers[9] = 0U;
}

static void BuildPwmRegisterImage(
    const product_modbus_register_context_t *registerContext,
    uint16_t *registers)
{
    uint8_t channel;

    for (channel = 0U;
         channel < PRODUCT_MODBUS_PWM_CHANNEL_COUNT;
         channel++)
    {
        uint16_t offset =
            (uint16_t)channel * PRODUCT_MODBUS_PWM_CHANNEL_STRIDE;
        registers[offset + PRODUCT_MODBUS_PWM_PERIOD_OFFSET] =
            registerContext->activePwmConfig[channel].periodMs;
        registers[offset + PRODUCT_MODBUS_PWM_DUTY_OFFSET] =
            registerContext->activePwmConfig[channel].dutyPermille;
        registers[offset + PRODUCT_MODBUS_PWM_UPDATE_MODE_OFFSET] =
            registerContext->activePwmConfig[channel].updateMode;
    }
    /* Apply key is write-only. */
    registers[12] = 0U;
    registers[13] = registerContext->pwmConfigurationRevision;
    registers[14] = registerContext->pwmPendingMask;
}

static void BuildDacRegisterImage(
    const product_modbus_register_context_t *registerContext,
    uint16_t *registers)
{
    uint8_t channel;

    for (channel = 0U;
         channel < PRODUCT_MODBUS_DAC_CHANNEL_COUNT;
         channel++)
    {
        registers[channel] = registerContext->activeDacCode[channel];
    }
    registers[4] = 0U; /* Apply key is write-only. */
    registers[5] = registerContext->dacConfigurationRevision;
    registers[6] = registerContext->dacPendingMask;
    registers[7] = registerContext->dacStatus;
    registers[8] = registerContext->dacFailedChannel;
}

static bool BuildDigitalInputImage(uint16_t *registers)
{
    DigitalInputSnapshot_t snapshot;
    uint8_t channel;

    if (!DigitalInputService_GetSnapshot(&snapshot))
    {
        return false;
    }
    for (channel = 0U; channel < PRODUCT_MODBUS_DI_CHANNEL_COUNT; channel++)
    {
        registers[channel] =
            ((snapshot.state_mask & (uint16_t)(1UL << channel)) != 0U) ?
                1U : 0U;
    }
    registers[4] = snapshot.state_mask;
    registers[5] = snapshot.revision;
    return true;
}

static bool BuildDigitalOutputImage(
    const product_modbus_register_context_t *registerContext,
    uint16_t *registers)
{
    DigitalOutputState_t state;
    uint8_t channel;

    if (!DigitalOutputService_GetState(&state))
    {
        return false;
    }
    for (channel = 0U; channel < PRODUCT_MODBUS_DO_CHANNEL_COUNT; channel++)
    {
        registers[channel] =
            ((state.active_mask & (uint16_t)(1UL << channel)) != 0U) ?
                1U : 0U;
    }
    registers[4] = 0U;
    registers[5] = state.revision;
    registers[6] = registerContext->digitalOutputPendingMask;
    registers[7] = state.active_mask;
    if (state.last_status == DIGITAL_OUTPUT_STATUS_OK)
    {
        registers[8] = PRODUCT_MODBUS_DO_STATUS_READY;
    }
    else if (state.last_status == DIGITAL_OUTPUT_STATUS_ROLLBACK_ERROR)
    {
        registers[8] = PRODUCT_MODBUS_DO_STATUS_ROLLBACK_FAILED;
    }
    else
    {
        registers[8] = PRODUCT_MODBUS_DO_STATUS_APPLY_FAILED;
    }
    registers[9] = state.failed_channel;
    return true;
}

static bool BuildDipSwitchImage(uint16_t *registers)
{
    ProductDipSwitchSnapshot_t snapshot;
    uint8_t switch_index;

    if (!ProductDipSwitchDriver_GetSnapshot(&snapshot))
    {
        return false;
    }
    for (switch_index = 0U;
         switch_index < PRODUCT_MODBUS_DIP_SWITCH_COUNT;
         switch_index++)
    {
        registers[switch_index] =
            ((snapshot.logical_mask &
              (uint8_t)(1UL << switch_index)) != 0U) ? 1U : 0U;
    }
    registers[8] = snapshot.logical_mask;
    registers[9] = snapshot.raw_value;
    registers[10] = snapshot.revision;
    registers[11] = (uint16_t)snapshot.status;
    return true;
}

static void BuildLowVoltageImage(
    const product_modbus_register_context_t *registerContext,
    uint16_t *registers)
{
    const product_low_voltage_monitor_t *monitor =
        &registerContext->lowVoltageMonitor;

    registers[0] = monitor->rawActive ? 1U : 0U;
    registers[1] = monitor->confirmedActive ? 1U : 0U;
    registers[2] = monitor->faultActive ? 1U : 0U;
    registers[3] = monitor->revision;
    Uint32ToRegisters(monitor->interruptCount,
                      &registers[4], &registers[5]);
    registers[6] = monitor->status;
}

static bool BuildRotarySwitchImage(uint16_t *registers)
{
    ProductRotarySwitchSnapshot_t snapshot;

    if (!ProductRotarySwitchDriver_GetSnapshot(&snapshot))
    {
        return false;
    }
    registers[0] = snapshot.position;
    registers[1] = snapshot.raw_value;
    registers[2] = snapshot.revision;
    registers[3] = (uint16_t)snapshot.status;
    return true;
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
    uint16_t versionImage[9];
    uint16_t factoryImage[14];
    uint16_t diagnosticsImage[11];
    uint16_t factoryFramImage[13];
    uint16_t pwmImage[15];
    uint16_t dacImage[9];
    uint16_t digitalInputImage[6];
    uint16_t digitalOutputImage[10];
    uint16_t dipSwitchImage[12];
    uint16_t lowVoltageImage[7];
    uint16_t rotarySwitchImage[4];
    uint16_t sourceOffset;
    uint8_t temperatureChannel;
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

    if (IsVersionRangeValid(starting_address, quantity))
    {
        BuildVersionImage(versionImage);
        sourceOffset = (uint16_t)(starting_address -
                                 PRODUCT_MODBUS_VERSION_BASE_ADDRESS);
        (void)memcpy(values, &versionImage[sourceOffset],
                     (size_t)quantity * sizeof(values[0]));
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
    if (IsFactoryFramRangeValid(starting_address, quantity))
    {
        BuildFactoryFramImage(factoryFramImage);
        sourceOffset = (uint16_t)(starting_address -
                                 PRODUCT_MODBUS_FACTORY_FRAM_BASE_ADDRESS);
        (void)memcpy(values, &factoryFramImage[sourceOffset],
                     (size_t)quantity * sizeof(values[0]));
        return MODBUS_EXCEPTION_NONE;
    }
    if (IsDiagnosticsRangeValid(starting_address, quantity))
    {
        BuildDiagnosticsImage(registerContext, diagnosticsImage);
        sourceOffset = (uint16_t)(starting_address -
                                 PRODUCT_MODBUS_DIAGNOSTICS_BASE_ADDRESS);
        (void)memcpy(values, &diagnosticsImage[sourceOffset],
                     (size_t)quantity * sizeof(values[0]));
        return MODBUS_EXCEPTION_NONE;
    }
    if (IsAdcDiagnosticsRangeValid(starting_address, quantity))
    {
        return ReadAdcDiagnostics(starting_address, quantity, values);
    }
    if (IsPwmRangeValid(starting_address, quantity))
    {
        BuildPwmRegisterImage(registerContext, pwmImage);
        sourceOffset = (uint16_t)(starting_address -
                                 PRODUCT_MODBUS_PWM_BASE_ADDRESS);
        (void)memcpy(values, &pwmImage[sourceOffset],
                     (size_t)quantity * sizeof(values[0]));
        return MODBUS_EXCEPTION_NONE;
    }
    if (IsDacRangeValid(starting_address, quantity))
    {
        BuildDacRegisterImage(registerContext, dacImage);
        sourceOffset = (uint16_t)(starting_address -
                                 PRODUCT_MODBUS_DAC_BASE_ADDRESS);
        (void)memcpy(values, &dacImage[sourceOffset],
                     (size_t)quantity * sizeof(values[0]));
        return MODBUS_EXCEPTION_NONE;
    }
    if (IsDigitalInputRangeValid(starting_address, quantity))
    {
        if (!BuildDigitalInputImage(digitalInputImage))
        {
            return MODBUS_EXCEPTION_SERVER_DEVICE_FAILURE;
        }
        sourceOffset = (uint16_t)(starting_address -
                                 PRODUCT_MODBUS_DI_BASE_ADDRESS);
        (void)memcpy(values, &digitalInputImage[sourceOffset],
                     (size_t)quantity * sizeof(values[0]));
        return MODBUS_EXCEPTION_NONE;
    }
    if (IsDigitalOutputRangeValid(starting_address, quantity))
    {
        if (!BuildDigitalOutputImage(registerContext, digitalOutputImage))
        {
            return MODBUS_EXCEPTION_SERVER_DEVICE_FAILURE;
        }
        sourceOffset = (uint16_t)(starting_address -
                                 PRODUCT_MODBUS_DO_BASE_ADDRESS);
        (void)memcpy(values, &digitalOutputImage[sourceOffset],
                     (size_t)quantity * sizeof(values[0]));
        return MODBUS_EXCEPTION_NONE;
    }
    if (IsDipSwitchRangeValid(starting_address, quantity))
    {
        if (!BuildDipSwitchImage(dipSwitchImage))
        {
            return MODBUS_EXCEPTION_SERVER_DEVICE_FAILURE;
        }
        sourceOffset = (uint16_t)(starting_address -
                                 PRODUCT_MODBUS_DIP_SWITCH_BASE_ADDRESS);
        (void)memcpy(values, &dipSwitchImage[sourceOffset],
                     (size_t)quantity * sizeof(values[0]));
        return MODBUS_EXCEPTION_NONE;
    }
    if (IsLowVoltageRangeValid(starting_address, quantity))
    {
        BuildLowVoltageImage(registerContext, lowVoltageImage);
        sourceOffset = (uint16_t)(starting_address -
                                 PRODUCT_MODBUS_LOW_VOLTAGE_BASE_ADDRESS);
        (void)memcpy(values, &lowVoltageImage[sourceOffset],
                     (size_t)quantity * sizeof(values[0]));
        return MODBUS_EXCEPTION_NONE;
    }
    if (IsRotarySwitchRangeValid(starting_address, quantity))
    {
        if (!BuildRotarySwitchImage(rotarySwitchImage))
        {
            return MODBUS_EXCEPTION_SERVER_DEVICE_FAILURE;
        }
        sourceOffset = (uint16_t)(starting_address -
                                 PRODUCT_MODBUS_ROTARY_SWITCH_BASE_ADDRESS);
        (void)memcpy(values, &rotarySwitchImage[sourceOffset],
                     (size_t)quantity * sizeof(values[0]));
        return MODBUS_EXCEPTION_NONE;
    }
    if (!ResolveTemperatureRegisterRange(starting_address, quantity,
                                         &temperatureChannel,
                                         &sourceOffset))
    {
        return MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS;
    }

    BuildRegisterImage(registerContext, temperatureChannel, registerImage);
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

static ModbusExceptionCode_t ExecuteFactoryFramCommand(uint16_t command)
{
    ProductFramBankTestSnapshot_t snapshot;

    if (command == PRODUCT_FACTORY_FRAM_COMMAND_ABORT)
    {
        ProductFramBankTest_Abort();
        return MODBUS_EXCEPTION_NONE;
    }
    if (command != PRODUCT_FACTORY_FRAM_COMMAND_START)
    {
        return MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE;
    }
    if (ProductFramBankTest_IsBusy())
    {
        return MODBUS_EXCEPTION_SERVER_DEVICE_BUSY;
    }
    if (ProductFramBankTest_Start())
    {
        return MODBUS_EXCEPTION_NONE;
    }
    ProductFramBankTest_GetSnapshot(&snapshot);
    return (snapshot.error == PRODUCT_FRAM_TEST_ERROR_NVM_BUSY) ?
               MODBUS_EXCEPTION_SERVER_DEVICE_BUSY :
               MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE;
}

static ModbusExceptionCode_t ApplyPendingConfiguration(
    product_modbus_register_context_t *registerContext,
    uint8_t channel,
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

    if ((channel >= PRODUCT_MODBUS_TEMPERATURE_INPUT_COUNT) ||
        !registerContext->pendingDirty[channel])
    {
        return MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE;
    }

    if (registerContext->activeConfig[channel].filterTimeConstantSeconds !=
        registerContext->pendingConfig[channel].filterTimeConstantSeconds)
    {
        changedMask |=
            EVENT_TEMPERATURE_INPUT_CHANGE_FILTER_TIME_CONSTANT;
    }
    if (registerContext->activeConfig[channel].sensorType !=
        registerContext->pendingConfig[channel].sensorType)
    {
        changedMask |= EVENT_TEMPERATURE_INPUT_CHANGE_SENSOR_TYPE;
    }
    if (changedMask == 0U)
    {
        registerContext->pendingDirty[channel] = false;
        return MODBUS_EXCEPTION_NONE;
    }

    if (EventService_IsTemperatureInputConfigurationChangedPending(
            channel))
    {
        return MODBUS_EXCEPTION_SERVER_DEVICE_FAILURE;
    }

    oldConfig = registerContext->activeConfig[channel];
    oldRevision = registerContext->configurationRevision[channel];
    oldEventConfig.filter_time_constant_seconds =
        oldConfig.filterTimeConstantSeconds;
    oldEventConfig.sensor_type = oldConfig.sensorType;
    newEventConfig.filter_time_constant_seconds =
        registerContext->pendingConfig[channel].filterTimeConstantSeconds;
    newEventConfig.sensor_type =
        registerContext->pendingConfig[channel].sensorType;

    {
        float minimum;
        float maximum;
        bool inputEnabled;
        if (!ProductTemperatureRangeResolver_Resolve(
                &newEventConfig, &minimum, &maximum, &inputEnabled, NULL) ||
            (inputEnabled && !(minimum <= maximum)))
        {
            /* Linked-field validation failed: retain Pending for correction. */
            return MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE;
        }
    }

    newRevision = (uint16_t)(oldRevision + 1U);
    if (newRevision == 0U)
    {
        newRevision = 1U;
    }

    registerContext->activeConfig[channel] =
        registerContext->pendingConfig[channel];
    registerContext->configurationRevision[channel] = newRevision;
    if (!EventService_RaiseTemperatureInputConfigurationChanged(
            channel,
            newRevision,
            changedMask,
            &oldEventConfig,
            &newEventConfig,
            NULL))
    {
        registerContext->activeConfig[channel] = oldConfig;
        registerContext->configurationRevision[channel] = oldRevision;
        return MODBUS_EXCEPTION_SERVER_DEVICE_FAILURE;
    }

    registerContext->pendingDirty[channel] = false;
    return MODBUS_EXCEPTION_NONE;
}

static PwmOutputStatus_t ApplyOnePwmConfiguration(
    uint8_t channel,
    const product_pwm_output_config_t *config)
{
    PwmOutputPeriodUpdateMode_t updateMode =
        (config->updateMode == PRODUCT_MODBUS_PWM_UPDATE_IMMEDIATE) ?
        PWM_OUTPUT_PERIOD_UPDATE_IMMEDIATE :
        PWM_OUTPUT_PERIOD_UPDATE_NEXT_CYCLE;
    PwmOutputStatus_t status;

    if (updateMode == PWM_OUTPUT_PERIOD_UPDATE_IMMEDIATE)
    {
        status = PwmOutputService_SetCommand(
            channel, config->dutyPermille);
        if (status != PWM_OUTPUT_STATUS_OK)
        {
            return status;
        }
        return PwmOutputService_SetPeriod(
            channel, config->periodMs, updateMode);
    }

    status = PwmOutputService_SetPeriod(
        channel, config->periodMs, updateMode);
    if (status != PWM_OUTPUT_STATUS_OK)
    {
        return status;
    }
    return PwmOutputService_SetCommand(channel, config->dutyPermille);
}

static ModbusExceptionCode_t ApplyPendingPwmConfiguration(
    product_modbus_register_context_t *registerContext,
    uint16_t applyKey)
{
    uint8_t channel;
    uint16_t effectiveMask = 0U;

    if ((applyKey != PRODUCT_MODBUS_PWM_APPLY_KEY_VALUE) ||
        (registerContext->pwmPendingMask == 0U))
    {
        return MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE;
    }

    for (channel = 0U;
         channel < PRODUCT_MODBUS_PWM_CHANNEL_COUNT;
         channel++)
    {
        uint16_t channelMask = (uint16_t)(1UL << channel);
        const product_pwm_output_config_t *active =
            &registerContext->activePwmConfig[channel];
        const product_pwm_output_config_t *pending =
            &registerContext->pendingPwmConfig[channel];

        if (((registerContext->pwmPendingMask & channelMask) != 0U) &&
            ((active->periodMs != pending->periodMs) ||
             (active->dutyPermille != pending->dutyPermille) ||
             (active->updateMode != pending->updateMode)))
        {
            effectiveMask |= channelMask;
        }
    }
    if (effectiveMask == 0U)
    {
        registerContext->pwmPendingMask = 0U;
        return MODBUS_EXCEPTION_NONE;
    }

    for (channel = 0U;
         channel < PRODUCT_MODBUS_PWM_CHANNEL_COUNT;
         channel++)
    {
        uint16_t channelMask = (uint16_t)(1UL << channel);
        if ((effectiveMask & channelMask) == 0U)
        {
            continue;
        }
        if (ApplyOnePwmConfiguration(
                channel,
                &registerContext->pendingPwmConfig[channel]) !=
            PWM_OUTPUT_STATUS_OK)
        {
            uint8_t rollbackChannel;
            for (rollbackChannel = 0U;
                 rollbackChannel <= channel;
                 rollbackChannel++)
            {
                uint16_t rollbackMask =
                    (uint16_t)(1UL << rollbackChannel);
                if ((effectiveMask & rollbackMask) != 0U)
                {
                    product_pwm_output_config_t rollbackConfig =
                        registerContext->activePwmConfig[rollbackChannel];
                    rollbackConfig.updateMode =
                        PRODUCT_MODBUS_PWM_UPDATE_IMMEDIATE;
                    (void)ApplyOnePwmConfiguration(
                        rollbackChannel, &rollbackConfig);
                }
            }
            return MODBUS_EXCEPTION_SERVER_DEVICE_FAILURE;
        }
    }

    for (channel = 0U;
         channel < PRODUCT_MODBUS_PWM_CHANNEL_COUNT;
         channel++)
    {
        uint16_t channelMask = (uint16_t)(1UL << channel);
        if ((effectiveMask & channelMask) != 0U)
        {
            registerContext->activePwmConfig[channel] =
                registerContext->pendingPwmConfig[channel];
        }
    }
    registerContext->pwmPendingMask = 0U;
    registerContext->pwmConfigurationRevision++;
    if (registerContext->pwmConfigurationRevision == 0U)
    {
        registerContext->pwmConfigurationRevision = 1U;
    }
    return MODBUS_EXCEPTION_NONE;
}

static ModbusExceptionCode_t StagePwmRegister(
    product_modbus_register_context_t *registerContext,
    uint16_t address,
    uint16_t value)
{
    uint16_t offset = (uint16_t)(address -
                                 PRODUCT_MODBUS_PWM_BASE_ADDRESS);
    uint8_t channel =
        (uint8_t)(offset / PRODUCT_MODBUS_PWM_CHANNEL_STRIDE);
    uint16_t field = offset % PRODUCT_MODBUS_PWM_CHANNEL_STRIDE;

    if ((channel >= PRODUCT_MODBUS_PWM_CHANNEL_COUNT) ||
        !IsPwmConfigValueValid(field, value))
    {
        return MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE;
    }

    if (field == PRODUCT_MODBUS_PWM_PERIOD_OFFSET)
    {
        registerContext->pendingPwmConfig[channel].periodMs = value;
    }
    else if (field == PRODUCT_MODBUS_PWM_DUTY_OFFSET)
    {
        registerContext->pendingPwmConfig[channel].dutyPermille = value;
    }
    else
    {
        registerContext->pendingPwmConfig[channel].updateMode = value;
    }
    registerContext->pwmPendingMask |= (uint16_t)(1UL << channel);
    return MODBUS_EXCEPTION_NONE;
}

static ModbusExceptionCode_t StageDacRegister(
    product_modbus_register_context_t *registerContext,
    uint16_t address,
    uint16_t value)
{
    uint16_t offset = (uint16_t)(address -
                                 PRODUCT_MODBUS_DAC_BASE_ADDRESS);

    if (offset >= PRODUCT_MODBUS_DAC_CHANNEL_COUNT)
    {
        return MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS;
    }
    registerContext->pendingDacCode[offset] = value;
    registerContext->dacPendingMask |= (uint16_t)(1UL << offset);
    return MODBUS_EXCEPTION_NONE;
}

static ModbusExceptionCode_t ApplyPendingDacCodes(
    product_modbus_register_context_t *registerContext,
    uint16_t applyKey)
{
    uint8_t channel;
    uint16_t effectiveMask = 0U;

    if ((applyKey != PRODUCT_MODBUS_DAC_APPLY_KEY_VALUE) ||
        (registerContext->dacPendingMask == 0U))
    {
        return MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE;
    }

    for (channel = 0U;
         channel < PRODUCT_MODBUS_DAC_CHANNEL_COUNT;
         channel++)
    {
        uint16_t channelMask = (uint16_t)(1UL << channel);
        if (((registerContext->dacPendingMask & channelMask) != 0U) &&
            (registerContext->activeDacCode[channel] !=
             registerContext->pendingDacCode[channel]))
        {
            effectiveMask |= channelMask;
        }
    }
    if (effectiveMask == 0U)
    {
        registerContext->dacPendingMask = 0U;
        registerContext->dacStatus = PRODUCT_MODBUS_DAC_STATUS_READY;
        registerContext->dacFailedChannel =
            PRODUCT_MODBUS_DAC_FAILED_CHANNEL_NONE;
        return MODBUS_EXCEPTION_NONE;
    }

    for (channel = 0U;
         channel < PRODUCT_MODBUS_DAC_CHANNEL_COUNT;
         channel++)
    {
        uint16_t channelMask = (uint16_t)(1UL << channel);
        if ((effectiveMask & channelMask) == 0U)
        {
            continue;
        }
        if (!BspAnalogOutput_WriteCode(
                (BspAnalogOutput_t)channel,
                registerContext->pendingDacCode[channel]))
        {
            uint8_t rollbackChannel;
            bool rollbackFailed = false;

            registerContext->dacStatus =
                PRODUCT_MODBUS_DAC_STATUS_APPLY_FAILED;
            registerContext->dacFailedChannel = channel;
            for (rollbackChannel = 0U;
                 rollbackChannel <= channel;
                 rollbackChannel++)
            {
                uint16_t rollbackMask =
                    (uint16_t)(1UL << rollbackChannel);
                if (((effectiveMask & rollbackMask) != 0U) &&
                    !BspAnalogOutput_WriteCode(
                        (BspAnalogOutput_t)rollbackChannel,
                        registerContext->activeDacCode[rollbackChannel]))
                {
                    rollbackFailed = true;
                }
            }
            if (rollbackFailed)
            {
                registerContext->dacStatus =
                    PRODUCT_MODBUS_DAC_STATUS_ROLLBACK_FAILED;
            }
            return MODBUS_EXCEPTION_SERVER_DEVICE_FAILURE;
        }
    }

    for (channel = 0U;
         channel < PRODUCT_MODBUS_DAC_CHANNEL_COUNT;
         channel++)
    {
        uint16_t channelMask = (uint16_t)(1UL << channel);
        if ((effectiveMask & channelMask) != 0U)
        {
            registerContext->activeDacCode[channel] =
                registerContext->pendingDacCode[channel];
        }
    }
    registerContext->dacPendingMask = 0U;
    registerContext->dacConfigurationRevision++;
    if (registerContext->dacConfigurationRevision == 0U)
    {
        registerContext->dacConfigurationRevision = 1U;
    }
    registerContext->dacStatus = PRODUCT_MODBUS_DAC_STATUS_READY;
    registerContext->dacFailedChannel =
        PRODUCT_MODBUS_DAC_FAILED_CHANNEL_NONE;
    return MODBUS_EXCEPTION_NONE;
}

static ModbusExceptionCode_t StageDigitalOutput(
    product_modbus_register_context_t *registerContext,
    uint16_t address,
    uint16_t value)
{
    uint16_t channel = (uint16_t)(address - PRODUCT_MODBUS_DO_BASE_ADDRESS);

    if (channel >= PRODUCT_MODBUS_DO_CHANNEL_COUNT)
    {
        return MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS;
    }
    if (value > 1U)
    {
        return MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE;
    }
    registerContext->pendingDigitalOutput[channel] = value;
    registerContext->digitalOutputPendingMask |=
        (uint16_t)(1UL << channel);
    return MODBUS_EXCEPTION_NONE;
}

static ModbusExceptionCode_t ApplyPendingDigitalOutputs(
    product_modbus_register_context_t *registerContext,
    uint16_t applyKey)
{
    DigitalOutputState_t state;
    uint16_t targetMask;
    uint8_t channel;

    if ((applyKey != PRODUCT_MODBUS_DO_APPLY_KEY_VALUE) ||
        (registerContext->digitalOutputPendingMask == 0U))
    {
        return MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE;
    }
    if (!DigitalOutputService_GetState(&state))
    {
        return MODBUS_EXCEPTION_SERVER_DEVICE_FAILURE;
    }
    targetMask = state.active_mask;
    for (channel = 0U; channel < PRODUCT_MODBUS_DO_CHANNEL_COUNT; channel++)
    {
        uint16_t channelMask = (uint16_t)(1UL << channel);
        if ((registerContext->digitalOutputPendingMask & channelMask) != 0U)
        {
            if (registerContext->pendingDigitalOutput[channel] != 0U)
            {
                targetMask |= channelMask;
            }
            else
            {
                targetMask &= (uint16_t)~channelMask;
            }
        }
    }
    if (DigitalOutputService_SetMask(targetMask) != DIGITAL_OUTPUT_STATUS_OK)
    {
        return MODBUS_EXCEPTION_SERVER_DEVICE_FAILURE;
    }
    registerContext->digitalOutputPendingMask = 0U;
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
    uint8_t temperatureChannel;
    uint16_t temperatureOffset;

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
    if (address == PRODUCT_MODBUS_FACTORY_FRAM_COMMAND_ADDRESS)
    {
        return ExecuteFactoryFramCommand(value);
    }

    if (address == PRODUCT_MODBUS_DIAGNOSTICS_FAULT_INDEX_ADDRESS)
    {
        if (value >= FaultService_GetActiveCount())
        {
            return MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE;
        }
        registerContext->diagnosticFaultIndex = value;
        return MODBUS_EXCEPTION_NONE;
    }

    if ((address >= PRODUCT_MODBUS_PWM_BASE_ADDRESS) &&
        (address < PRODUCT_MODBUS_PWM_APPLY_KEY_ADDRESS))
    {
        return StagePwmRegister(registerContext, address, value);
    }
    if (address == PRODUCT_MODBUS_PWM_APPLY_KEY_ADDRESS)
    {
        return ApplyPendingPwmConfiguration(registerContext, value);
    }
    if ((address >= PRODUCT_MODBUS_DAC_BASE_ADDRESS) &&
        (address < PRODUCT_MODBUS_DAC_APPLY_KEY_ADDRESS))
    {
        return StageDacRegister(registerContext, address, value);
    }
    if (address == PRODUCT_MODBUS_DAC_APPLY_KEY_ADDRESS)
    {
        return ApplyPendingDacCodes(registerContext, value);
    }
    if ((address >= PRODUCT_MODBUS_DO_BASE_ADDRESS) &&
        (address < PRODUCT_MODBUS_DO_APPLY_KEY_ADDRESS))
    {
        return StageDigitalOutput(registerContext, address, value);
    }
    if (address == PRODUCT_MODBUS_DO_APPLY_KEY_ADDRESS)
    {
        return ApplyPendingDigitalOutputs(registerContext, value);
    }

    if (!ResolveTemperatureRegisterRange(address, 1U, &temperatureChannel,
                                         &temperatureOffset))
    {
        return MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS;
    }

    if (temperatureOffset == 7U)
    {
        if (!IsSensorTypeValid(value))
        {
            return MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE;
        }
        registerContext->pendingConfig[temperatureChannel].sensorType = value;
        registerContext->pendingDirty[temperatureChannel] = true;
        return MODBUS_EXCEPTION_NONE;
    }

    if (temperatureOffset == 8U)
    {
        return MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS;
    }

    if (temperatureOffset == 9U)
    {
        return ApplyPendingConfiguration(registerContext, temperatureChannel,
                                         value);
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
    uint8_t temperatureChannel;
    uint16_t temperatureOffset;

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

    if (IsPwmRangeValid(starting_address, quantity))
    {
        product_modbus_register_context_t stagedContext = *registerContext;
        bool applyRequested = false;
        uint16_t index;

        for (index = 0U; index < quantity; index++)
        {
            uint16_t address = (uint16_t)(starting_address + index);
            ModbusExceptionCode_t result;

            if (address < PRODUCT_MODBUS_PWM_APPLY_KEY_ADDRESS)
            {
                result = StagePwmRegister(
                    &stagedContext, address, values[index]);
                if (result != MODBUS_EXCEPTION_NONE)
                {
                    return result;
                }
            }
            else if ((address == PRODUCT_MODBUS_PWM_APPLY_KEY_ADDRESS) &&
                     (index == (uint16_t)(quantity - 1U)) &&
                     (values[index] == PRODUCT_MODBUS_PWM_APPLY_KEY_VALUE))
            {
                applyRequested = true;
            }
            else
            {
                return (address <= PRODUCT_MODBUS_PWM_APPLY_KEY_ADDRESS) ?
                    MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE :
                    MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS;
            }
        }

        (void)memcpy(registerContext->pendingPwmConfig,
                     stagedContext.pendingPwmConfig,
                     sizeof(registerContext->pendingPwmConfig));
        registerContext->pwmPendingMask = stagedContext.pwmPendingMask;
        if (applyRequested)
        {
            return ApplyPendingPwmConfiguration(
                registerContext, PRODUCT_MODBUS_PWM_APPLY_KEY_VALUE);
        }
        return MODBUS_EXCEPTION_NONE;
    }

    if (IsDacRangeValid(starting_address, quantity))
    {
        product_modbus_register_context_t stagedContext = *registerContext;
        bool applyRequested = false;
        uint16_t index;

        for (index = 0U; index < quantity; index++)
        {
            uint16_t address = (uint16_t)(starting_address + index);
            ModbusExceptionCode_t result;

            if (address < PRODUCT_MODBUS_DAC_APPLY_KEY_ADDRESS)
            {
                result = StageDacRegister(
                    &stagedContext, address, values[index]);
                if (result != MODBUS_EXCEPTION_NONE)
                {
                    return result;
                }
            }
            else if ((address == PRODUCT_MODBUS_DAC_APPLY_KEY_ADDRESS) &&
                     (index == (uint16_t)(quantity - 1U)) &&
                     (values[index] == PRODUCT_MODBUS_DAC_APPLY_KEY_VALUE))
            {
                applyRequested = true;
            }
            else
            {
                return (address <= PRODUCT_MODBUS_DAC_APPLY_KEY_ADDRESS) ?
                    MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE :
                    MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS;
            }
        }

        (void)memcpy(registerContext->pendingDacCode,
                     stagedContext.pendingDacCode,
                     sizeof(registerContext->pendingDacCode));
        registerContext->dacPendingMask = stagedContext.dacPendingMask;
        if (applyRequested)
        {
            return ApplyPendingDacCodes(
                registerContext, PRODUCT_MODBUS_DAC_APPLY_KEY_VALUE);
        }
        return MODBUS_EXCEPTION_NONE;
    }

    if (IsDigitalOutputRangeValid(starting_address, quantity))
    {
        product_modbus_register_context_t stagedContext = *registerContext;
        bool applyRequested = false;
        uint16_t index;

        for (index = 0U; index < quantity; index++)
        {
            uint16_t address = (uint16_t)(starting_address + index);
            ModbusExceptionCode_t result;

            if (address < PRODUCT_MODBUS_DO_APPLY_KEY_ADDRESS)
            {
                result = StageDigitalOutput(
                    &stagedContext, address, values[index]);
                if (result != MODBUS_EXCEPTION_NONE)
                {
                    return result;
                }
            }
            else if ((address == PRODUCT_MODBUS_DO_APPLY_KEY_ADDRESS) &&
                     (index == (uint16_t)(quantity - 1U)) &&
                     (values[index] == PRODUCT_MODBUS_DO_APPLY_KEY_VALUE))
            {
                applyRequested = true;
            }
            else
            {
                return (address <= PRODUCT_MODBUS_DO_APPLY_KEY_ADDRESS) ?
                    MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE :
                    MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS;
            }
        }

        (void)memcpy(registerContext->pendingDigitalOutput,
                     stagedContext.pendingDigitalOutput,
                     sizeof(registerContext->pendingDigitalOutput));
        registerContext->digitalOutputPendingMask =
            stagedContext.digitalOutputPendingMask;
        if (applyRequested)
        {
            return ApplyPendingDigitalOutputs(
                registerContext, PRODUCT_MODBUS_DO_APPLY_KEY_VALUE);
        }
        return MODBUS_EXCEPTION_NONE;
    }

    if ((starting_address == PRODUCT_MODBUS_FACTORY_CAL_UNLOCK1_ADDRESS) &&
        (quantity == 2U))
    {
        FactoryCalibrationService_SetUnlockKey1(values[0]);
        FactoryCalibrationService_SetUnlockKey2(values[1]);
        return MODBUS_EXCEPTION_NONE;
    }

    if ((starting_address == PRODUCT_MODBUS_FACTORY_FRAM_COMMAND_ADDRESS) &&
        (quantity == 1U))
    {
        return ExecuteFactoryFramCommand(values[0]);
    }

    if ((starting_address == PRODUCT_MODBUS_DIAGNOSTICS_CLEAR_CODE_ADDRESS) &&
        (quantity == 2U))
    {
        FaultCode_t code = (FaultCode_t)values[0];
        if ((values[1] != PRODUCT_DIAGNOSTICS_CLEAR_KEY_VALUE) ||
            !FaultService_IsActive(code))
        {
            return MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE;
        }
        if (!FaultService_Clear(code))
        {
            return MODBUS_EXCEPTION_SERVER_DEVICE_FAILURE;
        }
        if (registerContext->diagnosticFaultIndex >=
            FaultService_GetActiveCount())
        {
            registerContext->diagnosticFaultIndex = 0U;
        }
        return MODBUS_EXCEPTION_NONE;
    }

    if ((starting_address == PRODUCT_MODBUS_DIAGNOSTICS_FAULT_INDEX_ADDRESS) &&
        (quantity == 1U))
    {
        if (values[0] >= FaultService_GetActiveCount())
        {
            return MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE;
        }
        registerContext->diagnosticFaultIndex = values[0];
        return MODBUS_EXCEPTION_NONE;
    }

    if (!ResolveTemperatureRegisterRange(starting_address, quantity,
                                         &temperatureChannel,
                                         &temperatureOffset))
    {
        return MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS;
    }
    pendingConfig = registerContext->pendingConfig[temperatureChannel];

    if ((temperatureOffset == 3U) && (quantity == 2U))
    {
        filterTimeConstant = RegistersToFloat(values[0], values[1]);
        if (!IsFilterTimeConstantValid(filterTimeConstant))
        {
            return MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE;
        }
        pendingConfig.filterTimeConstantSeconds = filterTimeConstant;
    }
    else if ((temperatureOffset == 7U) && (quantity == 1U))
    {
        if (!IsSensorTypeValid(values[0]))
        {
            return MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE;
        }
        pendingConfig.sensorType = values[0];
    }
    else if ((temperatureOffset == 7U) && (quantity == 3U))
    {
        ModbusExceptionCode_t applyResult;
        if (!IsSensorTypeValid(values[0]) || (values[1] != 0U))
        {
            return MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE;
        }
        pendingConfig.sensorType = values[0];
        registerContext->pendingConfig[temperatureChannel] = pendingConfig;
        registerContext->pendingDirty[temperatureChannel] = true;
        applyResult = ApplyPendingConfiguration(
            registerContext, temperatureChannel, values[2]);
        return applyResult;
    }
    else if ((temperatureOffset == 9U) && (quantity == 1U))
    {
        return ApplyPendingConfiguration(registerContext, temperatureChannel,
                                         values[0]);
    }
    else
    {
        return MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS;
    }

    registerContext->pendingConfig[temperatureChannel] = pendingConfig;
    registerContext->pendingDirty[temperatureChannel] = true;
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
    return ProductModbusRegisterAdapter_SetTemperatureInputMonitorForChannel(
        0U, monitor);
}

bool ProductModbusRegisterAdapter_SetTemperatureInputMonitorForChannel(
    uint8_t channel,
    const product_temperature_input_monitor_t *monitor)
{
    if ((channel >= PRODUCT_MODBUS_TEMPERATURE_INPUT_COUNT) ||
        (monitor == NULL) ||
        !IsTemperatureValueValid(monitor->unfilteredProcessValue) ||
        !IsTemperatureValueValid(monitor->filteredProcessValue))
    {
        return false;
    }

    s_registerContext.monitor[channel] = *monitor;
    return true;
}

void ProductModbusRegisterAdapter_GetTemperatureInputConfig(
    product_temperature_input_config_t *config)
{
    (void)ProductModbusRegisterAdapter_GetTemperatureInputConfigForChannel(
        0U, config);
}

bool ProductModbusRegisterAdapter_GetTemperatureInputConfigForChannel(
    uint8_t channel,
    product_temperature_input_config_t *config)
{
    if ((channel >= PRODUCT_MODBUS_TEMPERATURE_INPUT_COUNT) ||
        (config == NULL))
    {
        return false;
    }
    *config = s_registerContext.activeConfig[channel];
    return true;
}

uint16_t ProductModbusRegisterAdapter_GetTemperatureInputConfigurationRevision(
    void)
{
    return
        ProductModbusRegisterAdapter_GetTemperatureInputConfigurationRevisionForChannel(
            0U);
}

uint16_t
ProductModbusRegisterAdapter_GetTemperatureInputConfigurationRevisionForChannel(
    uint8_t channel)
{
    return (channel < PRODUCT_MODBUS_TEMPERATURE_INPUT_COUNT) ?
        s_registerContext.configurationRevision[channel] : 0U;
}

bool ProductModbusRegisterAdapter_RestoreTemperatureInputConfig(
    const product_temperature_input_config_t *config,
    uint16_t configuration_revision)
{
    return ProductModbusRegisterAdapter_RestoreTemperatureInputConfigForChannel(
            0U, config, configuration_revision);
}

bool ProductModbusRegisterAdapter_RestoreTemperatureInputConfigForChannel(
    uint8_t channel,
    const product_temperature_input_config_t *config,
    uint16_t configuration_revision)
{
    if ((channel >= PRODUCT_MODBUS_TEMPERATURE_INPUT_COUNT) ||
        (config == NULL) || (configuration_revision == 0U) ||
        !IsFilterTimeConstantValid(config->filterTimeConstantSeconds) ||
        !IsSensorTypeValid(config->sensorType))
    {
        return false;
    }
    s_registerContext.activeConfig[channel] = *config;
    s_registerContext.pendingConfig[channel] = *config;
    s_registerContext.configurationRevision[channel] =
        configuration_revision;
    s_registerContext.pendingDirty[channel] = false;
    return true;
}

bool ProductModbusRegisterAdapter_HasPendingTemperatureInputConfig(void)
{
    return
        ProductModbusRegisterAdapter_HasPendingTemperatureInputConfigForChannel(
            0U);
}

bool ProductModbusRegisterAdapter_HasPendingTemperatureInputConfigForChannel(
    uint8_t channel)
{
    return (channel < PRODUCT_MODBUS_TEMPERATURE_INPUT_COUNT) &&
           s_registerContext.pendingDirty[channel];
}

void ProductModbusRegisterAdapter_GetPendingTemperatureInputConfig(
    product_temperature_input_config_t *config)
{
    (void)ProductModbusRegisterAdapter_GetPendingTemperatureInputConfigForChannel(
        0U, config);
}

bool ProductModbusRegisterAdapter_GetPendingTemperatureInputConfigForChannel(
    uint8_t channel,
    product_temperature_input_config_t *config)
{
    if ((channel >= PRODUCT_MODBUS_TEMPERATURE_INPUT_COUNT) ||
        (config == NULL))
    {
        return false;
    }
    *config = s_registerContext.pendingConfig[channel];
    return true;
}

void ProductModbusRegisterAdapter_DiscardPendingTemperatureInputConfig(void)
{
    ProductModbusRegisterAdapter_DiscardPendingTemperatureInputConfigForChannel(
        0U);
}

void ProductModbusRegisterAdapter_DiscardPendingTemperatureInputConfigForChannel(
    uint8_t channel)
{
    if (channel < PRODUCT_MODBUS_TEMPERATURE_INPUT_COUNT)
    {
        s_registerContext.pendingConfig[channel] =
            s_registerContext.activeConfig[channel];
        s_registerContext.pendingDirty[channel] = false;
    }
}

bool ProductModbusRegisterAdapter_GetPwmConfig(
    uint8_t channel,
    product_pwm_output_config_t *config)
{
    if ((channel >= PRODUCT_MODBUS_PWM_CHANNEL_COUNT) || (config == NULL))
    {
        return false;
    }
    *config = s_registerContext.activePwmConfig[channel];
    return true;
}

bool ProductModbusRegisterAdapter_GetPendingPwmConfig(
    uint8_t channel,
    product_pwm_output_config_t *config)
{
    if ((channel >= PRODUCT_MODBUS_PWM_CHANNEL_COUNT) || (config == NULL))
    {
        return false;
    }
    *config = s_registerContext.pendingPwmConfig[channel];
    return true;
}

uint16_t ProductModbusRegisterAdapter_GetPwmConfigurationRevision(void)
{
    return s_registerContext.pwmConfigurationRevision;
}

uint16_t ProductModbusRegisterAdapter_GetPwmPendingMask(void)
{
    return s_registerContext.pwmPendingMask;
}

void ProductModbusRegisterAdapter_DiscardPendingPwmConfig(void)
{
    (void)memcpy(s_registerContext.pendingPwmConfig,
                 s_registerContext.activePwmConfig,
                 sizeof(s_registerContext.pendingPwmConfig));
    s_registerContext.pwmPendingMask = 0U;
}

bool ProductModbusRegisterAdapter_GetDacCode(
    uint8_t channel,
    uint16_t *code)
{
    if ((channel >= PRODUCT_MODBUS_DAC_CHANNEL_COUNT) || (code == NULL))
    {
        return false;
    }
    *code = s_registerContext.activeDacCode[channel];
    return true;
}

bool ProductModbusRegisterAdapter_GetPendingDacCode(
    uint8_t channel,
    uint16_t *code)
{
    if ((channel >= PRODUCT_MODBUS_DAC_CHANNEL_COUNT) || (code == NULL))
    {
        return false;
    }
    *code = s_registerContext.pendingDacCode[channel];
    return true;
}

uint16_t ProductModbusRegisterAdapter_GetDacConfigurationRevision(void)
{
    return s_registerContext.dacConfigurationRevision;
}

uint16_t ProductModbusRegisterAdapter_GetDacPendingMask(void)
{
    return s_registerContext.dacPendingMask;
}

uint16_t ProductModbusRegisterAdapter_GetDacStatus(void)
{
    return s_registerContext.dacStatus;
}

uint16_t ProductModbusRegisterAdapter_GetDacFailedChannel(void)
{
    return s_registerContext.dacFailedChannel;
}

void ProductModbusRegisterAdapter_DiscardPendingDacCodes(void)
{
    (void)memcpy(s_registerContext.pendingDacCode,
                 s_registerContext.activeDacCode,
                 sizeof(s_registerContext.pendingDacCode));
    s_registerContext.dacPendingMask = 0U;
}

void ProductModbusRegisterAdapter_SetLowVoltageMonitor(
    const product_low_voltage_monitor_t *monitor)
{
    if (monitor != NULL)
    {
        s_registerContext.lowVoltageMonitor = *monitor;
    }
}
