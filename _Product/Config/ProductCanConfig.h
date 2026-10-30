#ifndef PRODUCT_CAN_CONFIG_H
#define PRODUCT_CAN_CONFIG_H

/*
 * Initial internal CAN transport settings.
 * Every node on the physical bus must use the same nominal bit rate.
 * Set this from the approved product network configuration before hardware
 * validation; 500 kbit/s is the MCUXpresso MCAN driver's conservative default.
 */
#define PRODUCT_CAN_BITRATE_BPS             (500000U)
#define PRODUCT_CAN_SOURCE_CLOCK_HZ         (15000000U)

/* LPC55S16 message RAM is reserved at the start of SRAMX. */
#define PRODUCT_CAN_MESSAGE_RAM_BASE        (0x04000000UL)
#define PRODUCT_CAN_MESSAGE_RAM_BYTES       (0x00001000UL)

#define PRODUCT_CAN_RX_FIFO_DEPTH           (16U)
#define PRODUCT_CAN_TX_QUEUE_DEPTH          (8U)
#define PRODUCT_CAN_MAX_DATA_LENGTH         (8U)

#endif /* PRODUCT_CAN_CONFIG_H */
