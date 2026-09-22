/*
 * Product feature configuration.
 *
 * FLEXCOMM0 is initialized as a debug console for boot/maintenance output,
 * then ownership is transferred to the external Modbus Slave. Debug output
 * is forbidden after ProductRs485Driver_Initialize() succeeds because an
 * unsolicited string would corrupt the RS-485 protocol stream.
 */

#ifndef PRODUCT_FEATURE_CONFIG_H_
#define PRODUCT_FEATURE_CONFIG_H_

#ifndef PRODUCT_FC0_BOOT_DEBUG_ENABLE
#define PRODUCT_FC0_BOOT_DEBUG_ENABLE (1U)
#endif

#if ((PRODUCT_FC0_BOOT_DEBUG_ENABLE != 0U) && \
     (PRODUCT_FC0_BOOT_DEBUG_ENABLE != 1U))
#error "PRODUCT_FC0_BOOT_DEBUG_ENABLE must be 0 or 1"
#endif

#endif /* PRODUCT_FEATURE_CONFIG_H_ */
