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

/* Dedicated diagnostic builds only. Both default to zero in product builds. */
#ifndef PRODUCT_CAN_TEST_BUILD
#define PRODUCT_CAN_TEST_BUILD 0
#endif
#ifndef PRODUCT_CAN_INTERNAL_LOOPBACK
#define PRODUCT_CAN_INTERNAL_LOOPBACK 0
#endif
#if PRODUCT_CAN_INTERNAL_LOOPBACK && !PRODUCT_CAN_TEST_BUILD
#error "Internal CAN loopback requires PRODUCT_CAN_TEST_BUILD=1"
#endif

#endif /* PRODUCT_CAN_CONFIG_H */
