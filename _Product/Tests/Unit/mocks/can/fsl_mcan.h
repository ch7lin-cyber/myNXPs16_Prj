#ifndef CAN_SDK_MOCK_H
#define CAN_SDK_MOCK_H
/* SDK boundary mock: no driver internals. ReadRxFifo returns borrowed RAM,
 * ClearStatusFlag implements W1C, pending/completion are independently driven. */
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <assert.h>
typedef struct { uint32_t TXBTIE,GFC,RXF0S,IR,PSR,ECR; } CAN_Type;
extern CAN_Type mock_can;
#define CAN0 (&mock_can)
#define CAN_RXF0S_F0FL_MASK 0x7fU
#define CAN_PSR_LEC_SHIFT 0U
#define CAN_PSR_LEC_MASK 7U
#define CAN_PSR_EW_MASK (1U<<6)
#define CAN_PSR_EP_MASK (1U<<5)
#define CAN_PSR_BO_MASK (1U<<7)
#define CAN_ECR_TEC_MASK 255U
#define CAN_ECR_REC_SHIFT 8U
#define CAN_ECR_REC_MASK (127U<<8)
#define CAN_IR_BO_MASK (1U<<25)
#define CAN_IR_EW_MASK (1U<<24)
#define CAN_IR_EP_MASK (1U<<23)
#define CAN_IR_PEA_MASK (1U<<27)
#define CAN_IR_PED_MASK (1U<<28)
#define CAN_IR_RF0L_MASK (1U<<3)
#define CAN_GFC_ANFE_MASK (3U<<2)
#define CAN_GFC_RRFE_MASK 1U
#define CAN_GFC_ANFE(x) ((x)<<2)
#define CAN_GFC_RRFE(x) (x)
enum { kStatus_Success=0,kStatus_Fail=1,kMCAN_FrameIDStandard=0,
 kMCAN_FrameTypeData=0,kMCAN_rejectFrame=1,kMCAN_reject0=2,
 kMCAN_FifoBlocking=0,kMCAN_8ByteDatafield=0,kMCAN_txFifo=0,
 kMCAN_storeinFifo0=1,kMCAN_range=0,kMCAN_Fifo0=0,
 kMCAN_TxTransmitCompleteFlag=1U<<9 };
typedef struct { uint32_t id,xtd,rtr,fdf,brs,dlc; uint8_t *data; uint8_t size; } mcan_rx_buffer_frame_t;
typedef mcan_rx_buffer_frame_t mcan_tx_buffer_frame_t;
typedef struct { uint32_t address,idFormat,listSize,remFrame,nmFrame; } mcan_frame_filter_config_t;
typedef struct { uint32_t address,elementSize,watermark,opmode,datafieldSize; } mcan_rx_fifo_config_t;
typedef struct { uint32_t address,dedicatedSize,fqSize,mode,datafieldSize; } mcan_tx_buffer_config_t;
typedef struct { uint32_t sfec,sft,sfid1,sfid2; } mcan_std_filter_element_config_t;
typedef struct { uintptr_t baseAddr; mcan_frame_filter_config_t *stdFilterCfg; mcan_rx_fifo_config_t *rxFifo0Cfg; mcan_tx_buffer_config_t *txBufferCfg; } mcan_memory_config_t;
typedef struct { uint32_t baudRateA,timingConfig; bool enableCanfdNormal,enableCanfdSwitch,enableLoopBackInt,enableLoopBackExt,enableBusMon; } mcan_config_t;
extern uint8_t mock_message_ram[4096];
extern bool mock_timing_ok,mock_ram_ok,mock_write_ok,mock_read_ok,mock_pending;
extern unsigned mock_writes,mock_requests,mock_deinits;
extern mcan_config_t mock_config;
extern mcan_tx_buffer_frame_t mock_tx;
extern uint8_t mock_tx_data[8];
void Mock_Reset(void);
void Mock_Push(uint16_t id,uint8_t dlc,uint8_t flags,uint8_t value);
void Mock_Complete(void);
void Mock_PushData(uint16_t id,const uint8_t *payload);
void MCAN_GetDefaultConfig(mcan_config_t *c);
bool MCAN_CalculateImprovedTimingValues(uint32_t b,uint32_t h,uint32_t *t);
void MCAN_Init(CAN_Type *b,const mcan_config_t *c,uint32_t h);
void MCAN_Deinit(CAN_Type *b);
int MCAN_SetMessageRamConfig(CAN_Type *b,const mcan_memory_config_t *m);
void MCAN_EnterInitialMode(CAN_Type *b);
void MCAN_EnterNormalMode(CAN_Type *b);
void MCAN_SetSTDFilterElement(CAN_Type *b,const mcan_frame_filter_config_t *c,const mcan_std_filter_element_config_t *e,uint8_t i);
void MCAN_ClearStatusFlag(CAN_Type *b,uint32_t f);
uint32_t MCAN_GetStatusFlag(CAN_Type *b,uint32_t f);
uint32_t MCAN_IsTransmitRequestPending(CAN_Type *b,uint8_t i);
int MCAN_WriteTxBuffer(CAN_Type *b,uint8_t i,const mcan_tx_buffer_frame_t *f);
void MCAN_TransmitAddRequest(CAN_Type *b,uint8_t i);
int MCAN_ReadRxFifo(CAN_Type *b,uint8_t i,mcan_rx_buffer_frame_t *f);
#endif
