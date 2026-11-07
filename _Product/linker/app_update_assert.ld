/* Supplemental to MCUXpresso's managed linker. DirectUpdate stores the raw payload at APP base. */
ASSERT(ORIGIN(PROGRAM_FLASH) == 0x8000, "APP must link at 0x8000 for boot006")
ASSERT(LENGTH(PROGRAM_FLASH) == 0x34000, "APP partition must remain 208 KiB; boot006 DirectUpdate required")
ASSERT(_image_end <= 0x3C000, "APP payload exceeds DirectUpdate APP boundary")
ASSERT(ORIGIN(SRAMX) + LENGTH(SRAMX) <= 0x04003FC0, "APP overlaps boot request mailbox")

/* The MCAN message RAM is a hardware-visible reservation in SRAMX. */
PRODUCT_CAN_MRAM_START = 0x04000000;
PRODUCT_CAN_MRAM_END = 0x04001000;
ASSERT(ORIGIN(SRAMX) == PRODUCT_CAN_MRAM_END,
       "SRAMX linker region must start after MCAN message RAM")

/* Signature record reservation, schema/layout 1; ordinary updater never writes.
 * Public trust anchors belong in protected provisioning, private signing keys off-device.
 * Absolute symbols do not consume APP/Bootloader image bytes. */
FW_SIGNATURE_COPY0_BASE = 0x3C400;
FW_SIGNATURE_COPY1_BASE = 0x3CA00;
FW_SIGNATURE_COPY_SIZE = 0x600;
FW_SIGNATURE_RESERVE_END = 0x3D000;
ASSERT(FW_SIGNATURE_COPY0_BASE == 0x3C200 + 0x200, "Signature copy overlaps metadata")
ASSERT(FW_SIGNATURE_COPY0_BASE + FW_SIGNATURE_COPY_SIZE == FW_SIGNATURE_COPY1_BASE, "Signature copies overlap")
ASSERT(FW_SIGNATURE_COPY1_BASE + FW_SIGNATURE_COPY_SIZE == FW_SIGNATURE_RESERVE_END, "Signature reservation exceeds conservative Flash end")
