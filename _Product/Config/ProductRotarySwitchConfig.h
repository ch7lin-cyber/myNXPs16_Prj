#ifndef PRODUCT_ROTARY_SWITCH_CONFIG_H
#define PRODUCT_ROTARY_SWITCH_CONFIG_H

/* DTX station straps. Legacy driver filename is retained for IDE source lists.
 * PIN85 / PIO1_27: low = station 1, high = station 2.
 * PIN86 / PIO0_4: diagnostic raw bit only, does not select the station. */
#define PRODUCT_ROTARY_SWITCH_SAMPLE_PERIOD_MS (10U)

#endif
