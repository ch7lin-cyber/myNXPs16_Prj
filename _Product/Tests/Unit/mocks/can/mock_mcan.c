#include "fsl_mcan.h"
CAN_Type mock_can;
uint8_t mock_message_ram[4096],mock_tx_data[8];
bool mock_timing_ok,mock_ram_ok,mock_write_ok,mock_read_ok,mock_pending;
unsigned mock_writes,mock_requests,mock_deinits;
mcan_config_t mock_config;
mcan_tx_buffer_frame_t mock_tx;
static mcan_rx_buffer_frame_t rx[16];
static uint8_t data[16][8];
static unsigned head,tail,count;
void Mock_Reset(void) {
 memset(&mock_can,0,sizeof mock_can); memset(&mock_config,0,sizeof mock_config);
 mock_timing_ok=mock_ram_ok=mock_write_ok=mock_read_ok=true;mock_pending=false;
 mock_writes=mock_requests=mock_deinits=head=tail=count=0;
}
void Mock_Push(uint16_t id,uint8_t dlc,uint8_t flags,uint8_t value) {
 assert(count<16);memset(&rx[head],0,sizeof rx[head]);memset(data[head],value,8);
 rx[head].id=(uint32_t)id<<18;rx[head].dlc=dlc;rx[head].xtd=flags&1;
 rx[head].rtr=(flags>>1)&1;rx[head].fdf=(flags>>2)&1;rx[head].brs=(flags>>3)&1;
 rx[head].data=data[head];head=(head+1)%16;count++;mock_can.RXF0S=count;
}
void Mock_PushData(uint16_t id,const uint8_t *payload) { Mock_Push(id,8,0,0);memcpy(data[(head+15)%16],payload,8); }
void Mock_Complete(void) { assert(mock_pending);mock_pending=false;mock_can.IR|=kMCAN_TxTransmitCompleteFlag; }
void MCAN_GetDefaultConfig(mcan_config_t *c) { memset(c,0,sizeof *c); }
bool MCAN_CalculateImprovedTimingValues(uint32_t b,uint32_t h,uint32_t *t) { assert(b==500000 && h==15000000);*t=1;return mock_timing_ok; }
void MCAN_Init(CAN_Type *b,const mcan_config_t *c,uint32_t h) { (void)b;assert(h==15000000);mock_config=*c; }
void MCAN_Deinit(CAN_Type *b) { (void)b;mock_deinits++; }
int MCAN_SetMessageRamConfig(CAN_Type *b,const mcan_memory_config_t *m) {
 (void)b;assert(m->baseAddr==(uintptr_t)mock_message_ram);
 assert(m->stdFilterCfg->listSize==1 && m->rxFifo0Cfg->elementSize==16);
 assert(m->rxFifo0Cfg->address==0x10 && m->txBufferCfg->address==0x120);
 assert(m->txBufferCfg->dedicatedSize==1);return mock_ram_ok?0:1;
}
void MCAN_EnterInitialMode(CAN_Type *b) { (void)b; }
void MCAN_EnterNormalMode(CAN_Type *b) { (void)b; }
void MCAN_SetSTDFilterElement(CAN_Type *b,const mcan_frame_filter_config_t *c,const mcan_std_filter_element_config_t *e,uint8_t i) { (void)b;(void)c;assert(i==0 && e->sfid1==0 && e->sfid2==0x7ff && e->sfec==kMCAN_storeinFifo0); }
void MCAN_ClearStatusFlag(CAN_Type *b,uint32_t f) { b->IR&=~f; }
uint32_t MCAN_GetStatusFlag(CAN_Type *b,uint32_t f) { return b->IR&f; }
uint32_t MCAN_IsTransmitRequestPending(CAN_Type *b,uint8_t i) { (void)b;assert(i==0);return mock_pending; }
int MCAN_WriteTxBuffer(CAN_Type *b,uint8_t i,const mcan_tx_buffer_frame_t *f) {
 (void)b;assert(i==0 && !mock_pending);mock_writes++;
 if(!mock_write_ok)return 1;
 mock_tx=*f;memcpy(mock_tx_data,f->data,8);mock_tx.data=mock_tx_data;return 0;
}
void MCAN_TransmitAddRequest(CAN_Type *b,uint8_t i) { (void)b;assert(i==0);mock_requests++;mock_pending=true; }
int MCAN_ReadRxFifo(CAN_Type *b,uint8_t i,mcan_rx_buffer_frame_t *f) {
 assert(i==0 && count);if(!mock_read_ok)return 1;
 *f=rx[tail];tail=(tail+1)%16;count--;b->RXF0S=count;return 0;
}
