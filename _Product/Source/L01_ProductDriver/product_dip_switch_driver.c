#include "product_dip_switch_driver.h"

#include <stddef.h>

#include "ProductDipSwitchConfig.h"
#include "fsl_spi.h"
#include "peripherals.h"

static ProductDipSwitchSnapshot_t g_snapshot;
static bool g_has_valid_sample;

static uint8_t ReverseBits(uint8_t value)
{
    value = (uint8_t)(((value & 0x55U) << 1U) |
                      ((value & 0xAAU) >> 1U));
    value = (uint8_t)(((value & 0x33U) << 2U) |
                      ((value & 0xCCU) >> 2U));
    return (uint8_t)((value << 4U) | (value >> 4U));
}

static uint8_t ToLogicalMask(uint8_t raw_value)
{
    uint8_t mapped_value = raw_value;

#if (PRODUCT_DIP_SWITCH_REVERSE_BITS != 0U)
    mapped_value = ReverseBits(mapped_value);
#else
    (void)ReverseBits;
#endif
    return (uint8_t)(mapped_value ^ PRODUCT_DIP_SWITCH_ACTIVE_LOW_MASK);
}

void ProductDipSwitchDriver_Initialize(void)
{
    g_snapshot.logical_mask = 0U;
    g_snapshot.raw_value = 0U;
    g_snapshot.revision = 0U;
    g_snapshot.status = PRODUCT_DIP_SWITCH_STATUS_NOT_INITIALIZED;
    g_has_valid_sample = false;
}

bool ProductDipSwitchDriver_Process(void)
{
    uint8_t tx_data = 0xFFU;
    uint8_t rx_data = 0U;
    uint8_t logical_mask;
    spi_transfer_t transfer;

    transfer.txData = &tx_data;
    transfer.rxData = &rx_data;
    transfer.dataSize = 1U;
    transfer.configFlags = kSPI_FrameAssert;

    if (SPI_MasterTransferBlocking(DIPSW_SPI_PERIPHERAL, &transfer) !=
        kStatus_Success)
    {
        g_snapshot.status = PRODUCT_DIP_SWITCH_STATUS_IO_ERROR;
        return false;
    }

    logical_mask = ToLogicalMask(rx_data);
    if (!g_has_valid_sample ||
        (logical_mask != g_snapshot.logical_mask))
    {
        g_snapshot.revision++;
    }
    g_snapshot.raw_value = rx_data;
    g_snapshot.logical_mask = logical_mask;
    g_snapshot.status = PRODUCT_DIP_SWITCH_STATUS_READY;
    g_has_valid_sample = true;
    return true;
}

bool ProductDipSwitchDriver_GetSnapshot(ProductDipSwitchSnapshot_t *snapshot)
{
    if (snapshot == NULL)
    {
        return false;
    }
    *snapshot = g_snapshot;
    return true;
}
