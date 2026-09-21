/*
 * Copyright 2015-2019, 2023, 2026 Analog Devices, Inc.
 * Copyright 2026
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * This driver is a freestanding LPC55S16-oriented adaptation of the
 * Analog Devices no-OS AD7124 driver. Devices: AD7124-4 and AD7124-8.
 */

#ifndef ADI_AD7124_DRIVER_H_
#define ADI_AD7124_DRIVER_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ADI_AD7124_STATUS_REG             (0x00U)
#define ADI_AD7124_ADC_CONTROL_REG        (0x01U)
#define ADI_AD7124_DATA_REG               (0x02U)
#define ADI_AD7124_IO_CONTROL1_REG        (0x03U)
#define ADI_AD7124_IO_CONTROL2_REG        (0x04U)
#define ADI_AD7124_ID_REG                 (0x05U)
#define ADI_AD7124_ERROR_REG              (0x06U)
#define ADI_AD7124_ERROR_ENABLE_REG       (0x07U)
#define ADI_AD7124_MCLK_COUNT_REG         (0x08U)
#define ADI_AD7124_CHANNEL0_REG           (0x09U)
#define ADI_AD7124_CHANNEL15_REG          (0x18U)
#define ADI_AD7124_CONFIG0_REG            (0x19U)
#define ADI_AD7124_CONFIG7_REG            (0x20U)
#define ADI_AD7124_FILTER0_REG            (0x21U)
#define ADI_AD7124_FILTER7_REG            (0x28U)
#define ADI_AD7124_OFFSET0_REG            (0x29U)
#define ADI_AD7124_OFFSET7_REG            (0x30U)
#define ADI_AD7124_GAIN0_REG              (0x31U)
#define ADI_AD7124_GAIN7_REG              (0x38U)

#define ADI_AD7124_STATUS_RDY_MASK        (1UL << 7U)
#define ADI_AD7124_STATUS_ERROR_MASK      (1UL << 6U)
#define ADI_AD7124_STATUS_POR_MASK        (1UL << 4U)
#define ADI_AD7124_STATUS_CHANNEL_MASK    (0x0FUL)
#define ADI_AD7124_ERROR_ENABLE_CRC_MASK  (1UL << 2U)

#define ADI_AD7124_ID_4_STANDARD          (0x04U)
#define ADI_AD7124_ID_4_B_GRADE           (0x06U)
#define ADI_AD7124_ID_4_NEW               (0x07U)
#define ADI_AD7124_ID_8_STANDARD          (0x14U)
#define ADI_AD7124_ID_8_B_W_GRADE         (0x16U)
#define ADI_AD7124_ID_8_NEW               (0x17U)

#define ADI_AD7124_DEFAULT_POLL_LIMIT     (10000UL)

typedef enum _adi_ad7124_status
{
    kAdiAd7124_Ok = 0,
    kAdiAd7124_InvalidArgument = -1,
    kAdiAd7124_TransportError = -2,
    kAdiAd7124_Timeout = -3,
    kAdiAd7124_CrcError = -4,
    kAdiAd7124_InvalidRegister = -5,
    kAdiAd7124_UnexpectedDevice = -6
} adi_ad7124_status_t;

typedef enum _adi_ad7124_variant
{
    kAdiAd7124_AnyVariant = 0,
    kAdiAd7124_Variant4,
    kAdiAd7124_Variant8
} adi_ad7124_variant_t;

/* Full-duplex, in-place SPI transfer. Return true on success. */
typedef bool (*adi_ad7124_transfer_fn_t)(
    void *context,
    uint8_t *data,
    size_t length);

typedef void (*adi_ad7124_delay_ms_fn_t)(void *context, uint32_t delayMs);

typedef struct _adi_ad7124_device
{
    adi_ad7124_transfer_fn_t transfer;
    adi_ad7124_delay_ms_fn_t delayMs;
    void *transportContext;
    uint32_t pollLimit;
    adi_ad7124_variant_t expectedVariant;
    uint8_t deviceId;
    bool crcEnabled;
    bool initialized;
} adi_ad7124_device_t;

adi_ad7124_status_t ADI_AD7124_Init(adi_ad7124_device_t *device);
adi_ad7124_status_t ADI_AD7124_Reset(adi_ad7124_device_t *device);
adi_ad7124_status_t ADI_AD7124_WaitReady(adi_ad7124_device_t *device);
adi_ad7124_status_t ADI_AD7124_ReadRegister(
    adi_ad7124_device_t *device,
    uint8_t address,
    uint32_t *value);
adi_ad7124_status_t ADI_AD7124_WriteRegister(
    adi_ad7124_device_t *device,
    uint8_t address,
    uint32_t value);
adi_ad7124_status_t ADI_AD7124_ReadData(
    adi_ad7124_device_t *device,
    uint32_t *code,
    uint8_t *channel);
uint8_t ADI_AD7124_ComputeCrc8(const uint8_t *data, size_t length);
uint8_t ADI_AD7124_GetRegisterSize(uint8_t address);
bool ADI_AD7124_IsKnownDeviceId(uint8_t id, adi_ad7124_variant_t variant);

#ifdef __cplusplus
}
#endif

#endif /* ADI_AD7124_DRIVER_H_ */
