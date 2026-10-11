#include "product_usb_modbus.h"
#include "product_usb_cdc.h"
#include "product_modbus_register_adapter.h"
#include "ModbusRegisterAdapter.h"
#include "ModbusCrc16.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
static uint8_t packet[64],reply[256];
static size_t packet_n,reply_n;
static uint32_t epoch;
static uint16_t shared_value=0x4357,unit=2;
static unsigned sends,writes;
static bool accept_send=true;
bool ProductUsbCdc_Init(void) {return true;}
void ProductUsbCdc_Process(void) {}
uint32_t ProductUsbCdc_Session(void) {return epoch;}
bool ProductUsbCdc_TxReady(void) {return true;}
size_t ProductUsbCdc_Read(uint8_t *b,size_t n) {
 assert(n>=packet_n); memcpy(b,packet,packet_n); n=packet_n;packet_n=0;return n;
}
bool ProductUsbCdc_Send(const uint8_t *b,size_t n,uint32_t expected_session) {
 if(!accept_send || expected_session!=epoch)return false;
 memcpy(reply,b,n);reply_n=n;sends++;return true;
}
static ModbusExceptionCode_t Read(void *c,uint16_t a,uint16_t n,uint16_t *v) {
 (void)c; if(a!=0x1000 || n!=1)return MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS;
 *v=shared_value;return MODBUS_EXCEPTION_NONE;
}
static ModbusExceptionCode_t Write(void *c,uint16_t a,uint16_t v) {
 (void)c; if(a!=0x1000)return MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS;
 shared_value=v;writes++;return MODBUS_EXCEPTION_NONE;
}
static ModbusExceptionCode_t WriteMany(void *c,uint16_t a,const uint16_t *v,uint16_t n) {
 if(n!=1)return MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE;
 return Write(c,a,*v);
}
void ProductModbusRegisterAdapter_GetInterface(ModbusSlaveRegisterInterface_t *v) {
 v->read_holding_registers=Read;v->read_input_registers=Read;
 v->write_single_register=Write;v->write_multiple_registers=WriteMany;v->context=NULL;
}
bool ModbusRegisterAdapter_GetActiveSerialConfiguration(uint8_t port,ModbusSerialPortConfiguration_t *v) {
 (void)port;memset(v,0,sizeof *v);v->unit_id=unit;return true;
}
static void Feed(const uint8_t *b,size_t n,uint32_t time) {
 assert(n<=64);memcpy(packet,b,n);packet_n=n;ProductUsbModbus_Process(time);
}
static void Crc(uint8_t *b,size_t n) {assert(ModbusCrc16_Append(b,n-2,n));}
int main(void) {
 uint8_t read[8]={2,3,0x10,0,0,1,0,0};
 uint8_t write[8]={2,6,0x10,0,0x12,0x34,0,0};
 uint8_t many[11]={2,16,0x10,0,0,1,2,0xAB,0xCD,0,0};
 uint8_t pair[16];
 Crc(read,8);Crc(write,8);Crc(many,11);
 assert(ProductUsbModbus_Init());
 Feed(read,3,0);assert(sends==0);
 Feed(read+3,5,1);assert(sends==1 && reply_n==7 && reply[3]==0x43 && reply[4]==0x57);
 assert(ModbusCrc16_IsFrameValid(reply,reply_n));
 Feed(write,8,2);assert(shared_value==0x1234 && writes==1 && sends==2);
 Feed(many,7,3);Feed(many+7,4,4);assert(shared_value==0xABCD && writes==2);
 memcpy(pair,read,8);memcpy(pair+8,read,8);
 Feed(pair,16,5);unsigned before=sends;ProductUsbModbus_Process(6);assert(sends==before+1);
 write[0]=0;Crc(write,8);before=sends;Feed(write,8,7);assert(sends==before && writes==3);
 read[0]=9;Crc(read,8);Feed(read,8,8);assert(sends==before);
 unit=9;Feed(read,8,9);assert(sends==before+1);
 read[6]^=1;before=sends;Feed(read,8,10);assert(sends==before);
 ProductUsbModbus_Process(1011);read[6]^=1;
 Feed(read,3,1012);epoch++;ProductUsbModbus_Process(1013);
 Feed(read,8,1014);assert(sends==before+1);
 Feed(read,3,1015);ProductUsbModbus_Process(2016);Feed(read,8,2017);assert(sends==before+2);
 accept_send=false;Feed(read,8,2018);assert(sends==before+2);
 accept_send=true;ProductUsbModbus_Process(2019);assert(sends==before+3);
 accept_send=false;Feed(read,8,2020);epoch++;accept_send=true;
 ProductUsbModbus_Process(2021);assert(sends==before+3);
 uint8_t large[255]={9,16,0x10,0,0,123,246};
 Crc(large,sizeof large);before=sends;
 for(size_t offset=0;offset<sizeof large;) {
     size_t count=sizeof large-offset;if(count>64)count=64;
     Feed(large+offset,count,2030);offset+=count;
 }
 assert(sends==before+1 && reply[1]==0x90 && reply_n==5);
 puts("PASS: fragmented/coalesced RTU, shared read/write, FC10, CRC, broadcast, unit changes, timeout, session reset, TX retry");
 return 0;
}
