/*
 * Copyright 2026
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef PRODUCT_AD7124_DRIVER_H_
#define PRODUCT_AD7124_DRIVER_H_

#include <stdint.h>

#include "adi_ad7124_driver.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PRODUCT_AD7124_DEVICE_COUNT (4U)

adi_ad7124_status_t ProductAd7124_InitDevice(uint8_t deviceIndex);
adi_ad7124_status_t ProductAd7124_InitAll(void);
adi_ad7124_device_t *ProductAd7124_GetDevice(uint8_t deviceIndex);

#ifdef __cplusplus
}
#endif

#endif /* PRODUCT_AD7124_DRIVER_H_ */
