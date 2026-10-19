#include "product_fw_update.h"
#include "../../Shared/boot_request.h"
#include "bsp_analog_output.h"
#include "fsl_device_registers.h"
#include "fsl_iocon.h"
#include "fsl_gpio.h"
#include "product_usb_cdc.h"
/* Same eight active-high outputs as 005. CS, USB and SWD are excluded. */
bool ProductFwUpdate_Quiesce(void) {
    const uint8_t pins[][2]={{0,0},{0,18},{1,19},{1,22},{0,19},{0,26},{0,25},{1,25}};
    uint32_t irq=__get_PRIMASK();__disable_irq();
    CTIMER0->TCR=0;CTIMER1->TCR=0;CTIMER2->TCR=0;CTIMER3->TCR=0;
    for(unsigned i=0;i<8;i++) {
        GPIO_PortClear(GPIO,pins[i][0],1u<<pins[i][1]);
        IOCON_PinMuxSet(IOCON,pins[i][0],pins[i][1],IOCON_FUNC0|IOCON_MODE_PULLDOWN|IOCON_DIGITAL_EN);
        GPIO->DIRSET[pins[i][0]]=1u<<pins[i][1];
    }
    __DSB();__set_PRIMASK(irq);
    bool ok=true;
    for(unsigned i=0;i<4;i++)ok=BspAnalogOutput_WriteCode((BspAnalogOutput_t)i,0u)&&ok;
    return ok;
}
void ProductFwUpdate_Reset(uint16_t route,uint16_t unit,uint32_t baud) {
    uint32_t words[16];BootRequest_Encode(words,route,unit,baud);
    /* Force an actual detach while IRQs/tick still work, before boot re-enumerates. */
    if(route==BOOT_REQUEST_USB)ProductUsbCdc_Stop();
    volatile uint32_t *request=(volatile uint32_t *)BOOT_REQUEST_ADDRESS;
    request[0]=0;
    for(unsigned i=1;i<16;i++)request[i]=words[i];
    __DSB();request[0]=words[0];__DSB();
    PMC->RESETCTRL |= PMC_RESETCTRL_SWRRESETENABLE_MASK;
    __DSB();
    SYSCON->SWR_RESET=0x5A000001u;
    for(;;)__WFI();
}
