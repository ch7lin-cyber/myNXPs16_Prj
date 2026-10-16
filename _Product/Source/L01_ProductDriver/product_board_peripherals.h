#ifndef PRODUCT_BOARD_PERIPHERALS_H
#define PRODUCT_BOARD_PERIPHERALS_H

#if defined(PRODUCT_HOST_TEST) && (PRODUCT_HOST_TEST != 0)
#include "../../Tests/Unit/mocks/mock_peripherals.h"
#else
#include "../../../board/peripherals.h"
#endif

#endif
