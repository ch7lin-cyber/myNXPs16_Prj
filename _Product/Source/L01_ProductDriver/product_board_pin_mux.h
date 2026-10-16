#ifndef PRODUCT_BOARD_PIN_MUX_H
#define PRODUCT_BOARD_PIN_MUX_H

#if defined(PRODUCT_HOST_TEST) && (PRODUCT_HOST_TEST != 0)
#include "../../Tests/Unit/mocks/mock_pin_mux.h"
#else
#include "../../../board/pin_mux.h"
#endif

#endif
