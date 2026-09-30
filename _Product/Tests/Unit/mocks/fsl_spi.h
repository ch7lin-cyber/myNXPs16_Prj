#ifndef TEST_MOCK_FSL_SPI_H
#define TEST_MOCK_FSL_SPI_H

#include <stddef.h>
#include <stdint.h>

typedef struct
{
    uint32_t unused;
} SPI_Type;

typedef int32_t status_t;

#define kStatus_Success (0)

typedef enum
{
    kSPI_FrameAssert = 1U
} spi_xfer_option_t;

typedef struct
{
    uint8_t *txData;
    uint8_t *rxData;
    uint32_t configFlags;
    size_t dataSize;
} spi_transfer_t;

status_t SPI_MasterTransferBlocking(SPI_Type *base, spi_transfer_t *transfer);

#endif
