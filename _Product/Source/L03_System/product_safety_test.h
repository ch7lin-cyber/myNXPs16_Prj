#ifndef PRODUCT_SAFETY_TEST_H
#define PRODUCT_SAFETY_TEST_H
#include <stdbool.h>
#include <stdint.h>
#if defined(PRODUCT_SAFETY_TEST_BUILD) && defined(PRODUCT_SAFETY_TEST_TARGET)
#define PRODUCT_SAFETY_COMMAND_ENABLED 1
void ProductSafetyTest_SetAddressed(bool addressed);
uint8_t ProductSafetyTest_Write(uint16_t address, const uint16_t *values, uint16_t count);
uint8_t ProductSafetyTest_Read(uint16_t address, uint16_t count, uint16_t *values);
void ProductSafetyTest_Process(void);
#endif
#endif
