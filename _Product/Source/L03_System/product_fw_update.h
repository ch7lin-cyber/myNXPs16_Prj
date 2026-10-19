#ifndef PRODUCT_FW_UPDATE_H
#define PRODUCT_FW_UPDATE_H
#include <stdbool.h>
#include <stdint.h>
/* Route 0 disables writes (broadcast / outside a transport callback). */
void ProductFwUpdate_Init(void);
void ProductFwUpdate_SetRequestContext(uint16_t route, uint16_t unit);
uint8_t ProductFwUpdate_Read(uint16_t address,uint16_t count,uint16_t *values);
uint8_t ProductFwUpdate_Write(uint16_t address,uint16_t value);
bool ProductFwUpdate_IsPending(void);
void ProductFwUpdate_ReplySent(uint16_t route);
void ProductFwUpdate_Process(uint32_t now_ms);
/* Board adapter: synchronous safe outputs, then reset from foreground only. */
bool ProductFwUpdate_Quiesce(void);
void ProductFwUpdate_Reset(uint16_t route,uint16_t unit,uint32_t baud);
#endif
