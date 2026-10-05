#ifndef TEST_MOCK_FSL_PINT_H
#define TEST_MOCK_FSL_PINT_H

#include <stdint.h>

typedef struct
{
    uint32_t unused;
} PINT_Type;

typedef enum
{
    kPINT_PinInt0 = 0U
} pint_pin_int_t;

typedef enum
{
    kPINT_PinIntEnableRiseEdge = 1U
} pint_pin_enable_t;

typedef void (*pint_cb_t)(pint_pin_int_t pintr, uint32_t pmatch_status);

void PINT_PinInterruptConfig(PINT_Type *base, pint_pin_int_t intr,
                             pint_pin_enable_t enable, pint_cb_t callback);
void PINT_EnableCallbackByIndex(PINT_Type *base, pint_pin_int_t intr);

#endif
