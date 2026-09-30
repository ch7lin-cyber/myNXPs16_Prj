#include "bsp_analog_output.h"

#include <stddef.h>

#include "HalDac.h"
#include "ProductDacConfig.h"

static bool IsOutputValid(BspAnalogOutput_t output)
{
    return ((uint32_t)output < BSP_ANALOG_OUTPUT_COUNT);
}

bool BspAnalogOutput_Initialize(void)
{
    uint8_t output;

    for (output = 0U; output < BSP_ANALOG_OUTPUT_COUNT; output++)
    {
        if (HalDac_Initialize(output) != HAL_DAC_STATUS_OK)
        {
            return false;
        }
    }
    return true;
}

bool BspAnalogOutput_WriteCode(BspAnalogOutput_t output, uint16_t code)
{
    return IsOutputValid(output) &&
           (HalDac_WriteCode((uint8_t)output, code) == HAL_DAC_STATUS_OK);
}

bool BspAnalogOutput_WriteDacMicrovolts(BspAnalogOutput_t output,
                                        uint32_t microvolts)
{
    uint32_t code;

    if (!IsOutputValid(output) ||
        (microvolts > PRODUCT_DAC_NOMINAL_FULL_SCALE_UV))
    {
        return false;
    }
    code = (uint32_t)((((uint64_t)microvolts * UINT16_MAX) +
                       (PRODUCT_DAC_NOMINAL_FULL_SCALE_UV / 2U)) /
                      PRODUCT_DAC_NOMINAL_FULL_SCALE_UV);
    return HalDac_WriteCode((uint8_t)output, (uint16_t)code) ==
           HAL_DAC_STATUS_OK;
}

bool BspAnalogOutput_GetLastCode(BspAnalogOutput_t output, uint16_t *code)
{
    return IsOutputValid(output) && (code != NULL) &&
           (HalDac_GetLastCode((uint8_t)output, code) == HAL_DAC_STATUS_OK);
}
