#ifndef TEST_MOCK_PERIPHERALS_H
#define TEST_MOCK_PERIPHERALS_H

#include "fsl_spi.h"

extern SPI_Type g_mock_spi;
#define MEM_FC7_PERIPHERAL (&g_mock_spi)
#define DAC_FC5_PERIPHERAL (&g_mock_spi)

#endif
