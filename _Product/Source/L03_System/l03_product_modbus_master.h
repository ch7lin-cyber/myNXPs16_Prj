#ifndef L03_PRODUCT_MODBUS_MASTER_H_
#define L03_PRODUCT_MODBUS_MASTER_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ModbusMaster.h"

typedef enum
{
    PRODUCT_MODBUS_MASTER_IDLE = 0,
    PRODUCT_MODBUS_MASTER_BUSY,
    PRODUCT_MODBUS_MASTER_COMPLETE,
    PRODUCT_MODBUS_MASTER_ERROR
} ProductModbusMasterState_t;

typedef struct
{
    ProductModbusMasterState_t state;
    ModbusMasterStatus_t status;
    ModbusExceptionCode_t exception;
    uint16_t values[MODBUS_MASTER_READ_QUANTITY_MAX];
    size_t value_count;
} ProductModbusMasterResult_t;

bool L03_ProductModbusMaster_Init(void);
bool L03_ProductModbusMaster_Submit(const ModbusMasterRequest_t *request);
void L03_ProductModbusMaster_Tick1ms(void);
void L03_ProductModbusMaster_Process(void);
bool L03_ProductModbusMaster_GetResult(ProductModbusMasterResult_t *result);
void L03_ProductModbusMaster_ClearResult(void);

#endif /* L03_PRODUCT_MODBUS_MASTER_H_ */
