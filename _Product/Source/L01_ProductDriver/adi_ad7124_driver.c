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
#define ADI_AD7124_ADC_CONTROL_REF_EN      (1UL << 8U)
#define ADI_AD7124_ADC_CONTROL_MODE_MASK   (0xFFUL)
#define ADI_AD7124_ADC_CONTROL_FULL_POWER  (0x80UL)
#define ADI_AD7124_CHANNEL_ENABLE          (1UL << 15U)
#define ADI_AD7124_CONFIG_BIPOLAR          (1UL << 11U)
#define ADI_AD7124_CONFIG_REF_BUFP         (1UL << 8U)
#define ADI_AD7124_CONFIG_REF_BUFM         (1UL << 7U)
#define ADI_AD7124_CONFIG_AIN_BUFP         (1UL << 6U)
#define ADI_AD7124_CONFIG_AIN_BUFM         (1UL << 5U)
#define ADI_AD7124_FILTER_REJECT_60_HZ      (1UL << 20U)

static bool MapExcitationCurrent(uint16_t currentUa, uint8_t *code)
{
    if (code == NULL)
    {
        return false;
    }
    switch (currentUa)
    {
        case 0U:    *code = 0U; return true;
        case 50U:   *code = 1U; return true;
        case 100U:  *code = 2U; return true;
        case 250U:  *code = 3U; return true;
        case 500U:  *code = 4U; return true;
        case 750U:  *code = 5U; return true;
        case 1000U: *code = 6U; return true;
        default: return false;
    }
}

static bool IsExcitationOutputValid(uint8_t input)
{
    return (input == 0U) || (input == 1U) || (input == 2U) ||
           (input == 3U) || (input == 4U) || (input == 5U) ||
           (input == 6U) || (input == 7U);
}

static uint8_t MapExcitationOutput(uint8_t input)
{
    static const uint8_t outputCode[8] =
        {0U, 1U, 4U, 5U, 10U, 11U, 14U, 15U};
    return outputCode[input];
}

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
    uint8_t command;
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

    command = ADI_AD7124_COMM_READ | ADI_AD7124_COMM_ADDRESS(address);
    buffer[0] = command;
    transferSize = 1U + (size_t)registerSize +
                   (device->crcEnabled ? 1U : 0U);
    status = Transfer(device, buffer, transferSize);
    if (status != kAdiAd7124_Ok)
    {
        return status;
    }

    /* SPI is full duplex; restore the transmitted command for CRC checking. */
    buffer[0] = command;
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

adi_ad7124_status_t ADI_AD7124_TryReadData(
    adi_ad7124_device_t *device,
    uint32_t *code,
    uint8_t *channel)
{
    return ADI_AD7124_TryReadDataDiagnostic(
        device, code, channel, NULL, NULL, NULL);
}

adi_ad7124_status_t ADI_AD7124_TryReadDataDiagnostic(
    adi_ad7124_device_t *device,
    uint32_t *code,
    uint8_t *channel,
    uint8_t *statusRegister,
    uint32_t *errorRegister,
    bool *errorRegisterRead)
{
    uint32_t statusValue;
    uint32_t errorValue = 0U;
    adi_ad7124_status_t status;

    if ((device == NULL) || (code == NULL))
    {
        return kAdiAd7124_InvalidArgument;
    }

    status = ReadRegisterUnchecked(device, ADI_AD7124_STATUS_REG,
                                   &statusValue);
    if (status != kAdiAd7124_Ok)
    {
        return status;
    }
    if (statusRegister != NULL)
    {
        *statusRegister = (uint8_t)statusValue;
    }
    if (errorRegisterRead != NULL)
    {
        *errorRegisterRead = false;
    }
    if ((statusValue & ADI_AD7124_STATUS_ERROR_MASK) != 0U)
    {
        status = ReadRegisterUnchecked(device, ADI_AD7124_ERROR_REG,
                                       &errorValue);
        if (status != kAdiAd7124_Ok)
        {
            return status;
        }
        if (errorRegister != NULL)
        {
            *errorRegister = errorValue;
        }
        if (errorRegisterRead != NULL)
        {
            *errorRegisterRead = true;
        }
    }
    if ((statusValue & ADI_AD7124_STATUS_RDY_MASK) != 0U)
    {
        return kAdiAd7124_NotReady;
    }

    status = ReadRegisterUnchecked(device, ADI_AD7124_DATA_REG, code);
    if ((status == kAdiAd7124_Ok) && (channel != NULL))
    {
        *channel = (uint8_t)(statusValue &
                             ADI_AD7124_STATUS_CHANNEL_MASK);
    }
    return status;
}

adi_ad7124_status_t ADI_AD7124_Init(adi_ad7124_device_t *device)
{
    uint32_t idValue;
    uint32_t errorValue;
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
    /* Per product policy, ERROR is sampled once at initialization only. */
    status = ReadRegisterUnchecked(device, ADI_AD7124_ERROR_REG,
                                   &errorValue);
    if (status != kAdiAd7124_Ok)
    {
        return status;
    }
    device->initialError = errorValue;
    device->initialized = true;
    return kAdiAd7124_Ok;
}

adi_ad7124_status_t ADI_AD7124_EnableCrc(adi_ad7124_device_t *device)
{
    uint32_t errorEnable;
    uint32_t verify;
    adi_ad7124_status_t status;

    if ((device == NULL) || !device->initialized)
    {
        return kAdiAd7124_InvalidArgument;
    }

    status = ADI_AD7124_ReadRegister(device, ADI_AD7124_ERROR_ENABLE_REG,
                                     &errorEnable);
    if (status != kAdiAd7124_Ok)
    {
        return status;
    }
    errorEnable |= ADI_AD7124_ERROR_ENABLE_CRC_MASK;
    status = ADI_AD7124_WriteRegister(device, ADI_AD7124_ERROR_ENABLE_REG,
                                      errorEnable);
    if (status != kAdiAd7124_Ok)
    {
        return status;
    }

    status = ADI_AD7124_ReadRegister(device, ADI_AD7124_ERROR_ENABLE_REG,
                                     &verify);
    if ((status != kAdiAd7124_Ok) ||
        ((verify & ADI_AD7124_ERROR_ENABLE_CRC_MASK) == 0U))
    {
        return (status == kAdiAd7124_Ok) ? kAdiAd7124_CrcError : status;
    }
    return kAdiAd7124_Ok;
}

adi_ad7124_status_t ADI_AD7124_Configure(
    adi_ad7124_device_t *device,
    const adi_ad7124_setup_config_t *setups,
    uint8_t setupCount,
    const adi_ad7124_channel_config_t *channels,
    uint8_t channelCount,
    const adi_ad7124_io_config_t *ioConfig)
{
    uint32_t control;
    uint32_t value;
    uint8_t index;
    adi_ad7124_status_t status;
    bool internalReferenceRequired = false;
    uint8_t excitationCode;

    if ((device == NULL) || !device->initialized || (setups == NULL) ||
        (setupCount == 0U) || (setupCount > ADI_AD7124_MAX_SETUP_COUNT) ||
        (channels == NULL) || (channelCount == 0U) ||
        (channelCount > ADI_AD7124_MAX_CHANNEL_COUNT))
    {
        return kAdiAd7124_InvalidArgument;
    }

    if ((ioConfig == NULL) ||
        !MapExcitationCurrent(ioConfig->excitationCurrentUa,
                              &excitationCode) ||
        !IsExcitationOutputValid(ioConfig->excitationOutput0) ||
        !IsExcitationOutputValid(ioConfig->excitationOutput1))
    {
        return kAdiAd7124_InvalidArgument;
    }

    /* Burnout currents and voltage bias remain disabled. */
    internalReferenceRequired = ioConfig->referenceOutputRequired;
    value = ((uint32_t)excitationCode << 11U) |
            ((uint32_t)excitationCode << 8U) |
            ((uint32_t)MapExcitationOutput(ioConfig->excitationOutput1)
             << 4U) |
            (uint32_t)MapExcitationOutput(ioConfig->excitationOutput0);
    status = ADI_AD7124_WriteRegister(device, ADI_AD7124_IO_CONTROL1_REG,
                                      value);
    if (status != kAdiAd7124_Ok)
    {
        return status;
    }
    status = ADI_AD7124_WriteRegister(device, ADI_AD7124_IO_CONTROL2_REG, 0U);
    if (status != kAdiAd7124_Ok)
    {
        return status;
    }

    for (index = 0U; index < setupCount; index++)
    {
        const adi_ad7124_setup_config_t *setup = &setups[index];
        if ((setup->setup >= ADI_AD7124_MAX_SETUP_COUNT) ||
            (setup->reference > 3U) || (setup->gain > 7U) ||
            (setup->filter > 7U) || (setup->filterWord > 0x07FFU))
        {
            return kAdiAd7124_InvalidArgument;
        }
        value = setup->bipolar ? ADI_AD7124_CONFIG_BIPOLAR : 0U;
        if (setup->inputBufferEnabled)
        {
            value |= ADI_AD7124_CONFIG_AIN_BUFP |
                     ADI_AD7124_CONFIG_AIN_BUFM;
        }
        if (setup->referenceBufferEnabled)
        {
            value |= ADI_AD7124_CONFIG_REF_BUFP |
                     ADI_AD7124_CONFIG_REF_BUFM;
        }
        value |= ((uint32_t)setup->reference << 3U) |
                 (uint32_t)setup->gain;
        status = ADI_AD7124_WriteRegister(
            device, (uint8_t)(ADI_AD7124_CONFIG0_REG + setup->setup), value);
        if (status != kAdiAd7124_Ok)
        {
            return status;
        }
        value = ((uint32_t)setup->filter << 21U) |
                (uint32_t)setup->filterWord;
        if (setup->singleCycle)
        {
            value |= (1UL << 16U);
        }
        if (setup->reject60Hz)
        {
            value |= ADI_AD7124_FILTER_REJECT_60_HZ;
        }
        status = ADI_AD7124_WriteRegister(
            device, (uint8_t)(ADI_AD7124_FILTER0_REG + setup->setup), value);
        if (status != kAdiAd7124_Ok)
        {
            return status;
        }
        internalReferenceRequired |= (setup->reference == 2U);
    }

    /* Remove reset defaults and stale channel selections before applying map. */
    for (index = 0U; index < ADI_AD7124_MAX_CHANNEL_COUNT; index++)
    {
        status = ADI_AD7124_WriteRegister(
            device, (uint8_t)(ADI_AD7124_CHANNEL0_REG + index), 0U);
        if (status != kAdiAd7124_Ok)
        {
            return status;
        }
    }
    for (index = 0U; index < channelCount; index++)
    {
        const adi_ad7124_channel_config_t *channel = &channels[index];
        if ((channel->channel >= ADI_AD7124_MAX_CHANNEL_COUNT) ||
            (channel->setup >= ADI_AD7124_MAX_SETUP_COUNT) ||
            (channel->positiveInput > 31U) || (channel->negativeInput > 31U))
        {
            return kAdiAd7124_InvalidArgument;
        }
        value = channel->enabled ? ADI_AD7124_CHANNEL_ENABLE : 0U;
        value |= ((uint32_t)channel->setup << 12U) |
                 ((uint32_t)channel->positiveInput << 5U) |
                 (uint32_t)channel->negativeInput;
        status = ADI_AD7124_WriteRegister(
            device, (uint8_t)(ADI_AD7124_CHANNEL0_REG + channel->channel),
            value);
        if (status != kAdiAd7124_Ok)
        {
            return status;
        }
    }

    status = ADI_AD7124_ReadRegister(device, ADI_AD7124_ADC_CONTROL_REG,
                                     &control);
    if (status != kAdiAd7124_Ok)
    {
        return status;
    }
    if (internalReferenceRequired)
    {
        control |= ADI_AD7124_ADC_CONTROL_REF_EN;
    }
    else
    {
        control &= ~ADI_AD7124_ADC_CONTROL_REF_EN;
    }
    /* Continuous conversion, internal 614.4 kHz clock, full-power mode. */
    control &= ~ADI_AD7124_ADC_CONTROL_MODE_MASK;
    control |= ADI_AD7124_ADC_CONTROL_FULL_POWER;
    return ADI_AD7124_WriteRegister(device, ADI_AD7124_ADC_CONTROL_REG,
                                    control);
}
