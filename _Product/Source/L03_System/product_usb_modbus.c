#include "product_usb_modbus.h"
#include "product_fw_update.h"
#include "product_usb_cdc.h"
#include "product_modbus_register_adapter.h"
#include "ModbusRegisterAdapter.h"
#include "ModbusSlave.h"
#include "ModbusCrc16.h"
#include <string.h>

/* USB packets are not RTU frames. Accumulate by request length, not T3.5. */
static uint8_t stream[512], response[256];
static size_t used, response_length;
static bool response_inflight;
static uint32_t last_rx_ms, session;
static ModbusSlaveConfig_t slave;
static void Consume(size_t n)
{
    used-=n; memmove(stream,stream+n,used);
}
bool ProductUsbModbus_Init(void)
{
    used=0; response_length=0; response_inflight=false; last_rx_ms=0;
    ProductModbusRegisterAdapter_GetInterface(&slave.registers);
    session=ProductUsbCdc_Session();
    return ProductUsbCdc_Init();
}
void ProductUsbModbus_Process(uint32_t now_ms)
{
    uint8_t packet[64];
    size_t n, frame_length;
    uint32_t current_session;
    ModbusSerialPortConfiguration_t active;
    ProductUsbCdc_Process();
    current_session=ProductUsbCdc_Session();
    if(current_session!=session) {
        session=current_session; used=0; response_length=0;response_inflight=false;
    }
    if(response_inflight && ProductUsbCdc_TxReady()) {response_inflight=false;ProductFwUpdate_ReplySent(1);}
    if(response_length) {
        if(ProductUsbCdc_Send(response,response_length,session)) {response_length=0;response_inflight=true;}
        return;
    }
    if(!ProductUsbCdc_TxReady()) return;
    if(used && (uint32_t)(now_ms-last_rx_ms)>=1000U) used=0;
    /* Backpressure: do not consume USB OUT unless the stream has room. */
    n=(sizeof stream-used>=sizeof packet)?ProductUsbCdc_Read(packet,sizeof packet):0;
    if(n) { memcpy(stream+used,packet,n); used+=n; last_rx_ms=now_ms; }
    while(used>=2U) {
        if(stream[0]>247U) { Consume(1); continue; }
        switch(stream[1]) {
        case 3: case 4: case 6: frame_length=8; break;
        case 16:
            if(used<7U) return;
            frame_length=9U+stream[6];
            if(frame_length>256U) { Consume(1); continue; }
            break;
        default:
            /* Unknown function ADU is delimited by the 1 s receive timeout.
             * Discard its prefix; never interpret USB packet edges as frames. */
            Consume(1); continue;
        }
        if(used<frame_length) return;
        if(!ModbusCrc16_IsFrameValid(stream,frame_length)) { Consume(1); continue; }
        if(!ModbusRegisterAdapter_GetActiveSerialConfiguration(0U,&active)) return;
        slave.unit_address=(uint8_t)active.unit_id;
        ProductFwUpdate_SetRequestContext(stream[0]?1:0,slave.unit_address);
        (void)ModbusSlave_ProcessRtuRequest(&slave,stream,frame_length,
                                           response,sizeof response,&response_length);
        ProductFwUpdate_SetRequestContext(0,0);
        Consume(frame_length);
        if(response_length) {
            /* Never transmit a stale reply into a newly enumerated session. */
            if(ProductUsbCdc_Session()!=session) { used=0; response_length=0; return; }
            if(ProductUsbCdc_Send(response,response_length,session)) {response_length=0;response_inflight=true;}
            return;
        }
    }
}
