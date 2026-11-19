# 產品文件索引

依文件主要用途分為功能類與測試類。兼含設計與測試的文件保留完整內容，以主要用途歸類。

| 分類 | 內容 | 數量 |
|---|---|---|
| [功能類](功能類/) | 架構、功能契約、設定、操作與整合設計 | 34 份 |
| [測試類](測試類/) | 測試操作、注入方式、驗證步驟與判定紀錄 | 6 份 |
| [尚未完成工作](OutstandingWork.md) | 跨功能／測試的待辦與待驗證事項 | 1 份 |

## 功能類

- [各層功能架構、主要函式與執行時機](功能類/LAYER_ARCHITECTURE_AND_EXECUTION_TIMING.md)
- [TC 電壓、溫度 Snapshot 與 L05 MovingAverage 資料流](功能類/TC_TO_SNAPSHOT_MOVING_AVERAGE_FLOW.md)
- [APP 線上更新與 RS485 入口](功能類/APP_ONLINE_UPDATE.md)
- [ADC 除錯資訊編譯開關](功能類/AdcDebugConfiguration.md)
- [Internal CAN first implementation](功能類/CAN_INTERNAL_MVP.md)
- [CALYS 1500 自動校正設計](功能類/Calys1500AutoCalibrationDesign.md)
- [CJC 補償來源選擇](功能類/CjcSourceSelection.md)
- [DTDM04 Cascade Control Loop Parameters 對照](功能類/DTDM04_CASCADE_MAPPING.md)
- [DTDM04 Control Loop Parameters 對照](功能類/DTDM04_CONTROL_LOOP_MAPPING.md)
- [DTDM04 Feedforward 參數與架構對照](功能類/DTDM04_FEEDFORWARD_MAPPING.md)
- [DTDM04 輸入參數與輸入折線表對照（暫存規劃）](功能類/DTDM04_INPUT_LINEARIZATION_MAPPING.md)
- [DTDM04 Model Output Parameters 對照](功能類/DTDM04_MODEL_OUTPUT_MAPPING.md)
- [DTDM04 跨模組多通道警報對照](功能類/DTDM04_MULTI_CHANNEL_ALARM_MAPPING.md)
- [DTDM04 輸出折線表對照](功能類/DTDM04_OUTPUT_LINEARIZATION_MAPPING.md)
- [DTDM04 PID 參數與既有架構對照（暫存規劃）](功能類/DTDM04_PID_MAPPING.md)
- [DTDM04 Ramp Soak Parameters 對照](功能類/DTDM04_RAMP_SOAK_MAPPING.md)
- [DTDM04 Input Switch Over Parameters 對照](功能類/DTDM04_SWITCH_OVER_MAPPING.md)
- [DTDM04 2000h 系統／串機參數對照（暫存規劃）](功能類/DTDM04_SYSTEM_MAPPING.md)
- [Function Block 文件索引](功能類/FB_DOCUMENT_INDEX.md)
- [Function Block 與 DTDM04 功能盤點](功能類/FB_DTDM04_GAP_REVIEW.md)
- [APP／OTA 暫存區擴大為 104 KiB](功能類/FLASH_LAYOUT_104K.md)
- [APP 更新入口公用庫整合與重測](功能類/FW_UPDATE_APP_LIBRARY.md)
- [APP 更新配套：DirectUpdate](功能類/FW_UPDATE_LAYOUT.md)
- [工廠校正資料槽與 Sensor type 對映](功能類/FactoryCalibrationSlots.md)
- [L05 Alarm／Logic 串接](功能類/L05_ALARM_LOGIC.md)
- [L05 各 FB 串接規劃（暫存）](功能類/L05_FB_CONNECTION_PLAN_DRAFT.md)
- [L05 產品 FB 排程預留](功能類/L05_FB_SCHEDULE.md)
- [PID Controller L04/L05 Integration](功能類/L05_PID_CONTROLLER_ADAPTER.md)
- [L05 溫度PV處理鏈](功能類/L05_PV_PROCESSING.md)
- [Register Map 至 UART Modbus 呼叫流程](功能類/RegisterMapToUartFlow.md)
- [RTD 外部參考與 REFOUT 板級偏壓](功能類/RtdReferenceBias.md)
- [5 點中位數濾波](功能類/SensorMedianFilter.md)
- [USB0 CDC 虛擬 COM / 共用 Modbus Register Map](功能類/USB_CDC_MODBUS.md)
- [產品板 USB FS 時鐘（無外部晶振）](功能類/UsbClockWithoutCrystal.md)

## 測試類

- [CAN0 Transport 回歸與 MCU 驗證](測試類/CAN_TRANSPORT_VALIDATION.md)
- [CALYS 1500 與 DUT 十點迴路測試](測試類/Calys1500LoopTest.md)
- [CH0 MovingAverage MCU 驗證操作](測試類/L05_CH0_MCU_VALIDATION.md)
- [CH0 PID 固定 MV 串接測試](測試類/L05_CH0_PID_TEST.md)
- [Python 系統安全上板測試](測試類/SAFETY_PYTHON_TEST.md)
- [System Safety Verification](測試類/SystemSafetyVerification.md)

## 閱讀原則

- 功能類文件中的測試摘要不代表 MCU 或實體輸出已通過，仍依原文的證據與限制判讀。
- 測試類文件可能包含測試專用組態與注入入口，正式建置仍遵循各文件的編譯開關限制。
- DTDM04 對照與串接草案依原文維持參考／待確認狀態。
- L04 共用庫的規格與測試文件保留在 PlatformCore 子模組原位置，可由[功能塊索引](功能類/FB_DOCUMENT_INDEX.md)查閱。
