/* Supplemental to MCUXpresso's managed linker. OTA1 has a 48-byte header. */
ASSERT(ORIGIN(PROGRAM_FLASH) == 0x8000, "APP must link at 0x8000 for boot005")
ASSERT(LENGTH(PROGRAM_FLASH) == 0x18000, "APP partition must remain 96 KiB")
ASSERT(_image_end <= 0x8000 + 0x18000 - 48, "APP payload exceeds OTA1 capacity")
ASSERT(ORIGIN(SRAMX) + LENGTH(SRAMX) <= 0x04003FC0, "APP overlaps boot request mailbox")
