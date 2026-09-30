#ifndef BSP_ANALOG_OUTPUT_H
#define BSP_ANALOG_OUTPUT_H

#include <stdbool.h>
#include <stdint.h>

#define BSP_ANALOG_OUTPUT_COUNT (4U)

typedef enum
{
    BSP_ANALOG_OUTPUT_0 = 0,
    BSP_ANALOG_OUTPUT_1,
    BSP_ANALOG_OUTPUT_2,
    BSP_ANALOG_OUTPUT_3
} BspAnalogOutput_t;

bool BspAnalogOutput_Initialize(void);
bool BspAnalogOutput_WriteCode(BspAnalogOutput_t output, uint16_t code);
bool BspAnalogOutput_WriteDacMicrovolts(BspAnalogOutput_t output,
                                        uint32_t microvolts);
bool BspAnalogOutput_GetLastCode(BspAnalogOutput_t output, uint16_t *code);

#endif /* BSP_ANALOG_OUTPUT_H */
