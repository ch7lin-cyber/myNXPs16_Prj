#ifndef TEST_MOCK_FSL_LPADC_H
#define TEST_MOCK_FSL_LPADC_H

#include <stdbool.h>
#include <stdint.h>

typedef struct { uint32_t unused; } LPADC_Type;
typedef struct
{
    uint16_t convValue;
    uint8_t commandIdSource;
} lpadc_conv_result_t;

#define kLPADC_ResultFIFO0OverflowFlag (1UL << 0)
#define kLPADC_ResultFIFO1OverflowFlag (1UL << 1)
#define FSL_FEATURE_LPADC_TEMP_PARAMETER_ALPHA (0.0F)
#define FSL_FEATURE_LPADC_TEMP_PARAMETER_A (1.0F)
#define FSL_FEATURE_LPADC_TEMP_PARAMETER_B (0.0F)

void LPADC_DoSoftwareTrigger(LPADC_Type *base, uint32_t triggerMask);
uint32_t LPADC_GetStatusFlags(LPADC_Type *base);
void LPADC_ClearStatusFlags(LPADC_Type *base, uint32_t mask);
bool LPADC_GetConvResult(LPADC_Type *base, lpadc_conv_result_t *result,
                         uint32_t fifoIndex);

#endif
