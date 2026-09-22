#ifndef BSP_RS485_H_
#define BSP_RS485_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum
{
    BSP_RS485_PORT_EXTERNAL_SLAVE = 0,
    BSP_RS485_PORT_MACHINE_MASTER = 1,
    BSP_RS485_PORT_COUNT
} BspRs485Port_t;

typedef enum
{
    BSP_RS485_STATUS_OK = 0,
    BSP_RS485_STATUS_INVALID_ARGUMENT,
    BSP_RS485_STATUS_BUSY,
    BSP_RS485_STATUS_NO_DATA,
    BSP_RS485_STATUS_NO_TRANSFER,
    BSP_RS485_STATUS_TIMEOUT,
    BSP_RS485_STATUS_IO_ERROR
} BspRs485Status_t;

BspRs485Status_t BspRs485_SetDirectionDelayUs(
    BspRs485Port_t port,
    uint32_t pre_tx_delay_us,
    uint32_t post_tx_delay_us);
BspRs485Status_t BspRs485_SetReceiveTimeoutUs(
    BspRs485Port_t port,
    uint32_t timeout_us);
BspRs485Status_t BspRs485_SetRtuTimingUs(
    BspRs485Port_t port,
    uint32_t t15_us,
    uint32_t t35_us);
BspRs485Status_t BspRs485_StartReceive(
    BspRs485Port_t port,
    uint8_t *buffer,
    size_t capacity);
BspRs485Status_t BspRs485_GetReceiveCount(
    BspRs485Port_t port,
    size_t *count);
BspRs485Status_t BspRs485_CompleteReceive(
    BspRs485Port_t port,
    size_t frame_length);
BspRs485Status_t BspRs485_TakeReceivedFrame(
    BspRs485Port_t port,
    size_t *length);
BspRs485Status_t BspRs485_GetLastReceiveTimingError(
    BspRs485Port_t port,
    bool *timing_error);
BspRs485Status_t BspRs485_CancelReceive(BspRs485Port_t port);
BspRs485Status_t BspRs485_StartSlaveResponseDelayUs(uint32_t delay_us);
BspRs485Status_t BspRs485_TakeSlaveResponseDelayElapsed(bool *elapsed);

#endif /* BSP_RS485_H_ */
