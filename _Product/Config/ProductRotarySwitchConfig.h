#ifndef PRODUCT_ROTARY_SWITCH_CONFIG_H
#define PRODUCT_ROTARY_SWITCH_CONFIG_H

/* SW2 is sampled as a four-bit 1/2/4/8 coded rotary switch. */
#define PRODUCT_ROTARY_SWITCH_SAMPLE_PERIOD_MS (10U)

/* Board pull-ups make a selected SW2 bit low. */
#define PRODUCT_ROTARY_SWITCH_ACTIVE_LOW_MASK  (0x0FU)

#endif
