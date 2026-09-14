/*
 * Copyright 2026
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "product_modbus_register_adapter.h"

#include <stddef.h>

static ModbusExceptionCode_t ReadRegisters(
    void *context,
    uint16_t starting_address,
    uint16_t quantity,
    uint16_t *values)
{
    (void)context;
    (void)starting_address;
    (void)quantity;
    (void)values;

    return MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS;
}

static ModbusExceptionCode_t WriteSingleRegister(
    void *context,
    uint16_t address,
    uint16_t value)
{
    (void)context;
    (void)address;
    (void)value;

    return MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS;
}

static ModbusExceptionCode_t WriteMultipleRegisters(
    void *context,
    uint16_t starting_address,
    const uint16_t *values,
    uint16_t quantity)
{
    (void)context;
    (void)starting_address;
    (void)values;
    (void)quantity;

    return MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS;
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
        interface->context = NULL;
    }
}
