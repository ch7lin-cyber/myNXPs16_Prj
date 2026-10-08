#ifndef PRODUCT_FACTORY_CALIBRATION_STORAGE_H
#define PRODUCT_FACTORY_CALIBRATION_STORAGE_H
#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    bool ready;
    uint16_t loaded_records;
    uint16_t corrupt_records;
    uint16_t last_error; /* 0 OK, 1 geometry/read, 2 busy, 3 write/verify. */
} ProductFactoryCalibrationStorageStatus_t;

bool ProductFactoryCalibrationStorage_Initialize(void);
void ProductFactoryCalibrationStorage_GetStatus(ProductFactoryCalibrationStorageStatus_t *status);
#endif
