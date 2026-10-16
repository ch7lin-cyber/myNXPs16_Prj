# 尚未完成工作清單

更新日期：2026-10-16

主專案基準：`14cbccd`

PlatformCore 基準：`21c06e7`

本文件記錄目前尚未完成、已實作但仍待硬體驗證，以及明確保留給後續版本的工作。
主機單元測試通過不等同於目標板整合與產品驗證完成。

## P0：目前優先處理

### MCU 編譯與版本同步

- [ ] 本機切換至 `branch2_ver1` 並更新主專案。
- [ ] 執行 `git submodule sync --recursive`。
- [ ] 執行 `git submodule update --init --recursive`。
- [ ] 確認 PlatformCore 位於 `21c06e7` 或其後續相容版本。
- [ ] 在 MCUXpresso 執行 Clean Project 與 Build Project。
- [ ] 確認以下欄位不再出現編譯錯誤：
  - `EventTemperatureInputConfiguration_t.cjc_offset_centi_c`
  - `SerialConfiguration_t.response_delay_us`

### MCUXpresso Config Tools 輸出路徑（已知問題）

- [ ] Config Tools 仍將產生預覽中的 `pin_mux.h` 與 `peripherals.h` 指向
  `_Product/Tests/Unit/mocks/`；即使在 Configuration Preferences 啟用
  Output path overrides，2026-10-16 實測仍未生效。
- [ ] 尚未確認 Config Tools 實際比對的原始路徑格式，以及 output path override
  是否會正確保存進 `myNXPs16_Prj.mex`。
- [ ] 問題解決前，每次產生程式碼都必須先檢查 Update Files 預覽；不得允許
  Config Tools 覆寫 `_Product/Tests/Unit/mocks/pin_mux.h` 或
  `_Product/Tests/Unit/mocks/peripherals.h`。
- [ ] 若預覽仍指向 mocks，先取消產生；若已產生，將正式 Config Tools 輸出的
  `pin_mux.h`／`peripherals.h` 手動更新至 `board/`，並還原 mocks，再執行 diff、
  host unit test 與目標板 Clean/Rebuild。
- [ ] 長期修正需讓四個正式產出固定為 `board/pin_mux.c`、`board/pin_mux.h`、
  `board/peripherals.c`、`board/peripherals.h`，同時保持 mocks 為獨立的手寫測試替身。

### Pt100／Profile 2 校正

- [x] RTD Profile 2 改用量程 10%／90% 校正點。
- [x] Pt100 目標值定義為 `27858 uV`／`176643 uV`。
- [x] 校正擷取使用未套用舊校正的 ADC microvolts，重校不會疊加舊係數。
- [ ] 燒錄最新版 MCU 韌體。
- [ ] 重新執行 CH0／Profile 2 校正，覆蓋舊 FRAM 校正資料。
- [ ] 讀取 `0x4720～0x4723`，確認韌體提供的 Profile target。
- [ ] 讀取 `0x4C10～0x4C15`，確認新的 measured zero/span 與狀態。
- [ ] 執行 Pt100 十點測試，核對 PV、Raw Code、ADC microvolts 與 Input Error。
- [ ] 確認實測誤差後，決定 Pt100／Profile 2 是否可關閉問題。

### 其他 RTD Profile

- [ ] 驗證 Ni120 與 Pt100 共用 Profile 2 是否適用相同前端與校正槽。
- [ ] 完成 JPt100／Profile 5 校正及十點驗證。
- [ ] 完成 Pt1000／Profile 8 校正及十點驗證。
  - CALYS 1500 Pt1000 模擬輸出範圍為 `-220～740°C`。
  - 校正點使用 CALYS 範圍的 10%／90%，即 `-124°C`／`644°C`。
- [ ] 完成 Cu50／Profile 9 校正及十點驗證。
- [ ] 重新確認 CALYS RTD 三線端與 DUT L1／L2／L3 的端子對映。
- [ ] 確認 RTD 外部參考、REFOUT 偏壓及 `4300 ohm` 等效係數與實際硬體一致。

### 電流輸入

- [x] 0–20 mA、4–20 mA Sensor Type 與換算路徑已建立。
- [x] 電流校正流程採用量程 10%／90% 點。
- [ ] 確認 49.9 ohm shunt、PGA gain 與實際硬體版本一致。
- [ ] 確認 CALYS current supply ON/OFF 與實際回路接法。
- [ ] 完成 0–20 mA 校正及十點驗證。
- [ ] 完成 4–20 mA 校正及十點驗證。
- [ ] 驗證物理輸入超出規格與超出可量測範圍的錯誤分類。

## P1：類比輸入完整驗證

### 四通道驗證

- [ ] 將 CH0 已完成的測試流程重複於 CH1。
- [ ] 將 CH0 已完成的測試流程重複於 CH2。
- [ ] 將 CH0 已完成的測試流程重複於 CH3。
- [ ] 確認每個通道使用正確 AD7124、Chip Select、CV_SEL 與 CJC 來源。
- [ ] 確認四通道校正槽互不覆寫。

### 熱電偶與 CJC

- [x] TC Profile 使用量程 10%／90% 電氣校正點。
- [x] 每通道 CJC Offset Register、Pending／Apply 與 NVM 保存已實作。
- [ ] 使用最新版韌體重新驗證 K 型完整量程。
- [ ] 檢查 K 型 1300°C 端點是否被錯誤判定為超量程。
- [ ] 驗證 CH0／CH1 使用 CJC_0，CH2／CH3 使用 CJC_1。
- [ ] 建立可信的 TMP20 CJC Offset 校正方式。
- [ ] 在沒有標準溫度計的情況下，標註 CJC Offset 只能做系統比對，不能宣稱可追溯溫度校正。
- [ ] 驗證 B、C、D、E、J、K、N、R、S、T、L、U、TXK 的代表 Profile 與量程端點。

### 電壓與 mV

- [x] 電壓與 mV 校正改為量程 10%／90% 點。
- [x] 0–5 V、0–10 V、0–50 mV 已建立 Sensor Type 與 PV 換算。
- [ ] 保存正式十點測試結果與合格判定。
- [ ] 將電壓／mV 測試擴展到 CH1～CH3。

### 輸入 Filter

- [x] 已實作一次低通 Filter：`y += alpha * (x - y)`。
- [x] Filter Time Constant 範圍為 `0.0～60.0 s`，預設 `0.5 s`。
- [x] CH0 Register：
  - 未濾波 PV：`0x1000～0x1001`
  - Filter Time Constant：`0x1003～0x1004`
  - 濾波後 PV：`0x1005～0x1006`
- [ ] 使用階躍輸入驗證實際時間常數。
- [ ] 驗證 Filter 關閉、Sensor 切換及錯誤恢復時的初始化行為。
- [ ] 確認不同 AD7124 ODR／RTD Filter 設定下使用的 sample period 是否正確。

## P1：工廠校正工具

- [x] 校正選單依接線類別與 Profile 分組。
- [x] TC、RTD、電壓／mV、電流皆使用 10%／90% 校正點。
- [x] 重新進入 Profile 選擇時會再次顯示可用 Profile 清單。
- [x] Apply 確認字串使用 `APPLY_CHn_PROFILE_p`，降低誤操作。
- [x] 重複校正同一 Profile 已允許。
- [ ] 驗證每個 Profile 的 CALYS mode、輸出點、端子與 DUT Sensor code。
- [ ] 確認共用 Profile 的 Sensor 使用相同 ADC 前端及實體接線。
- [ ] 依 Profile 建立允許 Apply／僅允許 Dry-run 的明確清單。
- [ ] Apply 完成後自動執行低／中／高點驗證。
- [ ] 將十點測試結果加入自動 PASS／FAIL 公差判定。
- [ ] 完成四通道、所有 Profile 的工廠校正報告格式。

## P1：硬體功能驗證

### FRAM／NVM

- [x] FRAM Driver、NVM Service、CRC、A/B bank 與校正槽已實作。
- [x] FRAM checkerboard bank test 已實作。
- [ ] 在正式校正前執行目標板 checkerboard bank test。
- [ ] 驗證寫入、重開機、掉電及 CRC 損壞回復。
- [ ] 驗證 NVM 寫入失敗能正確進入 FaultService。
- [ ] 校正資料存在後不得再執行破壞性全晶片 FRAM 測試。

### DAC／PWM／DI／DO

- [ ] 完成四通道 DAC：Modbus Write → Apply → DAC8562 硬體輸出驗證。
- [ ] 驗證 DAC 輸出失敗及 rollback 狀態。
- [ ] 完成四通道 PWM：Modbus Write → Apply → 實體波形驗證。
- [ ] 驗證 PWM immediate／next-cycle 更新模式。
- [ ] 驗證四路 DI bit-packed Register 與實體輸入。
- [ ] 驗證四路 DO bit-packed Register 與實體輸出。
- [ ] 驗證 Safety inhibit 時 PWM／DO 的安全狀態。

### 板上輸入與 Safety

- [ ] 驗證 DIP Switch bit-packed Register。
- [ ] 驗證 SW2 Rotary Switch Register。
- [ ] 驗證 Low Voltage Detect 門檻、去抖、恢復及輸出禁止。
- [ ] 驗證 MCU 溫度超過 85°C 且連續三次成立的保護流程。
- [ ] 執行 Fault／Event／Warning／Snapshot 故障注入測試。
- [ ] 執行長時間運行與錯誤恢復測試。

## P2：通訊完整驗證

- [ ] 完成 FC0 Modbus RTU 的 FC03／FC04／FC06／FC10 測試矩陣。
- [ ] 完成 FC0 Modbus ASCII 的 FC03／FC04／FC06／FC10 測試矩陣。
- [ ] 驗證 4800～921600 baud 支援範圍。
- [ ] 驗證 7／8 data bits、N／E／O parity、1／2 stop bits 的合法組合。
- [ ] 驗證 FC0 Response Delay `0x120B` 能解決上位機 RS-485 轉向過慢問題。
- [ ] 驗證 RS-485 DE／RE 在 TX complete 後切回 RX。
- [ ] 驗證 CRC、timeout、frame error、overflow 與錯誤後恢復。
- [ ] 完成 FC1 Modbus Master polling、timeout、retry 與離線恢復測試。
- [ ] FC1 Response Delay 目前只保存設定；待 FC1 支援 Slave 角色時才啟用。

### USB CDC

- [ ] 使用 FRO USB clock 執行目標板 Clean/Rebuild。
- [ ] 驗證冷開機 USB COM Port 列舉。
- [ ] 驗證直接接主機 USB Port 時的拔插與重新列舉。
- [ ] 驗證經 USB Hub 時的 Hub 重新斷電／列舉行為並記錄限制。
- [ ] 驗證 USB CDC Modbus 通訊。
- [ ] MCUXpresso Config Tools 重生設定後，確認 CDC class 設定未被覆蓋。

## P2：L04 Function Block 與 L05 Application

### L04 尚未完成的預留 FB

- [ ] FbSetpointRamp
- [ ] FbProfileSequencer
- [ ] FbInBandTimer
- [ ] FbPid
- [ ] FbOutputLimiter
- [ ] FbWeightedSum
- [ ] FbDivide
- [ ] FbSelector
- [ ] FbMovingAverage
- [ ] FbLinearization
- [ ] FbLeadLag
- [ ] FbDeadTime
- [ ] FbPowerVoltageCompensation
- [ ] FbHeaterResistance
- [ ] FbBatchMetrics
- [ ] FbProcessResponse
- [ ] FbRecordTrigger
- [ ] FbCascadeCoordinator

以上項目目前是明確標示的空架構，不可視為已實作或已驗證。

### L05 串接

- [x] 固定記憶體週期排程器及主機單元測試已完成。
- [ ] 建立正式產品 task table。
- [ ] 建立 L05 callback adapter。
- [ ] 實作新樣本、資料有效性及重複樣本判斷。
- [ ] 串接 L03 Input → L04 FB → Product Output。
- [ ] 串接 ProductApplication 初始化及主迴圈。
- [ ] 量測 MCU 執行時間、最壞延遲與排程抖動。
- [ ] 驗證資料過期、超長 dt 與安全輸出處置。

## P3：後續版本預留

- [ ] CJC source 2／3 外部來源。
  - 目前只能保存與讀回設定。
  - 尚未提供外部 CJC 溫度 Register。
  - 尚未實作由其他通道提供 CJC。
- [ ] FC1 Slave 角色及其 Response Delay 實際套用。
- [ ] IAP／線上韌體更新與 APP 專案整合。
- [ ] Bootloader A/B bank、映像驗證、rollback 與產品相容版本管理。

## 文件與簡報

- [ ] 第一份：全部系統架構與 System Service／System Routine。
- [ ] 第二份：類比輸入、UART、NVM 流程與時序。
- [ ] 第三份：Register 規劃、L04 FB、L05 APP、Driver／HAL。
- [ ] 第四份：工廠校正流程與 K 型 PV 三位小數驗證。
- [ ] 第五份：維護手冊。
- [ ] 維護手冊加入由大到小的系統方塊堆疊章節。
- [ ] 先完成 Markdown 內容審查，再產生不同風格的投影片。

## 建議執行順序

1. 更新 submodule 並確認 MCU Clean Build 通過。
2. 燒錄最新版韌體，重新完成 CH0／Pt100／Profile 2 校正。
3. 完成 Pt100 十點驗證並關閉 RTD 主要問題。
4. 完成 0–20 mA、4–20 mA 校正與十點驗證。
5. 將驗證流程擴展到 CH1～CH3。
6. 完成通訊、FRAM、DAC、PWM、DI、DO 的硬體驗收。
7. 串接 L04／L05 應用功能。
8. 完成五份簡報與維護文件。
