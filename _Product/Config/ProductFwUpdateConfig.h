#ifndef PRODUCT_FW_UPDATE_CONFIG_H
#define PRODUCT_FW_UPDATE_CONFIG_H

/* Wire identity retained for the released LPC55S16 Bootloader/UI protocol.
 * APP version remains the existing three U16 registers at 0x2000..0x2002.
 * Changing MCU/identity requires compatible Bootloader and UI support. */
#define PRODUCT_FW_UPDATE_IDENTITY {0xA016, 1, 1, 1, 0xC200, 8, 0, 1, 0, 1}
#define PRODUCT_FW_UPDATE_ALLOWED_ROUTES (FW_UPDATE_APP_ROUTE_USB | FW_UPDATE_APP_ROUTE_RS485)
#define PRODUCT_FW_UPDATE_REPLY_DELAY_MS 200u

#endif
