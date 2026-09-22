#ifndef L03_PRODUCT_MODBUS_H_
#define L03_PRODUCT_MODBUS_H_

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    uint32_t receivedFrames;
    uint32_t transmittedResponses;
    uint32_t ignoredFrames;
    uint32_t crcErrors;
    uint32_t lrcErrors;
    uint32_t rtuInterCharacterErrors;
    uint32_t transportErrors;
} l03_product_modbus_statistics_t;

bool L03_ProductModbus_Init(void);
void L03_ProductModbus_Tick1ms(void);
void L03_ProductModbus_Process(void);
bool L03_ProductModbus_GetStatistics(
    l03_product_modbus_statistics_t *statistics);
bool L03_ProductModbus_IsInitialized(void);

#endif /* L03_PRODUCT_MODBUS_H_ */
