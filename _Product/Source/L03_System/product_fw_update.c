#include "product_fw_update.h"
#include "ProductConfig.h"
#include "../../Shared/boot_request.h"
static uint16_t context_route,context_unit,route,unit,state;
static uint32_t baud;
static uint32_t started;
static bool reply_sent,clock_started;
void ProductFwUpdate_Init(void) {
    context_route=context_unit=route=unit=state=0;baud=115200u;started=0;reply_sent=clock_started=false;
}
void ProductFwUpdate_SetRequestContext(uint16_t r,uint16_t u) { context_route=r;context_unit=u; }
uint8_t ProductFwUpdate_Read(uint16_t a,uint16_t n,uint16_t *v) {
    const uint16_t id[10]={0xA016,1,1,1,0xC200,8,0,1,0,1};
    if(!v || !n)return 3;
    if(a>=0x7000u && (uint32_t)a+n<=0x700Au) {
        for(unsigned i=0;i<n;i++)v[i]=id[a-0x7000u+i];
        return 0;
    }
    if(a==0x7051u && n==3u) {v[0]=state;v[1]=route;v[2]=unit;return 0;}
    return 2;
}
uint8_t ProductFwUpdate_Write(uint16_t a,uint16_t value) {
    if(a==0x7052u) {
        static const uint32_t rates[]={38400u,57600u,115200u,230400u,460800u,921600u};
        if(context_route!=2u || context_unit<1u || context_unit>247u ||
           value>=sizeof rates/sizeof rates[0] || state)return 3;
        baud=rates[value];return 0;
    }
    uint16_t requested=value==BOOT_REQUEST_USB_KEY?BOOT_REQUEST_USB:
        value==BOOT_REQUEST_RS485_KEY?BOOT_REQUEST_RS485:0;
    if(a!=BOOT_REQUEST_REGISTER)return 2;
    if(!requested || context_route!=requested || context_unit<1 || context_unit>247)return 3;
    if(state)return 6;
    if(requested==BOOT_REQUEST_USB)baud=115200u;
    /* Interface is intentionally RTU 115200/8N1 after reset. The caller must
       finish the original-format reply before switching the line. */
    route=requested;unit=context_unit;state=1;return 0;
}
bool ProductFwUpdate_IsPending(void) {return state!=0;}
void ProductFwUpdate_ReplySent(uint16_t r) {if(state && r==route)reply_sent=true;}
void ProductFwUpdate_Process(uint32_t now) {
    if(!state || state==4)return;
    if(!clock_started){started=now;clock_started=true;}
    if(state==1) {
        if(!ProductFwUpdate_Quiesce()){state=4;return;}
        state=2;
    }
    /* No ACK => remain safely stopped. Never reset on a timer alone. */
    if(reply_sent && (uint32_t)(now-started)>=200u) {
        state=3;ProductFwUpdate_Reset(route,unit,baud);
        state=4; /* A production reset must not return. */
    }
}
