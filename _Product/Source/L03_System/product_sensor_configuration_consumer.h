#ifndef PRODUCT_SENSOR_CONFIGURATION_CONSUMER_H
#define PRODUCT_SENSOR_CONFIGURATION_CONSUMER_H

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    uint32_t event_id;
    uint32_t configure_attempts;
    uint32_t apply_failures;
    uint32_t ack_attempts;
    uint32_t ack_failures;
    uint16_t revision;
    uint16_t last_status; /* AnalogInputStatus_t; stage identifies other failures. */
    uint16_t stage; /* 0 idle, 1 route, 2 configure, 3 sensor class, 4 ACK, 5 complete. */
} ProductSensorConfigurationDiagnostics_t;

bool ProductSensorConfigurationConsumer_GetDiagnostics(
    uint8_t channel, ProductSensorConfigurationDiagnostics_t *diagnostics);
void ProductSensorConfigurationConsumer_Initialize(void);
bool ProductSensorConfigurationConsumer_Process(uint8_t channel,
                                                uint32_t timestamp_ms);

#endif
