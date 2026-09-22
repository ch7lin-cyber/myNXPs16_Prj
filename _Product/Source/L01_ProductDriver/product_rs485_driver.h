#ifndef PRODUCT_RS485_DRIVER_H_
#define PRODUCT_RS485_DRIVER_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum
{
    PRODUCT_RS485_PORT_0 = 0,
    PRODUCT_RS485_PORT_1 = 1,
    PRODUCT_RS485_PORT_COUNT
} ProductRs485Port_t;

typedef enum
{
    PRODUCT_RS485_STATUS_OK = 0,
    PRODUCT_RS485_STATUS_INVALID_ARGUMENT,
    PRODUCT_RS485_STATUS_BUSY,
    PRODUCT_RS485_STATUS_NO_DATA,
    PRODUCT_RS485_STATUS_NO_TRANSFER,
    PRODUCT_RS485_STATUS_TIMEOUT,
    PRODUCT_RS485_STATUS_IO_ERROR
} ProductRs485Status_t;

bool ProductRs485Driver_Initialize(void);
void ProductRs485Driver_Process(void);

ProductRs485Status_t ProductRs485Driver_SetDirectionDelayUs(
    ProductRs485Port_t port,
    uint32_t pre_tx_delay_us,
    uint32_t post_tx_delay_us);
ProductRs485Status_t ProductRs485Driver_SetReceiveTimeoutUs(
    ProductRs485Port_t port,
    uint32_t timeout_us);
ProductRs485Status_t ProductRs485Driver_SetRtuTimingUs(
    ProductRs485Port_t port,
    uint32_t t15_us,
    uint32_t t35_us);
ProductRs485Status_t ProductRs485Driver_StartReceive(
    ProductRs485Port_t port,
    uint8_t *buffer,
    size_t capacity);
ProductRs485Status_t ProductRs485Driver_GetReceiveCount(
    ProductRs485Port_t port,
    size_t *count);
ProductRs485Status_t ProductRs485Driver_CompleteReceive(
    ProductRs485Port_t port,
    size_t frame_length);
ProductRs485Status_t ProductRs485Driver_TakeReceivedFrame(
    ProductRs485Port_t port,
    size_t *length);
ProductRs485Status_t ProductRs485Driver_GetLastReceiveTimingError(
    ProductRs485Port_t port,
    bool *timing_error);
ProductRs485Status_t ProductRs485Driver_CancelReceive(ProductRs485Port_t port);
ProductRs485Status_t ProductRs485Driver_StartTurnaroundDelayUs(
    uint32_t delay_us);
ProductRs485Status_t ProductRs485Driver_TakeTurnaroundDelayElapsed(
    bool *elapsed);

#endif /* PRODUCT_RS485_DRIVER_H_ */
