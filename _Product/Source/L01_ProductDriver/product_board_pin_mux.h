#ifndef PRODUCT_BOARD_PIN_MUX_H
#define PRODUCT_BOARD_PIN_MUX_H

/* Keep MCUXpresso-generated headers unique inside the project. Host tests use
 * an explicitly named mock so Config Tools cannot select it as an output. */
#if defined(PRODUCT_HOST_TEST)
#include "mock_pin_mux.h"
#else
#include "pin_mux.h"
#endif

#endif /* PRODUCT_BOARD_PIN_MUX_H */
