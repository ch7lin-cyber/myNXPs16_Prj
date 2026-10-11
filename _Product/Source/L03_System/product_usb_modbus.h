#ifndef PRODUCT_USB_MODBUS_H
#define PRODUCT_USB_MODBUS_H
#include <stdbool.h>
#include <stdint.h>
bool ProductUsbModbus_Init(void);
void ProductUsbModbus_Process(uint32_t now_ms);
#endif
