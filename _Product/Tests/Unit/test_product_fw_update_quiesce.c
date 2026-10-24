#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include "fsl_device_registers.h"
#include "bsp_analog_output.h"
#include "product_fw_update.h"
#include "ProductSafetyTestConfig.h"

TestTimer timers[4];
TestGpio gpio;
TestPmc pmc;
TestSyscon syscon;
static unsigned cleared, muxed, dac_writes;
static bool dac_ready;
uint32_t __get_PRIMASK(void) { return 1U; }
void __disable_irq(void) {}
void __set_PRIMASK(uint32_t value) { assert(value == 1U); }
void __DSB(void) {}
void __WFI(void) { assert(false); }
void ProductUsbCdc_Stop(void) { assert(false); }
void GPIO_PortClear(TestGpio *base, unsigned port, uint32_t mask)
{ assert(base == &gpio && port <= 1U && mask); cleared++; }
void IOCON_PinMuxSet(void *base, unsigned port, unsigned pin, unsigned mode)
{ (void)base; (void)mode; assert(port <= 1U && pin <= 31U); muxed++; }
bool BspAnalogOutput_WriteCode(BspAnalogOutput_t output, uint16_t code)
{ assert((unsigned)output < 4U && code == 0U); dac_writes++; return dac_ready; }

int main(void)
{
    for (unsigned i=0;i<4;i++) timers[i].TCR=1U;
    bool ok=ProductFwUpdate_Quiesce();
    for (unsigned i=0;i<4;i++) assert(timers[i].TCR == 0U);
    assert(cleared == 8U && muxed == 8U);
#if PRODUCT_SAFETY_TEST_EVK
    assert(ok && dac_writes == 0U); /* Unregistered/missing DAC cannot block EVK. */
#else
    assert(!ok && dac_writes == 4U); /* Production still fails closed. */
    dac_ready=true;
    assert(ProductFwUpdate_Quiesce() && dac_writes == 8U);
#endif
    return 0;
}
