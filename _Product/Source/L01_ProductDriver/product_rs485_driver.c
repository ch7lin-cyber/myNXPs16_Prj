#include "product_rs485_driver.h"

#include "HalSerial.h"
#include "fsl_common.h"
#include "product_rs485_direction.h"
#include "product_rs485_dma.h"

static product_rs485_channel_t ToDriverChannel(ProductRs485Port_t port)
{
    return (port == PRODUCT_RS485_PORT_0) ?
        kProductRs485Channel0 : kProductRs485Channel1;
}

static ProductRs485Status_t ToProductStatus(status_t status)
{
    switch (status)
    {
        case kStatus_Success:
            return PRODUCT_RS485_STATUS_OK;
        case kStatus_InvalidArgument:
        case kStatus_OutOfRange:
            return PRODUCT_RS485_STATUS_INVALID_ARGUMENT;
        case kStatus_Busy:
            return PRODUCT_RS485_STATUS_BUSY;
        case kStatus_NoData:
            return PRODUCT_RS485_STATUS_NO_DATA;
        case kStatus_NoTransferInProgress:
            return PRODUCT_RS485_STATUS_NO_TRANSFER;
        case kStatus_Timeout:
            return PRODUCT_RS485_STATUS_TIMEOUT;
        default:
            return PRODUCT_RS485_STATUS_IO_ERROR;
    }
}

static bool IsPortValid(ProductRs485Port_t port)
{
    return ((uint32_t)port < (uint32_t)PRODUCT_RS485_PORT_COUNT);
}

static HalSerialStatus_t ToHalStatus(status_t status)
{
    ProductRs485Status_t product_status = ToProductStatus(status);

    if (product_status == PRODUCT_RS485_STATUS_OK)
    {
        return HAL_SERIAL_STATUS_OK;
    }
    if (product_status == PRODUCT_RS485_STATUS_INVALID_ARGUMENT)
    {
        return HAL_SERIAL_STATUS_INVALID_ARGUMENT;
    }
    if (product_status == PRODUCT_RS485_STATUS_BUSY)
    {
        return HAL_SERIAL_STATUS_BUSY;
    }
    return HAL_SERIAL_STATUS_IO_ERROR;
}

static HalSerialStatus_t HalInitialize(
    void *context,
    const HalSerialConfig_t *configuration)
{
    ProductRs485Port_t port = *(const ProductRs485Port_t *)context;

    return ToHalStatus(ProductRs485Dma_Configure(
        ToDriverChannel(port),
        configuration->baud_rate,
        (uint8_t)configuration->data_bits,
        (uint8_t)configuration->parity,
        (uint8_t)configuration->stop_bits));
}

static HalSerialStatus_t HalConfigure(
    void *context,
    const HalSerialConfig_t *configuration)
{
    return HalInitialize(context, configuration);
}

static HalSerialStatus_t HalWrite(
    void *context,
    const uint8_t *data,
    size_t length)
{
    ProductRs485Port_t port = *(const ProductRs485Port_t *)context;

    return ToHalStatus(ProductRs485Dma_SendAsync(
        ToDriverChannel(port), data, length));
}

static HalSerialStatus_t HalAbortWrite(void *context)
{
    ProductRs485Port_t port = *(const ProductRs485Port_t *)context;

    return ToHalStatus(ProductRs485Dma_AbortTransmit(ToDriverChannel(port)));
}

static bool HalIsWriteBusy(void *context)
{
    ProductRs485Port_t port = *(const ProductRs485Port_t *)context;
    bool busy = false;

    return (ProductRs485Dma_IsTransmitBusy(
                ToDriverChannel(port), &busy) == kStatus_Success) && busy;
}

bool ProductRs485Driver_Initialize(void)
{
    static const HalSerialDriverOps_t operations =
    {
        HalInitialize,
        HalConfigure,
        HalWrite,
        HalAbortWrite,
        HalIsWriteBusy
    };
    static ProductRs485Port_t ports[PRODUCT_RS485_PORT_COUNT] =
    {
        PRODUCT_RS485_PORT_0,
        PRODUCT_RS485_PORT_1
    };
    uint32_t index;

    if ((ProductRs485Direction_Init() != kStatus_Success) ||
        (ProductRs485Dma_Init() != kStatus_Success))
    {
        return false;
    }

    for (index = 0U; index < (uint32_t)PRODUCT_RS485_PORT_COUNT; index++)
    {
        if (HalSerial_RegisterDriver(
                (HalSerialPort_t)index,
                &operations,
                &ports[index]) != HAL_SERIAL_STATUS_OK)
        {
            return false;
        }
    }
    return true;
}

void ProductRs485Driver_Process(void)
{
    ProductRs485Dma_Process();
}

ProductRs485Status_t ProductRs485Driver_SetDirectionDelayUs(
    ProductRs485Port_t port,
    uint32_t pre_tx_delay_us,
    uint32_t post_tx_delay_us)
{
    product_rs485_timing_t timing;

    if (!IsPortValid(port))
    {
        return PRODUCT_RS485_STATUS_INVALID_ARGUMENT;
    }
    timing.preTxDelayUs = pre_tx_delay_us;
    timing.postTxDelayUs = post_tx_delay_us;
    return ToProductStatus(ProductRs485Direction_SetTiming(
        ToDriverChannel(port), &timing));
}

ProductRs485Status_t ProductRs485Driver_SetReceiveTimeoutUs(
    ProductRs485Port_t port,
    uint32_t timeout_us)
{
    return IsPortValid(port) ?
        ToProductStatus(ProductRs485Dma_SetReceiveTimeoutUs(
            ToDriverChannel(port), timeout_us)) :
        PRODUCT_RS485_STATUS_INVALID_ARGUMENT;
}

ProductRs485Status_t ProductRs485Driver_SetRtuTimingUs(
    ProductRs485Port_t port,
    uint32_t t15_us,
    uint32_t t35_us)
{
    return IsPortValid(port) ?
        ToProductStatus(ProductRs485Dma_SetRtuReceiveTimingUs(
            ToDriverChannel(port), t15_us, t35_us)) :
        PRODUCT_RS485_STATUS_INVALID_ARGUMENT;
}

ProductRs485Status_t ProductRs485Driver_StartReceive(
    ProductRs485Port_t port,
    uint8_t *buffer,
    size_t capacity)
{
    return IsPortValid(port) ?
        ToProductStatus(ProductRs485Dma_StartReceive(
            ToDriverChannel(port), buffer, capacity)) :
        PRODUCT_RS485_STATUS_INVALID_ARGUMENT;
}

ProductRs485Status_t ProductRs485Driver_GetReceiveCount(
    ProductRs485Port_t port,
    size_t *count)
{
    return IsPortValid(port) ?
        ToProductStatus(ProductRs485Dma_GetReceiveCount(
            ToDriverChannel(port), count)) :
        PRODUCT_RS485_STATUS_INVALID_ARGUMENT;
}

ProductRs485Status_t ProductRs485Driver_CompleteReceive(
    ProductRs485Port_t port,
    size_t frame_length)
{
    return IsPortValid(port) ?
        ToProductStatus(ProductRs485Dma_CompleteReceive(
            ToDriverChannel(port), frame_length)) :
        PRODUCT_RS485_STATUS_INVALID_ARGUMENT;
}

ProductRs485Status_t ProductRs485Driver_TakeReceivedFrame(
    ProductRs485Port_t port,
    size_t *length)
{
    return IsPortValid(port) ?
        ToProductStatus(ProductRs485Dma_TakeReceivedFrame(
            ToDriverChannel(port), length)) :
        PRODUCT_RS485_STATUS_INVALID_ARGUMENT;
}

ProductRs485Status_t ProductRs485Driver_GetLastReceiveTimingError(
    ProductRs485Port_t port,
    bool *timing_error)
{
    return IsPortValid(port) ?
        ToProductStatus(ProductRs485Dma_GetLastReceiveTimingError(
            ToDriverChannel(port), timing_error)) :
        PRODUCT_RS485_STATUS_INVALID_ARGUMENT;
}

ProductRs485Status_t ProductRs485Driver_CancelReceive(ProductRs485Port_t port)
{
    return IsPortValid(port) ?
        ToProductStatus(ProductRs485Dma_CancelReceive(ToDriverChannel(port))) :
        PRODUCT_RS485_STATUS_INVALID_ARGUMENT;
}

ProductRs485Status_t ProductRs485Driver_StartTurnaroundDelayUs(
    uint32_t delay_us)
{
    return ToProductStatus(ProductRs485Dma_StartTurnaroundDelayUs(delay_us));
}

ProductRs485Status_t ProductRs485Driver_TakeTurnaroundDelayElapsed(
    bool *elapsed)
{
    return ToProductStatus(ProductRs485Dma_TakeTurnaroundDelayElapsed(elapsed));
}
