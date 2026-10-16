#ifndef PRODUCT_DIP_SWITCH_CONFIG_H
#define PRODUCT_DIP_SWITCH_CONFIG_H

/* U5 is read through the board's parallel-in/serial-out SPI interface. */
#define PRODUCT_DIP_SWITCH_COUNT            (8U)
#define PRODUCT_DIP_SWITCH_SAMPLE_PERIOD_MS (10U)

/* A set bit means the corresponding U5 switch is ON when its input is low. */
#define PRODUCT_DIP_SWITCH_ACTIVE_LOW_MASK  (0xFFU)

/*
 * Raw SPI bit 0 is U5 switch 1, raw bit 1 is switch 2, and so on.  Set this
 * to 1 only if a board revision wires the parallel inputs in reverse order.
 */
#define PRODUCT_DIP_SWITCH_REVERSE_BITS     (0U)

#endif
