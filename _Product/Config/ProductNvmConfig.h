#ifndef PRODUCT_NVM_CONFIG_H
#define PRODUCT_NVM_CONFIG_H

/* External NVM fitted to FC7 SPI. */
#define PRODUCT_NVM_DEVICE_MODEL              "MB85RS2MTYPNF-GS-AWERE2"
#define PRODUCT_NVM_FRAM_CAPACITY_BYTES       (262144UL)
#define PRODUCT_NVM_LOGICAL_SLOT_SIZE         (32768UL)
#define PRODUCT_NVM_SLOT0_ADDRESS             (0x000000UL)
#define PRODUCT_NVM_SLOT1_ADDRESS             (0x008000UL)
#define PRODUCT_NVM_ADDRESS_MASK              (0x03FFFFUL)

/* Forty calibration records, two 64-byte copies each. End is exclusive.
 * Separate from temperature configuration slots at 0x00000 / 0x08000. */
#define PRODUCT_NVM_CALIBRATION_BASE_ADDRESS  (0x010000UL)
#define PRODUCT_NVM_CALIBRATION_END_ADDRESS   (0x011400UL)

/* RDID response: Manufacturer / continuation / product bytes. */
#define PRODUCT_NVM_MANUFACTURER_ID           (0x04U)
#define PRODUCT_NVM_CONTINUATION_CODE         (0x7FU)
#define PRODUCT_NVM_PRODUCT_ID_1              (0x48U)
#define PRODUCT_NVM_PRODUCT_ID_2              (0x0AU)

#endif
