/*
 * Copyright 2026
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "product_ad7124_driver.h"

#include <string.h>

#include "fsl_common.h"
#include "fsl_gpio.h"
#include "fsl_iocon.h"
#include "fsl_spi.h"
#include "peripherals.h"
#include "ProductAdcConfig.h"

#define PRODUCT_AD7124_MAX_TRANSFER_SIZE (8U)
#define PRODUCT_AD7124_INIT_POLL_LIMIT   (256UL)

typedef struct _product_ad7124_transport
{
    uint8_t chipSelectPort;
    uint8_t chipSelectPin;
} product_ad7124_transport_t;

/* Physical FC3 SSEL0/1/2/3 pins are controlled as active-low GPIO CS. */
static product_ad7124_transport_t s_transport[PRODUCT_AD7124_DEVICE_COUNT] =
{
    {0U, 20U}, /* Pin 74: FC3 SSEL0, ADC0. */
    {0U, 21U}, /* Pin 76: FC3 SSEL1, ADC1. */
    {0U, 9U},  /* Pin 55: FC3 SSEL2, ADC2. */
    {1U, 24U}  /* Pin 3:  FC3 SSEL3, ADC3. */
};

static adi_ad7124_device_t s_devices[PRODUCT_AD7124_DEVICE_COUNT];
static bool s_chipSelectsConfigured;

static void ProductAd7124_DeassertAll(void)
{
    uint8_t deviceIndex;

    for (deviceIndex = 0U;
         deviceIndex < PRODUCT_AD7124_DEVICE_COUNT;
         deviceIndex++)
    {
        GPIO_PinWrite(GPIO,
                      s_transport[deviceIndex].chipSelectPort,
                      s_transport[deviceIndex].chipSelectPin,
                      1U);
    }
}

static void ProductAd7124_ConfigureChipSelects(void)
{
    uint8_t deviceIndex;
    const gpio_pin_config_t outputHigh =
    {
        kGPIO_DigitalOutput,
        1U
    };

    if (s_chipSelectsConfigured)
    {
        return;
    }

    /* Load the inactive GPIO value before disconnecting hardware SSEL. */
    for (deviceIndex = 0U;
         deviceIndex < PRODUCT_AD7124_DEVICE_COUNT;
         deviceIndex++)
    {
        GPIO_PinInit(GPIO,
                     s_transport[deviceIndex].chipSelectPort,
                     s_transport[deviceIndex].chipSelectPin,
                     &outputHigh);
        IOCON_PinMuxSet(IOCON,
                        s_transport[deviceIndex].chipSelectPort,
                        s_transport[deviceIndex].chipSelectPin,
                        IOCON_DIGITAL_EN);
    }
    ProductAd7124_DeassertAll();
    s_chipSelectsConfigured = true;
}

static bool ProductAd7124_Transfer(
    void *context,
    uint8_t *data,
    size_t length)
{
    product_ad7124_transport_t *transport =
        (product_ad7124_transport_t *)context;
    spi_transfer_t transfer;
    uint8_t txBuffer[PRODUCT_AD7124_MAX_TRANSFER_SIZE];
    uint8_t rxBuffer[PRODUCT_AD7124_MAX_TRANSFER_SIZE];
    status_t status;

    if ((transport == NULL) || (data == NULL) || (length == 0U) ||
        (length > sizeof(txBuffer)))
    {
        return false;
    }

    ProductAd7124_ConfigureChipSelects();

    (void)memcpy(txBuffer, data, length);
    (void)memset(rxBuffer, 0, length);
    transfer.txData = txBuffer;
    transfer.rxData = rxBuffer;
    transfer.dataSize = length;
    transfer.configFlags = (uint32_t)kSPI_FrameAssert;

    ProductAd7124_DeassertAll();
    GPIO_PinWrite(GPIO, transport->chipSelectPort,
                  transport->chipSelectPin, 0U);
    status = SPI_MasterTransferBlocking(ADC_FC3_PERIPHERAL, &transfer);
    GPIO_PinWrite(GPIO, transport->chipSelectPort,
                  transport->chipSelectPin, 1U);
    if (status != kStatus_Success)
    {
        return false;
    }
    (void)memcpy(data, rxBuffer, length);
    return true;
}

static void ProductAd7124_DelayMs(void *context, uint32_t delayMs)
{
    (void)context;
    SDK_DelayAtLeastUs(delayMs * 1000UL, SystemCoreClock);
}

adi_ad7124_device_t *ProductAd7124_GetDevice(uint8_t deviceIndex)
{
    return (deviceIndex < PRODUCT_AD7124_DEVICE_COUNT) ?
           &s_devices[deviceIndex] : NULL;
}

adi_ad7124_status_t ProductAd7124_InitDevice(uint8_t deviceIndex)
{
    adi_ad7124_device_t *device;

    if (deviceIndex >= PRODUCT_AD7124_DEVICE_COUNT)
    {
        return kAdiAd7124_InvalidArgument;
    }

    device = &s_devices[deviceIndex];
    (void)memset(device, 0, sizeof(*device));
    device->transfer = ProductAd7124_Transfer;
    device->delayMs = ProductAd7124_DelayMs;
    device->transportContext = &s_transport[deviceIndex];
    /* Bound startup time when a module is absent from the shared SPI bus. */
    device->pollLimit = PRODUCT_AD7124_INIT_POLL_LIMIT;
    device->expectedVariant = kAdiAd7124_Variant4;
    {
        adi_ad7124_status_t status = ADI_AD7124_Init(device);
        if (status != kAdiAd7124_Ok)
        {
            return status;
        }
#if PRODUCT_ADC_SPI_CRC_ENABLED
        return ADI_AD7124_EnableCrc(device);
#else
        return kAdiAd7124_Ok;
#endif
    }
}

adi_ad7124_status_t ProductAd7124_InitAll(void)
{
    uint8_t deviceIndex;
    adi_ad7124_status_t status;

    for (deviceIndex = 0U;
         deviceIndex < PRODUCT_AD7124_DEVICE_COUNT;
         deviceIndex++)
    {
        status = ProductAd7124_InitDevice(deviceIndex);
        if (status != kAdiAd7124_Ok)
        {
            return status;
        }
    }
    return kAdiAd7124_Ok;
}
