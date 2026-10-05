#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "fsl_spi.h"
#include "product_dip_switch_driver.h"

SPI_Type g_mock_spi;
static uint8_t g_next_rx_value;
static bool g_fail_next_transfer;
static uint32_t g_transfer_count;

status_t SPI_MasterTransferBlocking(SPI_Type *base, spi_transfer_t *transfer)
{
    assert(base == &g_mock_spi);
    assert(transfer != NULL);
    assert(transfer->txData != NULL);
    assert(transfer->rxData != NULL);
    assert(transfer->dataSize == 1U);
    assert(transfer->configFlags == kSPI_FrameAssert);
    assert(transfer->txData[0] == 0xFFU);
    g_transfer_count++;

    if (g_fail_next_transfer)
    {
        g_fail_next_transfer = false;
        return -1;
    }
    transfer->rxData[0] = g_next_rx_value;
    return kStatus_Success;
}

int main(void)
{
    ProductDipSwitchSnapshot_t snapshot;

    ProductDipSwitchDriver_Initialize();
    assert(ProductDipSwitchDriver_GetSnapshot(&snapshot));
    assert(snapshot.status == PRODUCT_DIP_SWITCH_STATUS_NOT_INITIALIZED);
    assert(snapshot.revision == 0U);
    assert(!ProductDipSwitchDriver_GetSnapshot(NULL));

    g_next_rx_value = 0xF5U;
    assert(ProductDipSwitchDriver_Process());
    assert(ProductDipSwitchDriver_GetSnapshot(&snapshot));
    assert(snapshot.raw_value == 0xF5U);
    assert(snapshot.logical_mask == 0x0AU);
    assert(snapshot.revision == 1U);
    assert(snapshot.status == PRODUCT_DIP_SWITCH_STATUS_READY);

    assert(ProductDipSwitchDriver_Process());
    assert(ProductDipSwitchDriver_GetSnapshot(&snapshot));
    assert(snapshot.revision == 1U);

    g_next_rx_value = 0xF4U;
    assert(ProductDipSwitchDriver_Process());
    assert(ProductDipSwitchDriver_GetSnapshot(&snapshot));
    assert(snapshot.logical_mask == 0x0BU);
    assert(snapshot.revision == 2U);

    g_fail_next_transfer = true;
    assert(!ProductDipSwitchDriver_Process());
    assert(ProductDipSwitchDriver_GetSnapshot(&snapshot));
    assert(snapshot.raw_value == 0xF4U);
    assert(snapshot.logical_mask == 0x0BU);
    assert(snapshot.revision == 2U);
    assert(snapshot.status == PRODUCT_DIP_SWITCH_STATUS_IO_ERROR);

    assert(ProductDipSwitchDriver_Process());
    assert(ProductDipSwitchDriver_GetSnapshot(&snapshot));
    assert(snapshot.revision == 2U);
    assert(snapshot.status == PRODUCT_DIP_SWITCH_STATUS_READY);
    assert(g_transfer_count == 5U);
    return 0;
}
