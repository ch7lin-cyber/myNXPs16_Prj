#ifndef TEST_MOCK_FSL_POWER_H
#define TEST_MOCK_FSL_POWER_H

#include <stdint.h>

#define kPDRUNCFG_PD_TEMPSENS (1UL << 0)
void POWER_DisablePD(uint32_t mask);

#endif
