# Function Block 文件索引

## 摘要

2026-10-27：完成狀態統一區分「L04 實作／封裝、Host 單元測試／mock 測試、產品串接、MCU 實測」。[狀態定義與各 FB 總表](../../_Modules/PlatformCore/L04_FunctionBlock/VERIFICATION_STATUS.md)；不再將 Host 驗證視為獨立於單元測試的第三項完成工作。


**Alarm／Logic 串接尚未完成：正式 PID callback、Register、實體輸出與 MCU 實測仍待接入。**

各 L04 FB 的正式規格與測試放在 PlatformCore 內，產品 Docs 以連結集中導覽，避免複製後出現不同版本。

2026-10-26：各已實作 FB 已統一 SVG FBD 格式，共 25 個模組、26 張公開入口方塊圖（含 PID；輸入／輸出折線各一張）。[全部 FBD、引腳與參數文件索引](../../_Modules/PlatformCore/L04_FunctionBlock/FBD_INDEX.md)。本次為文件格式更新，未改變引腳或禁能契約；C7G Logic 仍為尚未實作的規劃。

| 範圍 | 文件 |
|---|---|
| PID目前0.1°C相容轉換與Colin下一版0.001°C待辦 | [PID單位邊界](../../_Modules/PlatformCore/L04_FunctionBlock/Control/FbPid/README.md) |
| L05 輸入折線→Switch Over→PV候選與整合測試 | [L05_PV_PROCESSING.md](L05_PV_PROCESSING.md) |
| 電壓平方補償、旁路與MV限制 | [FbPowerVoltageCompensation 規格](../../_Modules/PlatformCore/L04_FunctionBlock/Control/FbPowerVoltageCompensation/README.md) |
| 電壓補償 Host 測試細則 | [FbPowerVoltageCompensation Host Test](../../_Modules/PlatformCore/L04_FunctionBlock/Control/FbPowerVoltageCompensation/HOST_TEST.md) |
| 加熱器阻值、RMS品質與基準偏移診斷 | [FbHeaterResistance 規格](../../_Modules/PlatformCore/L04_FunctionBlock/Diagnostics/FbHeaterResistance/README.md) |
| 加熱器阻值 Host 測試細則 | [FbHeaterResistance Host Test](../../_Modules/PlatformCore/L04_FunctionBlock/Diagnostics/FbHeaterResistance/HOST_TEST.md) |
| 週期／事件記錄觸發、延遲與單次武裝 | [FbRecordTrigger 規格](../../_Modules/PlatformCore/L04_FunctionBlock/Recording/FbRecordTrigger/README.md) |
| 記錄觸發 Host 測試細則 | [FbRecordTrigger Host Test](../../_Modules/PlatformCore/L04_FunctionBlock/Recording/FbRecordTrigger/HOST_TEST.md) |
| 已知步階響應觀測、穩定增益與時刻指標 | [FbProcessResponse 規格](../../_Modules/PlatformCore/L04_FunctionBlock/Diagnostics/FbProcessResponse/README.md) |
| 製程響應 Host 測試細則 | [FbProcessResponse Host Test](../../_Modules/PlatformCore/L04_FunctionBlock/Diagnostics/FbProcessResponse/HOST_TEST.md) |
| 批次統計、過衝／到溫與偏差積分 | [FbBatchMetrics 規格](../../_Modules/PlatformCore/L04_FunctionBlock/Diagnostics/FbBatchMetrics/README.md) |
| 批次統計 Host 測試細則 | [FbBatchMetrics Host Test](../../_Modules/PlatformCore/L04_FunctionBlock/Diagnostics/FbBatchMetrics/HOST_TEST.md) |
| 事件前饋 L04 曲線、Force 與取消契約 | [FbFeedforward 規格](../../_Modules/PlatformCore/L04_FunctionBlock/Control/FbFeedforward/README.md) |
| 事件前饋 Host 測試細則 | [FbFeedforward Host Test](../../_Modules/PlatformCore/L04_FunctionBlock/Control/FbFeedforward/HOST_TEST.md) |
| 串級 MV→SV 換算核心、濾波與限制 | [FbCascadeCoordinator 規格](../../_Modules/PlatformCore/L04_FunctionBlock/Control/FbCascadeCoordinator/README.md) |
| 串級換算 Host 測試細則 | [FbCascadeCoordinator Host Test](../../_Modules/PlatformCore/L04_FunctionBlock/Control/FbCascadeCoordinator/HOST_TEST.md) |
| Ramp／Soak 程序核心、配方與命令引腳 | [FbProfileSequencer 規格](../../_Modules/PlatformCore/L04_FunctionBlock/Control/FbProfileSequencer/README.md) |
| Ramp／Soak Host 測試細則 | [FbProfileSequencer Host Test](../../_Modules/PlatformCore/L04_FunctionBlock/Control/FbProfileSequencer/HOST_TEST.md) |
| PID 四組候選參數、Auto／鎖組及整組驗證 | [FbPidGroupSelector 規格](../../_Modules/PlatformCore/L04_FunctionBlock/Control/FbPidGroupSelector/README.md) |
| PID 選組 Host 測試細則 | [FbPidGroupSelector Host Test](../../_Modules/PlatformCore/L04_FunctionBlock/Control/FbPidGroupSelector/HOST_TEST.md) |
| 跨通道溫差警報 L04 引腳、Hold／Reset 與動作脈衝 | [FbMultiChannelAlarm 規格](../../_Modules/PlatformCore/L04_FunctionBlock/Alarm/FbMultiChannelAlarm/README.md) |
| 跨通道警報 Host 測試細則 | [FbMultiChannelAlarm Host Test](../../_Modules/PlatformCore/L04_FunctionBlock/Alarm/FbMultiChannelAlarm/HOST_TEST.md) |
| 雙輸入切換 L04 引腳、故障與恢復契約 | [FbInputSwitchOver 規格](../../_Modules/PlatformCore/L04_FunctionBlock/Utility/FbInputSwitchOver/README.md) |
| 雙輸入切換 Host 測試細則 | [FbInputSwitchOver Host Test](../../_Modules/PlatformCore/L04_FunctionBlock/Utility/FbInputSwitchOver/HOST_TEST.md) |
| 輸入／輸出折線 L04 模式包裝、引腳與參數 | [FbSignalLinearization 規格](../../_Modules/PlatformCore/L04_FunctionBlock/Utility/FbSignalLinearization/README.md) |
| 輸入／輸出折線測試摘要與細則 | [FbSignalLinearization Host Test](../../_Modules/PlatformCore/L04_FunctionBlock/Utility/FbSignalLinearization/HOST_TEST.md) |
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
