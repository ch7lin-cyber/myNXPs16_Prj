#include "bsp_rs485.h"

#include "product_rs485_driver.h"

static ProductRs485Port_t ToProductPort(BspRs485Port_t port)
{
    return (port == BSP_RS485_PORT_EXTERNAL_SLAVE) ?
        PRODUCT_RS485_PORT_0 : PRODUCT_RS485_PORT_1;
}

static bool IsPortValid(BspRs485Port_t port)
{
    return ((uint32_t)port < (uint32_t)BSP_RS485_PORT_COUNT);
}

static BspRs485Status_t ToBspStatus(ProductRs485Status_t status)
{
    return (BspRs485Status_t)status;
}

BspRs485Status_t BspRs485_SetDirectionDelayUs(
    BspRs485Port_t port,
    uint32_t pre_tx_delay_us,
    uint32_t post_tx_delay_us)
{
    return IsPortValid(port) ?
        ToBspStatus(ProductRs485Driver_SetDirectionDelayUs(
            ToProductPort(port), pre_tx_delay_us, post_tx_delay_us)) :
        BSP_RS485_STATUS_INVALID_ARGUMENT;
}

BspRs485Status_t BspRs485_SetReceiveTimeoutUs(
    BspRs485Port_t port,
    uint32_t timeout_us)
{
    return IsPortValid(port) ?
        ToBspStatus(ProductRs485Driver_SetReceiveTimeoutUs(
            ToProductPort(port), timeout_us)) :
        BSP_RS485_STATUS_INVALID_ARGUMENT;
}

BspRs485Status_t BspRs485_SetRtuTimingUs(
    BspRs485Port_t port,
    uint32_t t15_us,
    uint32_t t35_us)
{
    return IsPortValid(port) ?
        ToBspStatus(ProductRs485Driver_SetRtuTimingUs(
            ToProductPort(port), t15_us, t35_us)) :
        BSP_RS485_STATUS_INVALID_ARGUMENT;
}

BspRs485Status_t BspRs485_StartReceive(
    BspRs485Port_t port,
    uint8_t *buffer,
    size_t capacity)
{
    return IsPortValid(port) ?
        ToBspStatus(ProductRs485Driver_StartReceive(
            ToProductPort(port), buffer, capacity)) :
        BSP_RS485_STATUS_INVALID_ARGUMENT;
}

BspRs485Status_t BspRs485_GetReceiveCount(
    BspRs485Port_t port,
    size_t *count)
{
    return IsPortValid(port) ?
        ToBspStatus(ProductRs485Driver_GetReceiveCount(
            ToProductPort(port), count)) :
        BSP_RS485_STATUS_INVALID_ARGUMENT;
}

BspRs485Status_t BspRs485_CompleteReceive(
    BspRs485Port_t port,
    size_t frame_length)
{
    return IsPortValid(port) ?
        ToBspStatus(ProductRs485Driver_CompleteReceive(
            ToProductPort(port), frame_length)) :
        BSP_RS485_STATUS_INVALID_ARGUMENT;
}

BspRs485Status_t BspRs485_TakeReceivedFrame(
    BspRs485Port_t port,
    size_t *length)
{
    return IsPortValid(port) ?
        ToBspStatus(ProductRs485Driver_TakeReceivedFrame(
            ToProductPort(port), length)) :
        BSP_RS485_STATUS_INVALID_ARGUMENT;
}

BspRs485Status_t BspRs485_GetLastReceiveTimingError(
    BspRs485Port_t port,
    bool *timing_error)
{
    return IsPortValid(port) ?
        ToBspStatus(ProductRs485Driver_GetLastReceiveTimingError(
            ToProductPort(port), timing_error)) :
        BSP_RS485_STATUS_INVALID_ARGUMENT;
}

BspRs485Status_t BspRs485_CancelReceive(BspRs485Port_t port)
{
    return IsPortValid(port) ?
        ToBspStatus(ProductRs485Driver_CancelReceive(ToProductPort(port))) :
        BSP_RS485_STATUS_INVALID_ARGUMENT;
}

BspRs485Status_t BspRs485_StartSlaveResponseDelayUs(uint32_t delay_us)
{
    return ToBspStatus(
        ProductRs485Driver_StartTurnaroundDelayUs(delay_us));
}

BspRs485Status_t BspRs485_TakeSlaveResponseDelayElapsed(bool *elapsed)
{
    return ToBspStatus(
        ProductRs485Driver_TakeTurnaroundDelayElapsed(elapsed));
}
