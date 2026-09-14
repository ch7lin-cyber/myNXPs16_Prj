/*
 * Product feature configuration.
 *
 * FLEXCOMM0 has one compile-time owner. Use the debug-console mode only
 * during hardware bring-up. Select RS-485 mode before enabling Modbus on
 * channel 0 so PRINTF cannot corrupt an RTU/ASCII frame.
 */

#ifndef PRODUCT_FEATURE_CONFIG_H_
#define PRODUCT_FEATURE_CONFIG_H_

#define PRODUCT_FC0_MODE_DEBUG_CONSOLE (0U)
#define PRODUCT_FC0_MODE_RS485         (1U)

#ifndef PRODUCT_FC0_MODE
#define PRODUCT_FC0_MODE PRODUCT_FC0_MODE_DEBUG_CONSOLE
#endif

#if ((PRODUCT_FC0_MODE != PRODUCT_FC0_MODE_DEBUG_CONSOLE) && \
     (PRODUCT_FC0_MODE != PRODUCT_FC0_MODE_RS485))
#error "PRODUCT_FC0_MODE must select DEBUG_CONSOLE or RS485"
#endif

#endif /* PRODUCT_FEATURE_CONFIG_H_ */
