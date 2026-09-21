/*
 * Copyright 2026
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "product_ad7124_driver.h"

#include <string.h>

#include "fsl_common.h"
#include "fsl_spi.h"
#include "peripherals.h"

#define PRODUCT_AD7124_MAX_TRANSFER_SIZE (8U)

typedef struct _product_ad7124_transport
{
    uint8_t deviceIndex;
    spi_ssel_t slaveSelect;
} product_ad7124_transport_t;

/* Board routing: ADC0/1/2/3 use FC3 SSEL2/1/0/3 respectively. */
static product_ad7124_transport_t s_transport[PRODUCT_AD7124_DEVICE_COUNT] =
{
    {0U, kSPI_Ssel2},
    {1U, kSPI_Ssel1},
    {2U, kSPI_Ssel0},
    {3U, kSPI_Ssel3}
};

static adi_ad7124_device_t s_devices[PRODUCT_AD7124_DEVICE_COUNT];
static uint8_t s_selectedDevice;
static bool s_busConfigured;

static bool ProductAd7124_Transfer(
    void *context,
    uint8_t *data,
    size_t length)
{
    product_ad7124_transport_t *transport =
        (product_ad7124_transport_t *)context;
    spi_master_config_t config;
    spi_transfer_t transfer;
    uint8_t txBuffer[PRODUCT_AD7124_MAX_TRANSFER_SIZE];
    uint8_t rxBuffer[PRODUCT_AD7124_MAX_TRANSFER_SIZE];
    status_t status;

    if ((transport == NULL) || (data == NULL) || (length == 0U) ||
        (length > sizeof(txBuffer)))
    {
        return false;
    }

    /*
     * The SDK stores SSEL in a private per-instance configuration. Re-init is
     * required to select among the four hardware SSEL outputs without changing
     * generated pin mux files.
     */
    if (!s_busConfigured || (s_selectedDevice != transport->deviceIndex))
    {
        config = ADC_FC3_config;
        config.sselNum = transport->slaveSelect;
        SPI_Deinit(ADC_FC3_PERIPHERAL);
        status = SPI_MasterInit(ADC_FC3_PERIPHERAL, &config,
                                ADC_FC3_CLOCK_SOURCE);
        if (status != kStatus_Success)
        {
            s_busConfigured = false;
            return false;
        }
        s_selectedDevice = transport->deviceIndex;
        s_busConfigured = true;
    }

    (void)memcpy(txBuffer, data, length);
    (void)memset(rxBuffer, 0, length);
    transfer.txData = txBuffer;
    transfer.rxData = rxBuffer;
    transfer.dataSize = length;
    transfer.configFlags = (uint32_t)kSPI_FrameAssert;

    status = SPI_MasterTransferBlocking(ADC_FC3_PERIPHERAL, &transfer);
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
    device->pollLimit = ADI_AD7124_DEFAULT_POLL_LIMIT;
    device->expectedVariant = kAdiAd7124_AnyVariant;
    return ADI_AD7124_Init(device);
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
