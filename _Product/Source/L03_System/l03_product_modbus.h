/*
 * Copyright 2026
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef L03_PRODUCT_MODBUS_H_
#define L03_PRODUCT_MODBUS_H_

#include <stdbool.h>
#include <stdint.h>

#include "fsl_common.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _l03_product_modbus_statistics
{
    uint32_t receivedFrames;
    uint32_t transmittedResponses;
    uint32_t ignoredFrames;
    uint32_t crcErrors;
    uint32_t lrcErrors;
    uint32_t rtuInterCharacterErrors;
    uint32_t transportErrors;
} l03_product_modbus_statistics_t;

status_t L03_ProductModbus_Init(void);
void L03_ProductModbus_Process(void);
status_t L03_ProductModbus_GetStatistics(
    l03_product_modbus_statistics_t *statistics);
bool L03_ProductModbus_IsInitialized(void);

#ifdef __cplusplus
}
#endif

#endif /* L03_PRODUCT_MODBUS_H_ */
