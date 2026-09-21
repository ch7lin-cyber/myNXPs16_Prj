/*
 * Copyright 2015-2019, 2023, 2026 Analog Devices, Inc.
 * Copyright 2026
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Freestanding adaptation of the Analog Devices no-OS AD7124 driver.
 */

#include "adi_ad7124_driver.h"

#include <string.h>

#define ADI_AD7124_COMM_READ              (1U << 6U)
#define ADI_AD7124_COMM_ADDRESS(x)        ((x) & 0x3FU)
#define ADI_AD7124_CRC8_POLYNOMIAL        (0x07U)
#define ADI_AD7124_RESET_BYTE_COUNT       (8U)
#define ADI_AD7124_POST_RESET_DELAY_MS    (4U)
#define ADI_AD7124_MAX_TRANSACTION_SIZE   (8U)

static adi_ad7124_status_t Transfer(
    adi_ad7124_device_t *device,
    uint8_t *buffer,
    size_t length)
{
    if ((device == NULL) || (device->transfer == NULL) ||
        (buffer == NULL) || (length == 0U) ||
        (length > ADI_AD7124_MAX_TRANSACTION_SIZE))
    {
        return kAdiAd7124_InvalidArgument;
    }

    return device->transfer(device->transportContext, buffer, length) ?
           kAdiAd7124_Ok : kAdiAd7124_TransportError;
}

static adi_ad7124_status_t ReadRegisterUnchecked(
    adi_ad7124_device_t *device,
    uint8_t address,
    uint32_t *value)
{
    uint8_t buffer[ADI_AD7124_MAX_TRANSACTION_SIZE] = {0U};
    uint8_t registerSize = ADI_AD7124_GetRegisterSize(address);
    size_t transferSize;
    uint8_t index;
    adi_ad7124_status_t status;

    if ((device == NULL) || (value == NULL))
    {
        return kAdiAd7124_InvalidArgument;
    }
    if (registerSize == 0U)
    {
        return kAdiAd7124_InvalidRegister;
    }

    buffer[0] = ADI_AD7124_COMM_READ | ADI_AD7124_COMM_ADDRESS(address);
    transferSize = 1U + (size_t)registerSize +
                   (device->crcEnabled ? 1U : 0U);
    status = Transfer(device, buffer, transferSize);
    if (status != kAdiAd7124_Ok)
    {
        return status;
    }

    if (device->crcEnabled &&
        (ADI_AD7124_ComputeCrc8(buffer, transferSize) != 0U))
    {
        return kAdiAd7124_CrcError;
    }

    *value = 0U;
    for (index = 1U; index <= registerSize; index++)
    {
        *value = (*value << 8U) | (uint32_t)buffer[index];
    }
    return kAdiAd7124_Ok;
}

static adi_ad7124_status_t WaitPowerOn(adi_ad7124_device_t *device)
{
    uint32_t statusValue;
    uint32_t count;
    uint32_t pollLimit;
    adi_ad7124_status_t status;

    pollLimit = (device->pollLimit == 0U) ?
                ADI_AD7124_DEFAULT_POLL_LIMIT : device->pollLimit;
    for (count = 0U; count < pollLimit; count++)
    {
        status = ReadRegisterUnchecked(device, ADI_AD7124_STATUS_REG,
                                       &statusValue);
        if (status != kAdiAd7124_Ok)
        {
            return status;
        }
        if ((statusValue & ADI_AD7124_STATUS_POR_MASK) == 0U)
        {
            return kAdiAd7124_Ok;
        }
    }
    return kAdiAd7124_Timeout;
}

uint8_t ADI_AD7124_GetRegisterSize(uint8_t address)
{
    if ((address == ADI_AD7124_STATUS_REG) ||
        (address == ADI_AD7124_ID_REG) ||
        (address == ADI_AD7124_MCLK_COUNT_REG))
    {
        return 1U;
    }
    if ((address == ADI_AD7124_ADC_CONTROL_REG) ||
        (address == ADI_AD7124_IO_CONTROL2_REG) ||
        ((address >= ADI_AD7124_CHANNEL0_REG) &&
         (address <= ADI_AD7124_CONFIG7_REG)))
    {
        return 2U;
    }
    if ((address == ADI_AD7124_DATA_REG) ||
        (address == ADI_AD7124_IO_CONTROL1_REG) ||
        (address == ADI_AD7124_ERROR_REG) ||
        (address == ADI_AD7124_ERROR_ENABLE_REG) ||
        ((address >= ADI_AD7124_FILTER0_REG) &&
         (address <= ADI_AD7124_GAIN7_REG)))
    {
        return 3U;
    }
    return 0U;
}

uint8_t ADI_AD7124_ComputeCrc8(const uint8_t *data, size_t length)
{
    uint8_t crc = 0U;
    size_t index;
    uint8_t bit;

    if (data == NULL)
    {
        return 0U;
    }

    for (index = 0U; index < length; index++)
    {
        crc ^= data[index];
        for (bit = 0U; bit < 8U; bit++)
        {
            crc = ((crc & 0x80U) != 0U) ?
                  (uint8_t)((crc << 1U) ^ ADI_AD7124_CRC8_POLYNOMIAL) :
                  (uint8_t)(crc << 1U);
        }
    }
    return crc;
}

bool ADI_AD7124_IsKnownDeviceId(uint8_t id, adi_ad7124_variant_t variant)
{
    bool isVariant4 = (id == ADI_AD7124_ID_4_STANDARD) ||
                      (id == ADI_AD7124_ID_4_B_GRADE) ||
                      (id == ADI_AD7124_ID_4_NEW);
    bool isVariant8 = (id == ADI_AD7124_ID_8_STANDARD) ||
                      (id == ADI_AD7124_ID_8_B_W_GRADE) ||
                      (id == ADI_AD7124_ID_8_NEW);

    if (variant == kAdiAd7124_Variant4)
    {
        return isVariant4;
    }
    if (variant == kAdiAd7124_Variant8)
    {
        return isVariant8;
    }
    return isVariant4 || isVariant8;
}

adi_ad7124_status_t ADI_AD7124_Reset(adi_ad7124_device_t *device)
{
    uint8_t buffer[ADI_AD7124_RESET_BYTE_COUNT];
    adi_ad7124_status_t status;

    if (device == NULL)
    {
        return kAdiAd7124_InvalidArgument;
    }

    (void)memset(buffer, 0xFF, sizeof(buffer));
    status = Transfer(device, buffer, sizeof(buffer));
    if (status != kAdiAd7124_Ok)
    {
        return status;
    }

    device->crcEnabled = false;
    device->initialized = false;
    if (device->delayMs != NULL)
    {
        device->delayMs(device->transportContext,
                        ADI_AD7124_POST_RESET_DELAY_MS);
    }
    return kAdiAd7124_Ok;
}

adi_ad7124_status_t ADI_AD7124_WaitReady(adi_ad7124_device_t *device)
{
    uint32_t statusValue;
    uint32_t count;
    uint32_t pollLimit;
    adi_ad7124_status_t status;

    if (device == NULL)
    {
        return kAdiAd7124_InvalidArgument;
    }

    pollLimit = (device->pollLimit == 0U) ?
                ADI_AD7124_DEFAULT_POLL_LIMIT : device->pollLimit;
    for (count = 0U; count < pollLimit; count++)
    {
        status = ReadRegisterUnchecked(device, ADI_AD7124_STATUS_REG,
                                       &statusValue);
        if (status != kAdiAd7124_Ok)
        {
            return status;
        }
        if ((statusValue & ADI_AD7124_STATUS_RDY_MASK) == 0U)
        {
            return kAdiAd7124_Ok;
        }
    }
    return kAdiAd7124_Timeout;
}

adi_ad7124_status_t ADI_AD7124_ReadRegister(
    adi_ad7124_device_t *device,
    uint8_t address,
    uint32_t *value)
{
    if ((device == NULL) || (value == NULL))
    {
        return kAdiAd7124_InvalidArgument;
    }
    return ReadRegisterUnchecked(device, address, value);
}

adi_ad7124_status_t ADI_AD7124_WriteRegister(
    adi_ad7124_device_t *device,
    uint8_t address,
    uint32_t value)
{
    uint8_t buffer[ADI_AD7124_MAX_TRANSACTION_SIZE] = {0U};
    uint8_t registerSize = ADI_AD7124_GetRegisterSize(address);
    size_t transferSize;
    uint8_t index;
    uint32_t requestedValue = value;
    adi_ad7124_status_t status;

    if (device == NULL)
    {
        return kAdiAd7124_InvalidArgument;
    }
    if (registerSize == 0U)
    {
        return kAdiAd7124_InvalidRegister;
    }

    buffer[0] = ADI_AD7124_COMM_ADDRESS(address);
    for (index = 0U; index < registerSize; index++)
    {
        buffer[registerSize - index] = (uint8_t)(value & 0xFFU);
        value >>= 8U;
    }
    transferSize = 1U + (size_t)registerSize;
    if (device->crcEnabled)
    {
        buffer[transferSize] = ADI_AD7124_ComputeCrc8(buffer, transferSize);
        transferSize++;
    }

    status = Transfer(device, buffer, transferSize);
    if ((status == kAdiAd7124_Ok) &&
        (address == ADI_AD7124_ERROR_ENABLE_REG))
    {
        device->crcEnabled =
            ((requestedValue & ADI_AD7124_ERROR_ENABLE_CRC_MASK) != 0U);
    }
    return status;
}

adi_ad7124_status_t ADI_AD7124_ReadData(
    adi_ad7124_device_t *device,
    uint32_t *code,
    uint8_t *channel)
{
    uint32_t statusValue;
    adi_ad7124_status_t status;

    if ((device == NULL) || (code == NULL))
    {
        return kAdiAd7124_InvalidArgument;
    }

    status = ADI_AD7124_WaitReady(device);
    if (status != kAdiAd7124_Ok)
    {
        return status;
    }

    status = ReadRegisterUnchecked(device, ADI_AD7124_DATA_REG, code);
    if ((status == kAdiAd7124_Ok) && (channel != NULL))
    {
        status = ReadRegisterUnchecked(device, ADI_AD7124_STATUS_REG,
                                       &statusValue);
        if (status == kAdiAd7124_Ok)
        {
            *channel = (uint8_t)(statusValue &
                                 ADI_AD7124_STATUS_CHANNEL_MASK);
        }
    }
    return status;
}

adi_ad7124_status_t ADI_AD7124_Init(adi_ad7124_device_t *device)
{
    uint32_t idValue;
    adi_ad7124_status_t status;

    if ((device == NULL) || (device->transfer == NULL))
    {
        return kAdiAd7124_InvalidArgument;
    }

    status = ADI_AD7124_Reset(device);
    if (status != kAdiAd7124_Ok)
    {
        return status;
    }
    status = WaitPowerOn(device);
    if (status != kAdiAd7124_Ok)
    {
        return status;
    }
    status = ReadRegisterUnchecked(device, ADI_AD7124_ID_REG, &idValue);
    if (status != kAdiAd7124_Ok)
    {
        return status;
    }

    device->deviceId = (uint8_t)idValue;
    if (!ADI_AD7124_IsKnownDeviceId(device->deviceId,
                                    device->expectedVariant))
    {
        return kAdiAd7124_UnexpectedDevice;
    }
    device->initialized = true;
    return kAdiAd7124_Ok;
}
