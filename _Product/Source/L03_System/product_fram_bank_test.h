#ifndef PRODUCT_FRAM_BANK_TEST_H
#define PRODUCT_FRAM_BANK_TEST_H

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    PRODUCT_FRAM_TEST_STATE_IDLE = 0,
    PRODUCT_FRAM_TEST_STATE_WRITE_PATTERN_1,
    PRODUCT_FRAM_TEST_STATE_VERIFY_PATTERN_1,
    PRODUCT_FRAM_TEST_STATE_WRITE_PATTERN_2,
    PRODUCT_FRAM_TEST_STATE_VERIFY_PATTERN_2,
    PRODUCT_FRAM_TEST_STATE_PASSED,
    PRODUCT_FRAM_TEST_STATE_FAILED,
    PRODUCT_FRAM_TEST_STATE_ABORTED
} ProductFramBankTestState_t;

typedef enum
{
    PRODUCT_FRAM_TEST_ERROR_NONE = 0,
    PRODUCT_FRAM_TEST_ERROR_FACTORY_MODE_LOCKED,
    PRODUCT_FRAM_TEST_ERROR_NVM_BUSY,
    PRODUCT_FRAM_TEST_ERROR_GEOMETRY,
    PRODUCT_FRAM_TEST_ERROR_WRITE,
    PRODUCT_FRAM_TEST_ERROR_READ,
    PRODUCT_FRAM_TEST_ERROR_VERIFY
} ProductFramBankTestError_t;

typedef struct
{
    ProductFramBankTestState_t state;
    ProductFramBankTestError_t error;
    uint8_t current_bank;
    uint16_t completed_bank_mask;
    uint16_t failed_bank_mask;
    uint16_t progress_permille;
    uint32_t current_address;
    uint32_t failure_address;
    uint16_t expected_value;
    uint16_t actual_value;
} ProductFramBankTestSnapshot_t;

void ProductFramBankTest_Initialize(void);
bool ProductFramBankTest_Start(void);
void ProductFramBankTest_Abort(void);
void ProductFramBankTest_Process(void);
bool ProductFramBankTest_IsBusy(void);
void ProductFramBankTest_GetSnapshot(ProductFramBankTestSnapshot_t *snapshot);

#endif
