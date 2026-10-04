#ifndef TEST_MOCK_FSL_GPIO_H
#define TEST_MOCK_FSL_GPIO_H

#include <stdint.h>

typedef struct
{
    uint32_t unused;
} GPIO_Type;

void GPIO_PinWrite(GPIO_Type *base, uint32_t port, uint32_t pin,
                   uint8_t output);
uint32_t GPIO_PinRead(GPIO_Type *base, uint32_t port, uint32_t pin);

#endif
