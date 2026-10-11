#ifndef PRODUCT_USB_CDC_H
#define PRODUCT_USB_CDC_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
bool ProductUsbCdc_Init(void);
void ProductUsbCdc_Process(void);
size_t ProductUsbCdc_Read(uint8_t *buffer, size_t capacity);
bool ProductUsbCdc_Send(const uint8_t *buffer, size_t length, uint32_t expected_session);
bool ProductUsbCdc_TxReady(void);
uint32_t ProductUsbCdc_Session(void);
#endif
