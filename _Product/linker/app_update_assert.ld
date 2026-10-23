/* Supplemental to MCUXpresso's managed linker. OTA1 has a 48-byte header. */
ASSERT(ORIGIN(PROGRAM_FLASH) == 0x8000, "APP must link at 0x8000 for boot005")
ASSERT(LENGTH(PROGRAM_FLASH) == 0x1A000, "APP partition must remain 104 KiB; boot005 >= 00.01.11 required")
ASSERT(_image_end <= 0x8000 + 0x1A000 - 48, "APP payload exceeds OTA1 capacity")
ASSERT(ORIGIN(SRAMX) + LENGTH(SRAMX) <= 0x04003FC0, "APP overlaps boot request mailbox")
