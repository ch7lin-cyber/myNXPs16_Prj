#ifndef PRODUCT_SENSOR_CONFIGURATION_CONSUMER_H
#define PRODUCT_SENSOR_CONFIGURATION_CONSUMER_H

#include <stdbool.h>
#include <stdint.h>

void ProductSensorConfigurationConsumer_Initialize(void);
bool ProductSensorConfigurationConsumer_Process(uint8_t channel,
                                                uint32_t timestamp_ms);

#endif
