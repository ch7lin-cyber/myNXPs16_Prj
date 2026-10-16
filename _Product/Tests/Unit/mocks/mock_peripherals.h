#ifndef TEST_MOCK_PERIPHERALS_H
#define TEST_MOCK_PERIPHERALS_H

#include "fsl_spi.h"
#include "fsl_pint.h"

extern SPI_Type g_mock_spi;
#define MEM_FC7_PERIPHERAL (&g_mock_spi)
#define DAC_FC5_PERIPHERAL (&g_mock_spi)
#define DIPSW_SPI_PERIPHERAL (&g_mock_spi)

extern PINT_Type g_mock_pint;
#define PINT_PERIPHERAL (&g_mock_pint)
#define PINT_INT_0 kPINT_PinInt0
void drv_level_detect_callback(pint_pin_int_t pintr,
                               uint32_t pmatch_status);

#endif
