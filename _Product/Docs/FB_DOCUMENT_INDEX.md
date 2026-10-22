# Function Block 文件索引

## 摘要

**Alarm／Logic 串接尚未完成：正式 PID callback、Register、實體輸出與 MCU 實測仍待接入。**

各 L04 FB 的正式規格與測試放在 PlatformCore 內，產品 Docs 以連結集中導覽，避免複製後出現不同版本。

| 範圍 | 文件 |
|---|---|
| 輸入參數分層、Value／Deviation 與輸入折線缺項 | [DTDM04_INPUT_LINEARIZATION_MAPPING.md](DTDM04_INPUT_LINEARIZATION_MAPPING.md) |
| 系統、取樣／控制週期、站號與串機管理 | [DTDM04_SYSTEM_MAPPING.md](DTDM04_SYSTEM_MAPPING.md) |
| PID 四組參數、SV 選組與 CKLIB 單位差異 | [DTDM04_PID_MAPPING.md](DTDM04_PID_MAPPING.md) |
| 跨模組溫差警報、Hold 與 SYSTEM 動作分派 | [DTDM04_MULTI_CHANNEL_ALARM_MAPPING.md](DTDM04_MULTI_CHANNEL_ALARM_MAPPING.md) |
| 前饋三階段、強制時間與 PID 補償 | [DTDM04_FEEDFORWARD_MAPPING.md](DTDM04_FEEDFORWARD_MAPPING.md) |
| 雙 Sensor 切換、中間區平均與故障品質 | [DTDM04_SWITCH_OVER_MAPPING.md](DTDM04_SWITCH_OVER_MAPPING.md) |
| Ramp／Soak 程序資料、END／STOP 與狀態缺項 | [DTDM04_RAMP_SOAK_MAPPING.md](DTDM04_RAMP_SOAK_MAPPING.md) |
| 串級 SV 協調、AT／校準與副控保護 | [DTDM04_CASCADE_MAPPING.md](DTDM04_CASCADE_MAPPING.md) |
| 輸出折線表欄位、缺項及建議契約 | [DTDM04_OUTPUT_LINEARIZATION_MAPPING.md](DTDM04_OUTPUT_LINEARIZATION_MAPPING.md) |
| 模型輸出欄位與規格疑點 | [DTDM04_MODEL_OUTPUT_MAPPING.md](DTDM04_MODEL_OUTPUT_MAPPING.md) |
| Control Loop 欄位、分層責任與單位疑點 | [DTDM04_CONTROL_LOOP_MAPPING.md](DTDM04_CONTROL_LOOP_MAPPING.md) |
| 全部 FB 盤點、DTDM04 功能缺口與待辦 | [FB_DTDM04_GAP_REVIEW.md](FB_DTDM04_GAP_REVIEW.md) |
| L05 Alarm／Logic 串接、設定 API 與 Host 驗證 | [L05_ALARM_LOGIC.md](L05_ALARM_LOGIC.md) |
| L05 排程與 CH0 MovingAverage | [L05_FB_SCHEDULE.md](L05_FB_SCHEDULE.md) |
| L04 Alarm 規格／方塊圖 | [FbAlarm README](../../_Modules/PlatformCore/L04_FunctionBlock/Alarm/FbAlarm/README.md) |
| L04 Alarm 測試細則 | [FbAlarm Host Test](../../_Modules/PlatformCore/L04_FunctionBlock/Alarm/FbAlarm/_UnitTest/TestResult/FbAlarm_HostTest.md) |
| L04 Logic 規格／方塊圖 | [FbLogic README](../../_Modules/PlatformCore/L04_FunctionBlock/Utility/FbLogic/README.md) |
| L04 Logic 手冊對照 | [FbLogic Manual Compliance](../../_Modules/PlatformCore/L04_FunctionBlock/Utility/FbLogic/MANUAL_COMPLIANCE.md) |
| L04 Logic 測試細則 | [FbLogic Host Test](../../_Modules/PlatformCore/L04_FunctionBlock/Utility/FbLogic/_UnitTest/TestResult/FbLogic_HostTest.md) |
