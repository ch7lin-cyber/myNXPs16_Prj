#ifndef TEST_MOCK_FSL_COMMON_H
#define TEST_MOCK_FSL_COMMON_H

#include <stdint.h>

uint32_t DisableGlobalIRQ(void);
void EnableGlobalIRQ(uint32_t primask);

#endif
