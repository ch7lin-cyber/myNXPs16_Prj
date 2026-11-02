#include "product_can_driver.h"
#include "fsl_mcan.h"
#include <stdio.h>
#include "product_can_test.h"
static ProductCanDiagnostics_t diag(void) { ProductCanDiagnostics_t d;assert(ProductCanDriver_GetDiagnostics(&d));return d; }
static void init(void) { Mock_Reset();assert(ProductCanDriver_Init()); }
static ProductCanFrame_t frame(uint16_t id,uint8_t len,uint8_t value) { ProductCanFrame_t f={0};f.identifier=id;f.length=len;memset(f.data,value,8);return f; }
static void boundaries(void) {
 ProductCanFrame_t f=frame(0,0,0),out;
 Mock_Reset();assert(!ProductCanDriver_Send(&f));assert(!ProductCanDriver_GetDiagnostics(NULL));
 mock_timing_ok=false;assert(!ProductCanDriver_Init());ProductCanDriver_Process();assert(mock_requests==0);
 Mock_Reset();mock_ram_ok=false;assert(!ProductCanDriver_Init());assert(mock_deinits==1);
 init();assert(mock_config.enableLoopBackInt==(PRODUCT_CAN_INTERNAL_LOOPBACK!=0));
 assert(!mock_config.enableLoopBackExt && !mock_config.enableCanfdNormal && !mock_config.enableCanfdSwitch && !mock_config.enableBusMon);
 assert(!ProductCanDriver_Receive(NULL));assert(!ProductCanDriver_Receive(&out));
 assert(!ProductCanDriver_Send(NULL));f.identifier=0x800;assert(!ProductCanDriver_Send(&f));
 f.identifier=0xffff;assert(!ProductCanDriver_Send(&f));f.identifier=0;f.length=9;assert(!ProductCanDriver_Send(&f));f.length=255;assert(!ProductCanDriver_Send(&f));assert(diag().tx_invalid_frame==5);
 for(unsigned len=0;len<=8;len++) {
  f=frame(len==8?0x7ff:0,len,0xa5);assert(ProductCanDriver_Send(&f));f.data[0]=0;
  ProductCanDriver_Process();assert(mock_tx.id==((len==8?0x7ffU:0U)<<18));assert(mock_tx.dlc==len && mock_tx.size==len);
  assert(mock_tx.xtd==0 && mock_tx.rtr==0 && mock_tx.fdf==0 && mock_tx.brs==0);
  for(unsigned i=0;i<8;i++)assert(mock_tx_data[i]==(i<len?0xa5:0));
  Mock_Complete();ProductCanDriver_Process();ProductCanDriver_Process();
 }
 assert(diag().tx_completed==9 && diag().tx_queued==9);
 for(unsigned len=0;len<=8;len++) { Mock_Push(len==8?0x7ff:0,len,0,0x5a);ProductCanDriver_Process();assert(ProductCanDriver_Receive(&out));assert(out.identifier==(len==8?0x7ff:0) && out.length==len);for(unsigned i=0;i<len;i++)assert(out.data[i]==0x5a); }
 for(unsigned flags=1;flags<=8;flags<<=1)Mock_Push(1,8,flags,0);
 for(unsigned len=9;len<=15;len++)Mock_Push(1,len,0,0);
 ProductCanDriver_Process();assert(diag().rx_invalid_frame==11 && diag().rx_received==9);assert(!ProductCanDriver_Receive(&out));
 puts("CAN init failures, configuration, ID/DLC boundaries, payload copying, invalid RX/TX: PASS");
}
static void queues(void) {
 ProductCanFrame_t f,out;init();
 for(unsigned round=0;round<3;round++) {
  for(unsigned i=0;i<8;i++) {f=frame(i,8,i);assert(ProductCanDriver_Send(&f));}
  assert(!ProductCanDriver_Send(&f));assert(diag().tx_queue_pending==8);
  mock_write_ok=false;ProductCanDriver_Process();assert(diag().tx_queue_pending==8 && mock_requests==round*8);
  mock_write_ok=true;
  for(unsigned i=0;i<8;i++) {ProductCanDriver_Process();assert(mock_tx.id==(i<<18) && mock_tx_data[0]==i);unsigned n=mock_requests;ProductCanDriver_Process();assert(mock_requests==n);Mock_Complete();}
  ProductCanDriver_Process();assert(diag().tx_queue_pending==0 && !diag().tx_in_flight);
 }
 assert(diag().tx_completed==24 && diag().tx_queue_full==3);
 for(unsigned round=0;round<3;round++) {
  for(unsigned i=0;i<16;i++)Mock_Push(i,8,0,i);
  mock_read_ok=false;ProductCanDriver_Process();assert(mock_can.RXF0S==16);mock_read_ok=true;
  ProductCanDriver_Process();Mock_Push(100,1,0,100);ProductCanDriver_Process();
  assert(!ProductCanDriver_Receive(NULL));
  for(unsigned i=0;i<16;i++){assert(ProductCanDriver_Receive(&out));assert(out.identifier==i && out.data[0]==i);}
  assert(!ProductCanDriver_Receive(&out));
 }
 assert(diag().rx_received==48 && diag().rx_queue_dropped==3);
 assert(ProductCanDriver_Init());assert(diag().tx_queued==0 && diag().rx_received==0);assert(!ProductCanDriver_Receive(&out));
 puts("CAN TX/RX FIFO ordering, wrap, full, pending, read/write failure, reset: PASS");
}
static void errors(void) {
 init();mock_can.ECR=200U|(100U<<8);mock_can.PSR=CAN_PSR_EW_MASK|CAN_PSR_EP_MASK|3;
 mock_can.IR=CAN_IR_EW_MASK|CAN_IR_EP_MASK|CAN_IR_PEA_MASK|CAN_IR_PED_MASK|CAN_IR_RF0L_MASK;
 ProductCanDriver_Process();ProductCanDiagnostics_t d=diag();
 assert(d.tx_error_count==200 && d.rx_error_count==100 && d.last_protocol_error==3);
 assert(d.controller_state==PRODUCT_CAN_STATE_ERROR_PASSIVE && d.error_warning_events==1 && d.error_passive_events==1 && d.protocol_error_events==1 && d.rx_fifo_lost_events==1 && d.rx_fifo_lost);
 ProductCanDriver_Process();assert(diag().protocol_error_events==1 && diag().rx_fifo_lost_events==1);
 mock_can.PSR=CAN_PSR_EW_MASK;ProductCanDriver_Process();assert(diag().controller_state==PRODUCT_CAN_STATE_ERROR_WARNING);
 mock_can.PSR=0;ProductCanDriver_Process();assert(diag().controller_state==PRODUCT_CAN_STATE_ERROR_ACTIVE);
 for(unsigned cycle=0;cycle<2;cycle++) {
  mock_can.PSR=CAN_PSR_BO_MASK|CAN_PSR_EP_MASK;mock_can.IR=CAN_IR_BO_MASK;
  for(unsigned i=0;i<8;i++){ProductCanFrame_t f=frame(i,1,i);assert(ProductCanDriver_Send(&f));}
  ProductCanFrame_t f=frame(1,1,1);assert(!ProductCanDriver_Send(&f));
  ProductCanDriver_Process();ProductCanDriver_Process();assert(diag().bus_off && diag().controller_state==PRODUCT_CAN_STATE_BUS_OFF && diag().tx_queue_pending==8 && mock_requests==cycle*8);
  assert(diag().bus_off_events==cycle+1 && diag().bus_off_recoveries==cycle);
  Mock_Push(0x123,1,0,0xab);ProductCanDriver_Process();assert(ProductCanDriver_Receive(&f) && f.data[0]==0xab);
  mock_can.PSR=0;mock_can.IR=CAN_IR_BO_MASK;ProductCanDriver_Process();assert(diag().bus_off_recoveries==cycle+1);
  for(unsigned i=0;i<8;i++) {assert(mock_tx.id==(i<<18));Mock_Complete();ProductCanDriver_Process();}
  assert(diag().tx_queue_pending==0);
 }
 /* Existing hardware request stays pending across bus-off; no duplicate submit. */
 ProductCanFrame_t f=frame(0x321,1,0x12);assert(ProductCanDriver_Send(&f));ProductCanDriver_Process();
 assert(ProductCanDriver_Send(&f));unsigned n=mock_requests;mock_can.PSR=CAN_PSR_BO_MASK;ProductCanDriver_Process();mock_can.PSR=0;ProductCanDriver_Process();assert(mock_requests==n && diag().tx_queue_pending==1);Mock_Complete();ProductCanDriver_Process();assert(mock_requests==n+1);
 assert(ProductCanDriver_Init());assert(!diag().rx_fifo_lost && diag().bus_off_events==0);
 puts("CAN TEC/REC, W1C event accounting, passive/warning, bus-off pause/resume: PASS");
}

#if PRODUCT_CAN_TEST_BUILD
static void diagnostic(void) {
 init();memset((void *)&g_can_test_watch,0,sizeof g_can_test_watch);
 g_can_test_watch.requested_frames=2;
 g_can_test_watch.command=PRODUCT_CAN_INTERNAL_LOOPBACK?1:2;
 ProductCanTest_Process(0);assert(g_can_test_watch.state==1 && g_can_test_watch.submitted==1);
 for(unsigned seq=0;seq<2;seq++) {
  ProductCanDriver_Process();Mock_Complete();
  /* Mock does not auto-loopback; inject the independently expected payload. */
  uint8_t payload[8]={(uint8_t)seq,0,0,0,0xa5,0x5a,0,0xff};
  Mock_PushData(PRODUCT_CAN_INTERNAL_LOOPBACK?0x321:0x322,payload);
  ProductCanDriver_Process();ProductCanTest_Process(seq+1);
 }
 assert(g_can_test_watch.state==2 && g_can_test_watch.matched==2 && g_can_test_watch.submitted==2);
#if !PRODUCT_CAN_INTERNAL_LOOPBACK
 init();memset((void *)&g_can_test_watch,0,sizeof g_can_test_watch);
 g_can_test_watch.command=3;ProductCanTest_Process(0);
 uint8_t payload[8]={0,0,0,0,0xa5,0x5a,0,0xff};Mock_PushData(0x321,payload);
 ProductCanDriver_Process();ProductCanTest_Process(1);ProductCanDriver_Process();
 assert(mock_tx.id==(0x322U<<18) && memcmp(mock_tx_data,payload,8)==0);
 Mock_Complete();ProductCanDriver_Process();ProductCanTest_Process(2);assert(g_can_test_watch.state==2);
#endif
 init();memset((void *)&g_can_test_watch,0,sizeof g_can_test_watch);
 g_can_test_watch.command=PRODUCT_CAN_INTERNAL_LOOPBACK?1:2;ProductCanTest_Process(0);
 Mock_Push(0x321,8,0,0);ProductCanDriver_Process();ProductCanTest_Process(1);
 assert(g_can_test_watch.state==3 && g_can_test_watch.unexpected==1);
 init();memset((void *)&g_can_test_watch,0,sizeof g_can_test_watch);
 g_can_test_watch.command=PRODUCT_CAN_INTERNAL_LOOPBACK?1:2;
 ProductCanTest_Process(UINT32_MAX-10U);ProductCanTest_Process(4990U);
 assert(g_can_test_watch.state==3 && g_can_test_watch.timeouts==1);
 mock_can.PSR=CAN_PSR_BO_MASK;ProductCanDriver_Process();g_can_test_watch.command=4;
 ProductCanTest_Process(5000);assert(g_can_test_watch.recovery_requests==1);
 g_can_test_watch.command=5;ProductCanTest_Process(5001);assert(g_can_test_watch.state==0);
 puts("CAN diagnostic data matching, mismatch, timeout/wrap, recovery, stop: PASS");
}
#endif
int main(void) {boundaries();queues();errors();
#if PRODUCT_CAN_TEST_BUILD
 diagnostic();
#endif
return 0;}

