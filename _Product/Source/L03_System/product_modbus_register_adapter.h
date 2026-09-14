/*
 * Copyright 2026
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef PRODUCT_MODBUS_REGISTER_ADAPTER_H_
#define PRODUCT_MODBUS_REGISTER_ADAPTER_H_

#include "ModbusSlave.h"

#ifdef __cplusplus
extern "C" {
#endif

/*!
 * @brief Build the product register callback table used by ModbusSlave.
 *
 * The current safe implementation returns ILLEGAL_DATA_ADDRESS for every
 * address because _Product/RegisterMap/register_map.csv is not populated yet.
 * Replace the callback bodies when the product register map is defined.
 */
void ProductModbusRegisterAdapter_GetInterface(
    ModbusSlaveRegisterInterface_t *interface);

#ifdef __cplusplus
}
#endif

#endif /* PRODUCT_MODBUS_REGISTER_ADAPTER_H_ */
