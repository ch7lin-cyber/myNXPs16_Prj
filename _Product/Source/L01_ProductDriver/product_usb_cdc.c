#include "product_usb_cdc.h"
#include "usb_device_config.h"
#include "usb.h"
#include "usb_device.h"
#include "usb_device_class.h"
#include "usb_device_cdc_acm.h"
#include "usb_device_descriptor.h"
#include "fsl_clock.h"
#include "fsl_power.h"
#include "fsl_reset.h"
#include "fsl_iocon.h"
#include <string.h>

extern usb_device_class_struct_t g_UsbDeviceCdcVcomConfig;
static usb_device_handle device;
static class_handle_t cdc;
static volatile bool configured, rx_pending, rx_armed, tx_busy, tx_done;
static volatile uint32_t session, rx_size;
static bool tx_zlp;
USB_DMA_NONINIT_DATA_ALIGN(USB_DATA_ALIGN_SIZE) static uint8_t rx[64];
USB_DMA_NONINIT_DATA_ALIGN(USB_DATA_ALIGN_SIZE) static uint8_t tx[256];
/* CDC line coding: 115200 little endian, 1 stop bit, no parity, 8 data bits. */
static const uint8_t fixed_coding[7]={0,0xC2,1,0,0,0,8};
USB_DMA_INIT_DATA_ALIGN(USB_DATA_ALIGN_SIZE) static uint8_t coding[7]={0,0xC2,1,0,0,0,8};

/* Bare-metal critical sections; mutex lifecycle hooks are in product_usb_osa.c. */
void OSA_EnterCritical(uint32_t *sr) { *sr=__get_PRIMASK(); __disable_irq(); }
void OSA_ExitCritical(uint32_t sr) { __set_PRIMASK(sr); }

static usb_status_t ClassCallback(class_handle_t h, uint32_t event, void *param)
{
    (void)h;
    if(event==kUSB_DeviceCdcEventRecvResponse) {
        usb_device_endpoint_callback_message_struct_t *m=param;
        rx_armed=false;
        if(configured && m->length>0 && m->length<=sizeof rx) {
            rx_size=m->length; rx_pending=true;
        }
        return kStatus_USB_Success;
    }
    if(event==kUSB_DeviceCdcEventSendResponse) { tx_done=true; return kStatus_USB_Success; }
    if(event==kUSB_DeviceCdcEventSetControlLineState || event==kUSB_DeviceCdcEventSerialStateNotif)
        return kStatus_USB_Success;
    if(event==kUSB_DeviceCdcEventGetLineCoding || event==kUSB_DeviceCdcEventSetLineCoding) {
        usb_device_cdc_acm_request_param_struct_t *r=param;
        if(r->isSetup) {
            memcpy(coding,fixed_coding,sizeof coding);
            *r->buffer=coding; *r->length=sizeof coding;
        } else if(event==kUSB_DeviceCdcEventSetLineCoding) {
            /* Accept the control transfer so Windows can open the port, but
             * always retain/report 115200 8N1. Never reconfigure FC0. */
            memcpy(coding,fixed_coding,sizeof coding);
        }
        return kStatus_USB_Success;
    }
    return kStatus_USB_InvalidRequest;
}
static void ResetSession(void)
{
    rx_pending=false; rx_armed=false; tx_busy=false; tx_done=false;
    tx_zlp=false; session++;
}
static usb_status_t DeviceCallback(usb_device_handle h, uint32_t event, void *param)
{
    switch(event) {
    case kUSB_DeviceEventBusReset:
        configured=false; ResetSession(); return USB_DeviceSetSpeed(h,USB_SPEED_FULL);
    case kUSB_DeviceEventSetConfiguration:
        if(*(uint8_t *)param>1) return kStatus_USB_InvalidRequest;
        configured=(*(uint8_t *)param==1); ResetSession(); return kStatus_USB_Success;
    case kUSB_DeviceEventGetConfiguration:
        *(uint8_t *)param=configured?1:0; return kStatus_USB_Success;
    case kUSB_DeviceEventGetDeviceDescriptor: return USB_DeviceGetDeviceDescriptor(h,param);
    case kUSB_DeviceEventGetConfigurationDescriptor: return USB_DeviceGetConfigurationDescriptor(h,param);
    case kUSB_DeviceEventGetStringDescriptor: return USB_DeviceGetStringDescriptor(h,param);
    case kUSB_DeviceEventSetInterface:
        return (*(uint16_t *)param==0 || *(uint16_t *)param==0x100)?kStatus_USB_Success:kStatus_USB_InvalidRequest;
    case kUSB_DeviceEventGetInterface:
        if((*(uint16_t *)param>>8)>1) return kStatus_USB_InvalidRequest;
        *(uint16_t *)param&=0xFF00; return kStatus_USB_Success;
    default: return kStatus_USB_InvalidRequest;
    }
}
static usb_device_class_config_struct_t class_config={ClassCallback,NULL,&g_UsbDeviceCdcVcomConfig};
static usb_device_class_config_list_struct_t class_list={&class_config,DeviceCallback,1};
void USB0_IRQHandler(void) { if(device) USB_DeviceLpcIp3511IsrFunction(device); }

bool ProductUsbCdc_Init(void)
{
    CLOCK_EnableClock(kCLOCK_Iocon);
    IOCON_PinMuxSet(IOCON,0,22,IOCON_FUNC7|IOCON_MODE_INACT|IOCON_DIGITAL_EN);
    POWER_DisablePD(kPDRUNCFG_PD_USB0_PHY);
    RESET_PeripheralReset(kUSB0D_RST_SHIFT_RSTn);
    RESET_PeripheralReset(kUSB0HSL_RST_SHIFT_RSTn);
    RESET_PeripheralReset(kUSB0HMR_RST_SHIFT_RSTn);
    /* Product board has no crystal: FRO 96 MHz / 2 supplies USB FS 48 MHz.
     * The SDK enables USBCLKADJ for USB clock recovery in this mode. */
    if(!CLOCK_EnableUsbfs0DeviceClock(kCLOCK_UsbfsSrcFro,96000000U)) return false;
    CLOCK_EnableClock(kCLOCK_Usbhsl0);
    USBFSH->PORTMODE |= USBFSH_PORTMODE_DEV_ENABLE_MASK;
    CLOCK_DisableClock(kCLOCK_Usbhsl0);
    if(USB_DeviceClassInit(kUSB_ControllerLpcIp3511Fs0,&class_list,&device)!=kStatus_USB_Success)
        return false;
    cdc=class_config.classHandle;
    NVIC_ClearPendingIRQ(USB0_IRQn);
    NVIC_SetPriority(USB0_IRQn,3U);
    EnableIRQ(USB0_IRQn);
    if(USB_DeviceRun(device)!=kStatus_USB_Success) {
        DisableIRQ(USB0_IRQn); (void)USB_DeviceClassDeinit(kUSB_ControllerLpcIp3511Fs0);
        device=NULL; return false;
    }
    return true;
}
void ProductUsbCdc_Process(void)
{
    uint32_t sr=__get_PRIMASK(); __disable_irq();
    if(configured) {
        if(tx_done) {
            tx_done=false;
            if(tx_zlp) {
                /* Terminate exact-64-byte bulk transfers for host read completion. */
                if(USB_DeviceCdcAcmSend(cdc,USB_CDC_VCOM_BULK_IN_ENDPOINT,tx,0)==kStatus_USB_Success)
                    tx_zlp=false;
                else tx_done=true;
            } else tx_busy=false;
        }
        if(!rx_armed && !rx_pending) {
            rx_armed=true;
            if(USB_DeviceCdcAcmRecv(cdc,USB_CDC_VCOM_BULK_OUT_ENDPOINT,rx,sizeof rx)!=kStatus_USB_Success)
                rx_armed=false;
        }
    }
    __set_PRIMASK(sr);
}
size_t ProductUsbCdc_Read(uint8_t *buffer, size_t capacity)
{
    size_t n=0;
    uint32_t sr=__get_PRIMASK(); __disable_irq();
    if(configured && rx_pending && buffer && capacity>=rx_size) {
        n=rx_size; memcpy(buffer,rx,n); rx_pending=false;
    }
    __set_PRIMASK(sr); return n;
}
bool ProductUsbCdc_Send(const uint8_t *buffer, size_t length, uint32_t expected_session)
{
    bool ok=false;
    uint32_t sr=__get_PRIMASK(); __disable_irq();
    if(configured && session==expected_session && !tx_busy && buffer && length>0 && length<=sizeof tx) {
        memcpy(tx,buffer,length); tx_busy=true; tx_done=false; tx_zlp=(length%64U)==0U;
        ok=USB_DeviceCdcAcmSend(cdc,USB_CDC_VCOM_BULK_IN_ENDPOINT,tx,length)==kStatus_USB_Success;
        if(!ok) { tx_busy=false; tx_zlp=false; }
    }
    __set_PRIMASK(sr); return ok;
}
bool ProductUsbCdc_TxReady(void) { return configured && !tx_busy; }
uint32_t ProductUsbCdc_Session(void) { return session; }
