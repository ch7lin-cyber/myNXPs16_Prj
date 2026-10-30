#ifndef PRODUCT_CAN_DRIVER_H
#define PRODUCT_CAN_DRIVER_H

#include <stdbool.h>
#include <stdint.h>

#include "ProductCanConfig.h"

typedef struct
{
    uint16_t identifier; /* 11-bit standard CAN identifier. */
    uint8_t length;      /* 0..8 bytes. */
    uint8_t data[PRODUCT_CAN_MAX_DATA_LENGTH];
} ProductCanFrame_t;

typedef enum
{
    PRODUCT_CAN_STATE_ERROR_ACTIVE = 0U,
    PRODUCT_CAN_STATE_ERROR_WARNING,
    PRODUCT_CAN_STATE_ERROR_PASSIVE,
    PRODUCT_CAN_STATE_BUS_OFF
} ProductCanControllerState_t;

typedef struct
{
    uint32_t tx_queued;
    uint32_t tx_completed;
    uint32_t tx_queue_full;
    uint32_t tx_invalid_frame;
    uint32_t protocol_error_events;
    uint32_t error_warning_events;
    uint32_t error_passive_events;
    uint32_t bus_off_events;
    uint32_t bus_off_recoveries;
    uint32_t rx_received;
    uint32_t rx_queue_dropped;
    uint32_t rx_invalid_frame;
    uint32_t rx_fifo_lost_events;
    uint8_t tx_error_count;
    uint8_t rx_error_count;
    /* M_CAN LEC: 0=no error, 1=stuff, 2=form, 3=ACK, 4/5=bit, 6=CRC. */
    uint8_t last_protocol_error;
    uint8_t tx_queue_pending;
    bool tx_in_flight;
    ProductCanControllerState_t controller_state;
    bool bus_off;
    bool rx_fifo_lost; /* Sticky until ProductCanDriver_Init(). */
} ProductCanDiagnostics_t;

/* Initialize the Classic CAN controller and message RAM. */
bool ProductCanDriver_Init(void);

/* Drain received frames and submit queued transmissions. Call from foreground. */
void ProductCanDriver_Process(void);

/* Queue a standard data frame for transmission. Not callable from an ISR. */
bool ProductCanDriver_Send(const ProductCanFrame_t *frame);

/* Pop one received standard data frame from the software queue. */
bool ProductCanDriver_Receive(ProductCanFrame_t *frame);

/* Snapshot live controller state and sticky event counters. */
bool ProductCanDriver_GetDiagnostics(ProductCanDiagnostics_t *diagnostics);

#endif /* PRODUCT_CAN_DRIVER_H */
