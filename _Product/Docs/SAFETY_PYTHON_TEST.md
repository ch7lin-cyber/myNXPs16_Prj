# Python 系統安全上板測試

2026-10-24：改用 FC03／06／10 測試工具，MCU 正常運行並餵 Watchdog，不需 debugger call。已提供 MCU 過溫及低電壓注入循環，兩項 EVK 軟體流程實測均通過；其他故障及實體輸出量測尚未加入自動循環。

## 首次準備

1. 更新 `branch2_ver1`，執行 `git submodule sync --recursive`、`git submodule update --init --recursive`。本版本固定 PlatformCore `843cc36`，將 TC-B／C／D 執行期表格移至 SRAMX，避免 SafetyTest 主 SRAM 溢位。
2. SafetyTest 組態的 C compiler 同時定義 `PRODUCT_SAFETY_TEST_BUILD=1`、`PRODUCT_SAFETY_TEST_TARGET=1`。一般 Debug／Release 不定義。已有的測試函式 `--undefined` 連結選項可移除，現在正常程式流程會引用該函式。
3. Clean／Build SafetyTest，透過更新工具下載同次產生的 SafetyTest BIN，並啟動 APP。Bootloader 需為 00.01.11 或後續相容版本，APP 保持 104 KiB 分區。
4. 電腦執行一次 `python -m pip install pyserial`。關閉其他占用 DUT COM 埠的工具，結束 SWD 除錯連線。
5. 測試會真正觸發 Safety inhibit，請使用允許安全停機的測試設備。先確認沒有其他 Fault／Safety inhibit；程式不會為了開始測試而清除既有 Fault。

## 直接執行

### EVK 專用組態（沒有產品周邊時）

複製 SafetyTest 組態為 `SafetyTestEVK`，在該組態的 **C Compiler → Preprocessor → Defined symbols** 加入以下三個定義，並套用至整個專案：

```text
PRODUCT_SAFETY_TEST_BUILD=1
PRODUCT_SAFETY_TEST_TARGET=1
PRODUCT_SAFETY_TEST_EVK=1
```

EVK 開關位於 `_Product/Config/ProductSafetyTestConfig.h`，預設為 0。缺少任一 SafetyTest 定義，或其值不為 1，EVK 建置會報錯。一般 Debug／Release 不加入上述定義；正式板 SafetyTest 只保留前兩個定義。

EVK 模式跳過 DAC／FRAM 初始化、NVM 讀取及持久化、外部 AD7124 初始化／取樣／復原／故障判斷、外部感測器配置及 L05 FB 排程、CJC 取樣與 DIP／旋鈕讀取。低電壓未注入時讀入正常狀態，不讀取 PIO1_31；注入時以測試命令指定的原始狀態取代輸入。溫度輸入使用 RAM 預設配置，配置事件僅要求 Alarm／Safety 消費者確認，不假裝 NVM 已儲存。保留 MCU 內溫取樣、Watchdog、通訊、正式過溫／Fault／Event／Snapshot／Safety 與 PWM 抑制流程。

Clean／Build **SafetyTestEVK**，下載此組態輸出的 BIN，重新啟動 APP 後執行下列同一支 Python；不需手動清除前一個韌體執行時產生的四筆 Fault。不要混用 SafetyTest／Debug 資料夾中的舊 BIN。

此模式通過只能記為「EVK MCU 過溫軟體流程通過」。外部低電壓偵測、FRAM、AD7124、PV Alarm、L05 FB 及實際 PWM／DO 電位仍須正式板實測；不在 EVK 此次驗證範圍內。若仍有 Fault，腳本照常停止，不會略過故障繼續測試。

### 執行命令

```powershell
python E:\myGitHub\myNXPs16_Prj\_Product\Tools\safety_modbus_test.py
```

預設 RTU、站號 2、115200、8N1。只有一個 COM 埠時自動選用，有多個則選擇 DUT 編號。對不支援測試區的正式韌體，第一筆唯讀探測便停止，不進行注入。COM 埠存在不代表是 DUT；腳本不向其他 COM 埠輪流發送測試命令。

需要指定連線時：

```powershell
python _Product\Tools\safety_modbus_test.py --port COM8 --baud 115200 --station 2
```

可使用 USB CDC Modbus 或 RS485 Modbus RTU；USB 的 baud 欄位不代表 USB 實際傳輸速率。RS485 使用 APP 當前設定。此工具目前不提供 ASCII。

### EVK 進入 Updater 的修正

2026-10-24：使用者回報更新工具接受進入 Updater 請求後，COM8 一直重連為 APP。檢查發現 EVK 啟動刻意略過 DAC 初始化，但 `ProductFwUpdate_Quiesce()` 仍要求四路 `BspAnalogOutput_WriteCode(0)` 成功；DAC 未註冊使安全停機回傳 false，共用更新狀態機遂停在 FAILED，不執行 mailbox／reset。進入命令的 ACK 僅代表接受請求，並不代表模式已切換。尚未取得該次裝置的 `0x7051` 狀態讀值，此原因依程式路徑確認。

已修正為 EVK 仍停止四個 PWM timer 並將八路 GPIO 拉低，僅略過不存在的 DAC 歸零；正式模式仍要求 DAC 成功，失敗時禁止 reset。Host 已以真實 board quiesce 函式及 register／DAC mocks 驗證兩種模式，並通過共用更新 ACK／延遲／失敗保護測試；實機更新切換待確認。

如果目前裝置已裝入有此問題的 EVK APP，新版程式尚未下載前仍無法藉該 APP 進入 Updater。可透過 SWD 載入修正版 APP 並直接執行，再使用更新工具正式下載 BIN，讓 Bootloader 的映像 metadata／CRC 與 APP 同步；只透過 SWD 改寫 APP 後直接斷電重啟，可能因舊 metadata 而無法正常自動啟動。SWD 操作須保留 Bootloader 分區。

### 下一項：低電壓注入循環

更新後 Clean／Build 並下載最新 **SafetyTestEVK BIN**，保留原有三個巨集，不需新增設定。啟動 APP 後執行：

```powershell
python E:\myGitHub\myNXPs16_Prj\_Product\Tools\safety_modbus_test.py --port COM3 low-voltage-cycle
```

未指定子命令仍為過溫循環，低電壓測試需指定 `low-voltage-cycle`。此命令依序確認正常取樣、1 次與 2 次異常短脈衝均不成立、持續異常第 3 次成立、Fault `0x0202`／detail 1、Safety low-voltage bit（值 2）live／latch／inhibit、Event／Snapshot、新增故障期間拒絕 Reset、連續正常 100 次後 live 解除但 latch 保持，以及 Reset 後回復正常。過程不修改正式 3／100 次門檻或排程。

MCU 按正常低電壓服務取樣呼叫消耗有限次數的注入，不依靠 Python 串列通訊在 1 ms 內切換輸入。有限異常樣本耗盡後維持「注入正常」直到下一命令，保留該段實際異常樣本數、最大成立／恢復計數及曾經成立旗標。這驗證的是連續取樣判定，並非實體電路反應時間。一般 SafetyTest 關閉注入後恢復 GPIO 來源，EVK 關閉注入後恢復正常常數。

正常結束、失敗或 Ctrl+C 都嘗試關閉低電壓注入；清理失敗使最終結果為 FAIL，不會自動清除失敗時的 Fault。舊版／正式版韌體不支援低電壓測試區時，唯讀探測即停止，不送注入命令。低電壓偵測電路及實際 PWM／DO 仍須正式板量測。

## 自動循環與判定


### 正式 AXF 具名測試符號檢查（本次 Debug／Release AXF 均已通過）

沿用剛完成入口封鎖測試時的一般 Debug／Release 建置，**不需重新燒錄、不需連線 COM**，也不需要 pyserial 或額外安裝 nm。更新 Python 後執行：

```powershell
python E:\myGitHub\myNXPs16_Prj\_Product\Tools\safety_modbus_test.py production-symbol-check --axf "E:\myGitHub\myNXPs16_Prj\Debug\myNXPs16_Prj.axf"
```

若當次使用 Release，將路徑中的 `Debug` 改為 `Release`；請選擇與下載 BIN 同一次建置的 AXF，不使用 SafetyTestEVK 的 AXF。本命令不呼叫任何 Modbus 操作，不要求切換或重啟 MCU。

腳本以標準 Python 讀取 ELF32 little-endian ARM executable 的完整 SHT_SYMTAB，檢查入口位於現行 APP `0x8000..0x21FFF`、包含 ResetISR／Reset_Handler，並掃描全域及區域的已定義符號。拒絕名稱包含 `ProductSafetyTest_` 或 `ProductInternalAdcDriver_SetMcuTemperatureTestOverride` 的符號，也涵蓋 `.constprop`／`.isra` 等保留原名的 LTO 後綴；未定義項目及來源檔名不列入本次已定義符號掃描。

紀錄包含 AXF 絕對路徑、檔案大小、SHA-256、入口、已定義符號數與命中的禁止符號；沒有命中時回報 `production_symbol_check_pass` 及最終 PASS。檔案不存在、BIN／MAP、非 ARM、Bootloader、檔案截斷／格式錯誤、缺少符號表或 Reset 符號均 FAIL，不能把空掃描當通過。JSONL 與其他測試一樣放在目前工作目錄的 `safety_test_results`。

**判定範圍：只確認指定具名測試符號未出現。** LTO 可能內嵌或重新命名函式，測試 RAM 成員與匿名狀態不一定有獨立符號。因此仍須確認整個一般組態沒有定義三個測試／EVK 巨集，並搭配同次映像的 `production-guard-cycle`；本檢查不取代建置設定審核，也不能單獨證明測試程式或 RAM 狀態完全移除。SHA-256 識別所檢查 AXF，無法單憑此值證明 MCU 已下載同次 BIN。

新增後共 **64 項 Python 離線回歸通過**：使用合成 ARM ELF 測試成功、區域／全域及 LTO 後綴測試符號、stripped／錯誤架構／Bootloader、損壞 symbol table／名稱、離線 CLI JSONL 與檔案不存在。本次 Debug 路徑 AXF 的使用者實測結果如下；未修改 MCU 程式。


### 一般 Debug／Release 測試入口封鎖（本次映像／COM3 已通過）

此案例需要切換韌體，不能沿用目前的 SafetyTestEVK BIN：

1. 更新 `branch2_ver1`，選擇一般 **Debug** 或 **Release** 組態，確認整個專案沒有定義 `PRODUCT_SAFETY_TEST_BUILD`、`PRODUCT_SAFETY_TEST_TARGET`、`PRODUCT_SAFETY_TEST_EVK`。設定檔預設 EVK=0，勿為了在 EVK 執行而加入 EVK=1，否則會因缺少測試巨集而拒絕編譯。
2. Clean／Build 該組態，確認 104 KiB APP 容量限制通過；使用既有更新工具下載**同次一般 Debug／Release 組態產生的 BIN**，啟動 APP，勿下載 SafetyTestEVK 舊 BIN。更新過程保留 Bootloader，避免只用 SWD 改寫 APP 造成 metadata／CRC 不一致。
3. 關閉更新工具的 COM 連線，使用目前 APP 的 COM／站號／通訊設定執行：

```powershell
python E:\myGitHub\myNXPs16_Prj\_Product\Tools\safety_modbus_test.py --port COM3 production-guard-cycle
```

先讀正式 Safety／Fault count 作為通訊及狀態基準，再逐一 FC03 讀取 `0x4960..0x499F` 的 64 個位址，每筆必須回覆 Illegal Data Address（exception=2）。任何位址能讀取即失敗並停止，尚不寫入測試區；下載 SafetyTest／SafetyTestEVK 時應在此失敗，不能視為正式版通過。

64 筆唯讀檢查通過後，送出 4 筆 FC06（溫度／低電壓的 enable、key，值全為 0）及 5 筆 FC10（兩項完整停用命令、實際 ADC／低電壓／PWM 診斷區零值寫入）。兩項停用命令使用正確 key=0x5AFE，驗證位址被封鎖，而不是僅靠錯誤確認碼拒絕。全部必須 exception=2，共 **73 項拒絕**，最後回報 `production_guard_cycle_pass`、`rejected_cases=73` 與 PASS。

本案例不啟用任何注入、不寫產品設定／輸出、不清除 Fault，不需注入清理。既有 Fault 不會為此自動重置；一般韌體在 EVK 缺少周邊可能已有 Fault，只要前後 Fault count、Safety live／latch／state／inhibit、Event／Snapshot 計數及序號一致，仍可驗證測試 Register 封鎖。若期間有自然新增故障／事件，回報 FAIL，先查明狀態變動再重測。無回應、CRC 錯誤或其他 exception 都不能算正確拒絕。

結果僅證明該次韌體、通訊路徑上的測試 Register 不可存取，不等於確認 AXF 中所有測試符號均不存在。Debug／Release 與另一傳輸路徑須分別記錄；之後執行原有注入循環需重新下載 SafetyTestEVK BIN。

新增後共 **57 項 Python 離線回歸通過**，包含 73 項拒絕、既有 Fault 保留、誤載測試版時不寫測試區、寫入被接受立即失敗、錯誤 exception／timeout 不算通過，以及狀態變化偵測。Host 真實 Modbus adapter 的 DEBUG=1／0 均驗證整區 64 位址 FC03／06／10 拒絕，其他校正與 Safety PWM 回歸亦通過。本次只修改 Python、Host 測試與文件，沒有修改 MCU 執行程式；本次映像／COM3 上板結果如下；正式組態建置報表及其他組態／介面結果仍待取得。

### 雙重故障下的四路 PWM 互鎖（EVK 軟體流程已通過）

沿用已通過 `pwm-inhibit-cycle` 的 SafetyTestEVK 韌體，只更新 Python，不需重新燒錄：

```powershell
python E:\myGitHub\myNXPs16_Prj\_Product\Tools\safety_modbus_test.py --port COM3 pwm-multi-fault-cycle
```

本測試會產生非零 PWM，只供無產品負載的 EVK。沿用四路 PWM 測試的寫入前檢查與清理：必須為 EVK、HAL 初始化、四路 Duty=0、無 Pending、無既有 Fault／Safety 抑制，才開始。兩輪依序先清過溫及先清低電壓；每輪先建立四路 10／20／30／40% 輸出，再使兩項故障同時成立，確認四路套用值歸零。

確認帶故障 Reset 被拒絕；第一項條件恢復及重置後，剩餘 Fault／Safety 仍成立且四路套用值仍為零。此時改寫 20／30／40／50%，要求值更新但套用值仍為零。最後條件恢復但鎖存尚在時保持歸零，正確 Reset 後才恢復最後要求值。兩種解除順序都通過才回報 `pwm_multi_fault_software_cycle_pass`。

結束或失敗均先嘗試四路 Duty 歸零並還原週期／模式，再獨立關閉兩項注入；清理失敗回報 FAIL，不自動清除失敗時的 Fault。此測試觀察服務及 HAL 成功回覆後的套用值，不能證明實體 PWM 波形或排除讀取間隔內的短暫脈衝，仍須儀器量測。

新增後共 51 項 Python 離線回歸通過，包含兩種順序成功、部分 Reset 提早恢復輸出的偵測、最後鎖存前提早恢復的偵測、清理失敗仍關閉兩項注入，以及非 EVK 時不寫入。未修改 MCU 韌體；本案例 EVK 軟體流程結果如下，實體波形仍待量測。

### 四路 PWM 服務抑制（EVK 軟體流程已通過）

本次新增唯讀 PWM 服務診斷 `0x4980..0x499F`，需更新程式、Clean／Build **SafetyTestEVK** 並用更新工具下載同次 BIN；三個測試巨集不變。啟動 APP 後執行：

```powershell
python E:\myGitHub\myNXPs16_Prj\_Product\Tools\safety_modbus_test.py --port COM3 pwm-inhibit-cycle
```

此案例會在 EVK 的四路 PWM 腳位產生非零輸出（先 10／20／30／40%，再 20／30／40／50%），只供無產品負載的 EVK 測試。腳本檢查韌體 EVK flag=1、四路 HAL 已初始化、全域及通道均無抑制、Duty 要求值／套用值原本均為 0、沒有未套用 PWM Pending 與既有 Fault，才開始寫入；非 EVK 或不支援新診斷的韌體會在寫入前停止。

過溫及低電壓各測一輪，透過既有 FC10 `0x1300` 一次 Stage 四通道及 Apply key A5A5：

1. 正常狀態要求值與套用值均為 100／200／300／400 permille。
2. 觸發既有過溫或低電壓，要求全域 Safety 成立，四路服務 inhibited=1、套用值皆為 0，要求值保留。
3. 保護期間改要求值為 200／300／400／500，四路套用值仍須為 0。
4. 有效降溫／電壓恢復後，live 解除但 latch 保持，四路仍為 0。
5. 正確 Fault Reset 後，四路才解除抑制，套用最後要求值 200／300／400／500（現有服務的恢復契約）。
6. 每輪結束將 Duty 歸零；最後先以 immediate 模式確認四路要求／套用值歸零，再還原原週期及模式，Duty 保持零，最後獨立關閉兩種注入。失敗時也嘗試上述清理，不自動重置故障；任一清理失敗最終 FAIL。

`PwmOutputService` 在 `HalPwm_SetDutyPermille` 成功後才提交 applied 值，因此這項比只讀全域 inhibit 更接近輸出路徑；但讀值仍為軟體狀態，不能代替腳位波形、輸出級電位或功率量測。四個通道的實體波形及 DO 保護另列正式板待測。

每個來源顯示 `pwm_nonzero_pass`、`pwm_inhibited_pass`、`pwm_write_while_inhibited_pass` 與 `pwm_inhibit_source_pass`，兩輪及清理全部成功後才顯示 `pwm_inhibit_software_cycle_pass`。

### 廣播注入防護（EVK 本次 COM3 路徑已通過）

只更新 Python，沿用目前無效樣本版韌體，不需重新燒錄。請使用目前單台 EVK 的獨立測試線，執行：

```powershell
python E:\myGitHub\myNXPs16_Prj\_Product\Tools\safety_modbus_test.py --port COM3 broadcast-guard-cycle
```

腳本先以原站號（預設 2）關閉注入並建立正常基準，再發送站號 0 的六個測試封包：過溫與低電壓的 FC10 完整有效注入命令、各自 FC06 enable 與確認碼 word。FC10 使用正確 5AFE，以驗證「即使格式與確認碼正確，廣播仍不能啟用測試注入」。每個封包均含正常 RTU CRC。

發送排空後觀察靜默，時間為 max(0.25 秒、設定的 timeout)，預設 2 秒；收到任何 bytes（含 Echo）均停止並判 FAIL。廣播不等待 ACK，也不自動重送。隨後仍使用原站號 FC03，確認兩種注入參數／狀態、Fault／Safety 及 Event／Snapshot 最新序號未變。一般 `Client`／`--station` 仍只接受 1～247，新增廣播入口只允許指定 SafetyTest 位址，不允許其他產品設定或 Fault Reset。

六個案例顯示 `broadcast_guard_case_pass`，靜默觀察顯示 `broadcast_silence_pass`。成功、失敗或 Ctrl+C 都獨立嘗試以原站號關閉兩種注入，不清除失敗 Fault；全部通過才顯示 `broadcast_guard_software_cycle_pass`。可透過 RS485 RTU 或 USB CDC RTU 送出，但一次實測只代表當次選用的傳輸路徑，不能推定另一介面也已上板通過。此項只驗證測試注入的廣播政策，不宣稱一般產品 Register 的廣播寫入都應拒絕；瞬間 busy 時序仍未做目標板驗證。

### 注入命令防誤寫（EVK 軟體流程已通過）

使用剛通過無效樣本測試的 SafetyTestEVK 韌體即可，本次只更新 Python，不需重新燒錄：

```powershell
python E:\myGitHub\myNXPs16_Prj\_Product\Tools\safety_modbus_test.py --port COM3 injection-guard-cycle
```

先確認 Fault／Safety 正常，使用正確命令關閉兩種注入並建立參數回讀基準；接著測試共 26 種預期拒絕命令：

| 誤寫情境 | 適用入口 | 預期異常碼 |
|---|---|---|
| 錯誤注入確認碼 0000／FFFF／FE5A（正確為 5AFE） | 過溫、低電壓 | 03 |
| enable=2、valid／active=2 | 過溫、低電壓 | 03 |
| FC10 僅寫 1／3 words、誤寫 5 words | 過溫、低電壓 | 02 |
| FC06 寫 enable 或確認碼 word | 過溫、低電壓 | 02 |
| FC10 寫診斷區、從確認碼位址跨界寫入 | 過溫、低電壓 | 02 |
| FC06／FC10 寫實際 ADC valid 唯讀區 | 4964 | 02 |

每次拒絕都要求過溫注入 status／enable／valid／溫度／計數／成立狀態不變，低電壓注入 status／enable／active／limit 不變，且 Fault=0、Safety 狀態及 Event／Snapshot 最新序號不變。正常取樣的序號會自行前進，因此不將取樣序號當成誤寫不變條件。這可抓出「回了異常碼，但參數仍被寫入」的錯誤。

每項顯示 `injection_guard_rejection_pass`；成功或失敗／Ctrl+C 都獨立嘗試關閉兩種注入，失敗時不清除 Fault。全部 26 項及清理通過才顯示 `injection_guard_software_cycle_pass`／最終 PASS。此項未測目標板廣播及瞬間忙碌時序（仍僅有 Host 驗證），也未加入既有四案例 `repeat-cycle`。

### 無效溫度樣本（EVK 軟體流程已通過）

本次韌體新增 ADC 實際 valid 唯讀診斷 `0x4964..0x4967`，因此必須更新程式、Clean／Build **SafetyTestEVK** 並重新下載該組態 BIN；三個測試巨集保持不變。此欄位讀取真實 driver snapshot，不以「要求注入的 valid」代替結果。啟動 APP 後執行：

```powershell
python E:\myGitHub\myNXPs16_Prj\_Product\Tools\safety_modbus_test.py --port COM3 invalid-sample-cycle
```

先以有效 84.99°C 建立正常基準，再注入無效 85°C：等待 ADC 實際 valid=false，連續觀察至少 3.2 秒，要求有效樣本序號／最後有效溫度不變、過溫計數=0、不產生 Fault／Safety／Event／Snapshot 變化。接著有效 85°C 三次成立過溫，再注入無效 84.99°C：實際 valid=false 期間至少觀察 3.2 秒，要求計數保持 3、過溫保持成立、Safety live／latch／inhibit 保持，Fault 仍在且 Reset 被拒絕。最後有效 84.99°C 才能解除 live，重置後回復正常。

依現行設計，無效樣本不更新有效樣本序號、不改變計數，也不清除既有過溫；這不是「無效資料應新增獨立故障」的需求。觀察時長涵蓋正常 1 秒排程的多個週期，但沒有新增無效樣本嘗試次數計數器，不宣稱精確收到幾次無效樣本。測試不直接改動正式 driver 判定或 Safety 鎖存。

舊韌體不支援實際 valid 欄位時，唯讀探測即停止，不送注入。成功、失敗或 Ctrl+C 都嘗試關閉溫度注入，失敗時不自動清除 Fault。這項檢查未加入既有 `repeat-cycle`，避免改變已驗證的四案例／輪定義。實體 PWM／DO 仍待正式板量測。

### 連續循環與環形紀錄覆寫（EVK 軟體流程已通過）

沿用現有韌體，只更新 Python，不需重新燒錄。預設連續 3 輪，每輪執行過溫、低電壓、雙重故障、重置命令防護四項既有案例，共 12 個案例，需數分鐘：

```powershell
python E:\myGitHub\myNXPs16_Prj\_Product\Tools\safety_modbus_test.py --port COM3 repeat-cycle
```

也可指定 `repeat-cycle --cycles 5`，允許 2～100 輪；至少兩輪才能在本次測試中產生足夠紀錄以驗證覆寫。每項案例成功後都要求 Fault=0、Safety live／latch／state／inhibit=0，過溫及低電壓兩項注入均已關閉。任一失敗即停止後續案例，注入清理由該案例的 finally 負責，不自動清除失敗故障。

每輪檢查 Event 最新序號與 Snapshot 最新序號都前進，保留筆數符合新增數量及容量上限（目前 Event 32、Snapshot 16）。接著透過 FC06 索引／FC03 讀取全部保留紀錄（Event `0x4920`／12 words，Snapshot `0x4940`／27 words），驗證索引、類別、Event 狀態、由舊至新的連續序號，及最後一筆符合最新序號；因此可檢查容量滿後最舊紀錄被覆寫、保留順序正確且可讀。支援 U32 序號由 FFFFFFFF 回到 1、跳過 0 的既有規則。

完整紀錄讀入 JSONL 的 `retained_record`，畫面只顯示案例及輪次進度。讀取期間若 Safety 或紀錄最新狀態改變會停止；測試全程新增序號須超過 Event 32／Snapshot 16，最終保留筆數須為 32／16，才顯示 `repeat_software_cycle_pass`。此處驗證紀錄可讀、序號與覆寫順序，並非所有紀錄內容的逐筆語義核對，也不代表實體輸出量測或長期可靠性驗收已完成。

### 重置命令防護循環（EVK 軟體流程已通過）

沿用既有已通過過溫／低電壓的 SafetyTestEVK 韌體，只更新 Python，無需重新燒錄：

```powershell
python E:\myGitHub\myNXPs16_Prj\_Product\Tools\safety_modbus_test.py --port COM3 reset-guard-cycle
```

過溫 `0x0201` 與低電壓 `0x0202` 各測一輪，每輪包含下列檢查：

| 情境 | 預期 Modbus 異常碼 | 預期 Reset result（0x480B） |
|---|---|---|
| 條件仍成立，錯誤確認碼 0000／FFFF／EAC1 | 03 | 6：確認碼錯誤 |
| 無效故障代碼 0000／FFFF，確認碼 C1EA | 03 | 4：故障代碼無效 |
| 重置未成立的另一項故障 | 03 | 2：故障未成立 |
| 條件仍成立，正確代碼與確認碼 C1EA | 03 | 3：異常條件仍成立 |
| 條件恢復但鎖存尚在，錯誤確認碼 0000／FFFF／EAC1 | 03 | 6：確認碼錯誤 |
| 條件恢復後正確重置 | 無 | 1：成功 |
| 再次重置剛清除的故障 | 03 | 2：故障未成立 |

共 22 次預期拒絕與 2 次正確重置。每次拒絕均比對前後 Fault 清單、Safety live／latch／state／inhibit，以及 Event／Snapshot 最新序號，要求完全不變，避免「雖回異常碼卻清除了保護」的情況。正確重置後必須回復 Fault=0、Safety 正常。

每個拒絕案例顯示 `reset_guard_rejection_pass`，兩項完成各顯示 `reset_guard_source_pass`；結束或失敗／Ctrl+C 時獨立嘗試關閉兩項注入，不因其中一項清理失敗而省略另一項，也不自動清除失敗時的故障。只有兩輪及清理都通過才顯示 `reset_guard_software_cycle_pass` 與最終 PASS。

### 雙重故障互鎖循環（EVK 軟體流程已通過）

既有通過低電壓測試的韌體已具備兩種注入介面，本次只更新 Python／離線測試與文件，無需重新編譯或燒錄 MCU。更新 `branch2_ver1` 後，在 APP 運行時執行：

```powershell
python E:\myGitHub\myNXPs16_Prj\_Product\Tools\safety_modbus_test.py --port COM3 multi-fault-cycle
```

一個命令完成「先解除過溫」及「先解除低電壓」兩輪。每輪先觸發兩項故障並確認 Fault `0x0201`／detail 8500 與 `0x0202`／detail 1 同時存在、Safety live／latch=3、state／inhibit=1，以及新增 Event／Snapshot。兩項條件仍成立時，分別重置均須拒絕。

只恢復第一項條件時，須保持兩筆 Fault 與 latch=3；重置第一項後，live／latch 必須只留下另一項來源，另一筆 Fault 仍在、state／inhibit 仍為 1，且再次重置仍成立的故障須拒絕。另一項恢復後，live=0，但 latch／Fault／inhibit 保持到最後一次重置；全部重置後才要求 Fault=0、live／latch／state／inhibit=0。過溫仍用三次取樣確認，低電壓恢復仍要求實際連續 100 次正常取樣。

畫面分別顯示 `multi_fault_trip`、`remaining_fault_inhibit_pass`、兩筆 `multi_fault_order_pass`。結束或失敗／Ctrl+C 都獨立嘗試關閉過溫及低電壓注入；第一個清理失敗不阻擋第二個，任一無法確認關閉都使最終 FAIL。測試失敗時不自動重置故障。兩輪與兩項清理都成功後才輸出 `multi_fault_software_cycle_pass` 及最終 PASS。這仍是 EVK 軟體互鎖驗證，實體 PWM／DO 保護另待量測。

### 過溫循環

1. 唯讀檢查測試介面、Fault 數量與 Safety 初始狀態。
2. 注入有效 84.99°C，觀察至少三次正常排程的新樣本，要求過溫計數及狀態保持零。
3. 注入有效 85.00°C，要求至少三次新樣本、確認計數達三、Fault 0x0201／detail 8500、Safety live／latched bit、禁止輸出，以及 Event／Snapshot 新序號。
4. 保持高溫送 Fault Reset，要求 Modbus exception 03、Reset result=3。
5. 降回 84.99°C，要求 live 解除但 latch／輸出禁止保持。
6. 送既有 FC10 Fault Reset，要求 result=1、Fault 清除及 Safety 回復正常。
7. 正常結束或失敗／Ctrl+C 時都嘗試關閉注入；清理失敗會明確顯示「無法確認注入已關閉」，需要重新上電及檢查 Fault。不因測試失敗自動清除 Fault。

畫面顯示通過／失敗原因，退出碼 0 為通過，非零為失敗或無法執行。每次建立 `safety_test_results/日期時間.jsonl`，保存本機時區時間、通訊參數、TX／RX、樣本序號與計數、Safety 狀態、清理結果。逾時寫入不會自動重送，避免不確定的副作用。

Register 判定通過只代表軟體保護鏈驗證通過；PWM／DO 實際電位仍須儀器量測。此工具不會以讀回 inhibit 狀態推定所有實體輸出已安全，也不等於完整產品驗證通過。測試紀錄保存在執行電腦，不會自動上傳。

## 僅 SafetyTest 存在的 Register

新增命令的靜態 RAM 狀態（過溫與低電壓待處理／已套用參數、計數及旗標，Host object 確認 48 bytes）放置於 `.bss.$SRAMX`，由既有 managed linker／startup 的 `.bss_RAM3` 初始化。比僅過溫版本增加 32 bytes；函式內暫存值仍使用正常 stack，程式碼放 Flash，Python 在電腦執行。搭配六組表格後 SRAMX 預期約使用 8688 bytes，實際對齊與大小以目標板 MAP 為準。

| 位址（HEX） | 功能 |
|---|---|
| 4960～4963 | FC10 一次寫四個 word：enable、valid、S16 溫度（0.01°C）、確認碼 5AFE |
| 4964 | ADC driver snapshot 實際 valid（0／1），非注入命令回讀 |
| 4965 | ADC driver 最後有效 MCU 溫度（S16，0.01°C）；無效樣本不更新 |
| 4966～4967 | ADC driver 有效樣本序號，U32 高 word 在前；無效樣本不更新 |
| 4968 | 狀態 0 待命／1 待處理／2 成功／3 失敗 |
| 4969／496A／496B | 已套用 enable／valid／注入溫度 |
| 496C／496D | ADC driver 實際過溫計數／成立狀態 |
| 496E～496F | ADC driver 有效樣本序號，U32 高 word 在前 |
| 4970～4973 | FC10 一次寫四個 word：enable、原始低電壓 active、異常樣本上限（0=持續）、確認碼 5AFE |
| 4974 | 低電壓命令狀態 0 待命／1 待處理／2 已套用 |
| 4975～4977 | 已套用 enable／原始 active 要求／異常樣本上限 |
| 4978 | 本段實際注入的異常樣本數，65535 飽和 |
| 4979／497A | 本段最大成立／恢復計數（正式服務回報） |
| 497B | bit0=目前 confirmed；bit1=本段曾 confirmed；bit2=目前 raw；bit3=有限異常樣本已耗盡 |
| 497C～497D | 低電壓服務取樣呼叫序號，U32 高 word 在前 |
| 497E／497F | 正式成立／恢復門檻，目前 3／100 |
| 4980／4988／4990／4998 起各 8 words | 四通道 PWM 服務唯讀診斷：offset 0 要求 Duty、1 套用 Duty（permille）、2 inhibited、3～4 要求週期（U32 ms）、5 更新模式、6 HAL initialized、7 EVK build flag |

命令透過目前的 USB／RS485 request context 區分 addressed／broadcast；廣播不執行，Modbus 規則下也不回覆。部分寫入、FC06 測試寫入、錯誤確認碼及忙碌請求均拒絕。命令只更新 RAM，不寫 NVM；重新上電恢復真實溫度。正式 Debug／Release 的該區回覆 Illegal Data Address。

## 後續共用 FC03／06／10 操作

```powershell
python _Product\Tools\safety_modbus_test.py --port COM8 read 0x4810 16
python _Product\Tools\safety_modbus_test.py --port COM8 write06 0x4801 0
python _Product\Tools\safety_modbus_test.py --port COM8 write10 0x4809 0x0201 0xC1EA
```

數字預設十進位；HEX 一律加 `0x`。後續測試新增自動案例，重用同一組通訊與紀錄，不需再使用 debugger call。

## 驗證狀態


2026-10-24 21:04:03～21:04:07（台灣時間）：沿用目前 Release APP，使用者上傳 `貼上的文字 (1)(20261024-130420).txt`。清單顯示 COM8「USB 序列裝置」、COM3「USB-SERIAL CH340」，這次選擇第 2 項，`start.port=COM8`，確定與先前 CH340 COM3 路徑不同。

已逐筆解析 `production-guard-cycle`：FC03 64 個位址連續涵蓋 `0x4960..0x499F`、FC06 4 筆、FC10 5 筆，全部 exception=2；`production_guard_cycle_pass` rejected_cases=73，最終 PASS。依腳本判定前後 Fault count／Safety／Event／Snapshot 不變。站號 2、工具設定 115200／8N1（USB CDC 實際傳輸速率不由此 baud 決定），JSONL 位於 `C:\Users\ch\safety_test_results\20261024_210403_620501.jsonl`。

本次記為 Release USB CDC 路徑測試 Register 封鎖通過。21:00:56～21:01:01 的前一次上傳 `貼上的文字 (1)(20261024-130114).txt` 雖同樣 73 項 PASS，但選的是第 3 項 COM3，只是 CH340 路徑重測，未計入 USB CDC。本次已審核終端輸出，完整 JSONL 未取得；不推定 Debug USB CDC 及其他注入循環亦已在 USB 路徑完成。


2026-10-24 20:57:25～20:57:40（台灣時間）：使用者依前一步 Release 組態測試流程，上傳 `貼上的文字 (1)(20261024-125751).txt`，內容為 COM3 `production-guard-cycle` 及 Release 路徑 `production-symbol-check` 的終端輸出。

已逐筆解析核對 73 筆拒絕：FC03 的 64 個位址連續涵蓋 `0x4960..0x499F`、FC06 共 4 筆、FC10 共 5 筆，全部 exception=2，`production_guard_cycle_pass` rejected_cases=73，最終 PASS。依腳本判定，前後 Fault count／Safety／Event／Snapshot 狀態不變。COM3、115200／8N1、站號 2；JSONL 位於 `C:\Users\ch\safety_test_results\20261024_205725_863328.jsonl`。

| Release AXF 欄位 | 本次結果 |
|---|---|
| 路徑 | `E:\myGitHub\myNXPs16_Prj\Release\myNXPs16_Prj.axf` |
| 檔案大小 | 1495256 bytes（含符號／除錯資訊，不代表 Flash payload） |
| ARM APP 入口 | `0x00008171` |
| 已定義符號數 | 1847 |
| 指定禁止符號 | 空陣列 `[]` |
| SHA-256 | `06bfdab1eb4ea9c783f0679f9d0d8796ce46ff032029669ee6b690ed952e9904` |
| JSONL | `C:\Users\ch\safety_test_results\20261024_205740_363674.jsonl` |

Release AXF 檢查回報 `production_symbol_check_pass` 及最終 PASS。兩項記為本次 Release 驗證流程通過；已取得終端輸出，尚未取得完整 JSONL、AXF 本體及建置／下載紀錄。Debug／Release 的本次具名符號結果均已通過，但各次下載 BIN 對應、三個巨集排除與另一通訊介面仍待補驗；不由此推定 LTO 內嵌程式或匿名 RAM 狀態均已獨立排除。


2026-10-24 20:52:37（台灣時間）：使用者執行 `production-symbol-check --axf "E:\myGitHub\myNXPs16_Prj\Debug\myNXPs16_Prj.axf"`，回報 `production_symbol_check_pass` 及最終 PASS。

| 欄位 | 本次結果 |
|---|---|
| AXF 檔案大小 | 2674720 bytes（含符號／除錯資訊，不代表 Flash payload） |
| ARM APP 入口 | `0x00008171` |
| 已定義符號數 | 1928 |
| 指定禁止符號 | 空陣列 `[]` |
| AXF SHA-256 | `79a4308ac6a77eb51a494cad2a219a947f879afd2ab70779d02f307093fbfe42` |
| JSONL | `C:\Users\ch\safety_test_results\20261024_205237_149509.jsonl` |

本次 Debug 路徑 AXF 未發現指定的已定義測試符號。證據為使用者貼出的終端輸出，未取得 AXF 本體或完整 JSONL 供獨立重跑；不將此結果延伸為 Release 或所有測試 RAM 狀態已排除。先前 COM3 的 73 項 Register 封鎖亦通過，但仍須由建置／下載紀錄確認兩次測試對應同次映像，並審核三個測試／EVK 巨集未定義。AXF 大小包含除錯資訊，不能拿來與 104 KiB Flash 容量直接比較。


2026-10-24 20:45:13～20:45:18（台灣時間）：使用者上傳 `貼上的文字 (1)(20261024-124533).txt`，為執行 `production-guard-cycle` 的終端輸出，COM3、115200／8N1、站號 2。已逐筆解析並核對：FC03 恰有 64 筆，位址連續涵蓋 `0x4960..0x499F`；FC06 為 4 筆，FC10 為 5 筆，全部 exception=2；`production_guard_cycle_pass` 的 rejected_cases=73，最終 `complete` PASS，沒有錯誤或清理失敗。

依腳本判定，正常 Safety／Fault count 可讀取且讀寫檢查前後 Fault count、Safety live／latch／state／inhibit、Event／Snapshot 計數及序號保持不變。此案例未啟用注入、未更動產品輸出、未清除 Fault。JSONL 位於使用者電腦 `C:\Users\ch\safety_test_results\20261024_204513_967980.jsonl`；已收到並審核終端輸出，但未取得含原始 TX／RX 的完整 JSONL。

本次記為「當次下載映像及 COM3 通訊路徑的測試 Register 封鎖通過」。終端輸出未包含 Debug／Release 組態、映像雜湊或建置報表，不能據此將兩種組態與所有介面一併判定完成，也未證明 AXF 不含測試符號；其餘仍列待驗證。


2026-10-24 20:34:29～20:34:41（台灣時間）：使用者在 EVK 執行 `pwm-multi-fault-cycle`，COM3、115200／8N1、站號 2。先清過溫及先清低電壓兩種順序，各回報 `pwm_multi_fault_trip_pass`、`pwm_remaining_fault_pass` 及 `pwm_multi_fault_order_pass`；最終 `pwm_multi_fault_software_cycle_pass`（channels=4、sources=2）及 `complete` PASS。

兩輪雙重故障成立時，四路要求 Duty=100／200／300／400 permille，套用值全為 0、inhibited=1；僅清除第一項故障後同樣保持歸零。依腳本判定，剩餘故障期間改寫要求值、拒絕帶故障 Reset，以及全部條件恢復後的最後鎖存階段均保持抑制；最後正確 Reset 後，四路套用值才恢復為最後要求值 200／300／400／500 permille、inhibited=0。各通道回報 period=10、mode=0、hal_initialized=1、evk=1。

清理完成四路 Duty=0、原週期／模式還原，兩項注入均關閉。JSONL 位於使用者電腦 `C:\Users\ch\safety_test_results\20261024_203429_623912.jsonl`；證據為貼出的終端輸出，完整 JSONL 尚未取得。本次記為 EVK 雙重故障四路 PWM 服務互鎖通過；套用值代表服務獲得 HAL 成功回覆，實際 PWM 波形、DO 電位、讀取間隔內短暫脈衝與正式板仍待儀器量測。

PWM 服務抑制新增回歸後共 46 項 Python 離線測試通過，涵蓋兩個來源、四通道、保護期間改寫、提早恢復／未抑制／不恢復的偵測、拒絕非 EVK 與既有非零輸出、PWM 清理失敗仍清理兩項注入。Host 實際 Modbus adapter＋PwmOutputService＋HalPwm＋mock driver 測試確認診斷與 HAL Duty 一致、抑制時四路歸零、解除後恢復、讀取邊界及唯讀限制；正式 DEBUG=1／0 Modbus 仍不公開此區。靜態測試狀態仍為 48 bytes SRAMX，無新增靜態 RAM；PWM EVK 軟體流程結果如下；尚未取得目標 ARM 建置報表，實體波形仍待量測。


2026-10-24 20:22:59～20:23:06（台灣時間）：使用者在 EVK 執行 `pwm-inhibit-cycle`，COM3、115200／8N1、站號 2，兩項來源各回報 `pwm_nonzero_pass`、`pwm_inhibited_pass`、`pwm_write_while_inhibited_pass` 及 `pwm_inhibit_source_pass`，最後 `pwm_inhibit_software_cycle_pass`（channels=4、sources=2）及最終 PASS。

過溫 `0x0201` 與低電壓 `0x0202` 的四路要求／套用值最初為 100／200／300／400 permille；故障後套用值全為 0、inhibited=1。抑制期間改寫為 200／300／400／500 permille，要求值更新而套用值保持 0；依腳本判定，條件恢復後鎖存仍抑制，正確 Reset 後套用值才恢復為最後要求值、inhibited=0。各通道回報 period=10、mode=0、hal_initialized=1、evk=1。

清理回報四路 PWM 歸零、原週期／模式已還原、Duty 保持零，過溫及低電壓注入均關閉。JSONL 位於使用者電腦 `C:\Users\ch\safety_test_results\20261024_202259_516547.jsonl`；證據為貼出的終端輸出，完整 JSONL 尚未取得。本次記為 EVK 四路 PWM 服務抑制通過；套用值代表服務已取得 HAL 成功回覆，實際 PWM 波形、DO 電位及正式板仍待量測。

廣播防護新增回歸後共 39 項 Python 離線測試通過：新增實際站號 0 RTU frame／CRC／TX 排空、靜默觀察、回覆或 Echo 時失敗且還原 timeout、禁止廣播其他產品位址、六種命令參數不變、抓出廣播被套用，以及清理失敗不得通過。無 MCU 程式變更。

2026-10-24 20:04:28～20:04:43（台灣時間）：使用者在 EVK 執行 `broadcast-guard-cycle`，COM3、115200／8N1，廣播站號 0、讀回站號 2。終端輸出顯示 6 筆 `broadcast_silence_pass`（各觀察 2 秒）、6 筆 `broadcast_guard_case_pass`，兩項注入均 `disabled`、`broadcast_guard_software_cycle_pass`（cases=6）與最終 PASS。

依腳本判定，廣播未回覆、過溫及低電壓注入均未啟用，注入參數與 Fault／Safety／Event／Snapshot 保持不變，正常站號仍可讀回。本次工作目錄是 `C:\Users\ch`，JSONL 位於 `C:\Users\ch\safety_test_results\20261024_200428_761420.jsonl`；證據為貼出的終端輸出，完整 JSONL 尚未取得。只記為當次 COM3 傳輸路徑通過，不推定另一介面也已通過。目標板瞬間 busy 時序及實體 PWM／DO 仍待測。

注入防誤寫新增回歸後共 33 項 Python 離線測試通過，新增 26 種拒絕成功、回異常碼卻改參數的偵測、錯誤 key 被接受的偵測、兩項清理均須嘗試及清理失敗不得通過、舊韌體不寫入等案例。無 MCU 程式變更。

2026-10-24 19:44:00～19:44:08（台灣時間）：使用者在 EVK 執行 `injection-guard-cycle`，COM3、115200／8N1、站號 2。終端輸出為 26 筆 `injection_guard_rejection_pass`，錯誤 key／非法參數均 exception=3，部分／超長／FC06／唯讀／跨界寫入均 exception=2；最後兩項注入 `disabled`、`injection_guard_software_cycle_pass`（rejected_cases=26）及最終 PASS。

依腳本判定，每次拒絕後的注入參數、Fault／Safety 狀態、Event／Snapshot 最新序號均不變。JSONL 位於使用者電腦 `C:\safety_test_results\20261024_194400_196361.jsonl`；證據為貼出的終端輸出，完整 JSONL 尚未取得。EVK 注入命令防誤寫記為通過；目標板廣播／忙碌時序及實體 PWM／DO 仍待測，不能由這 26 項推定全部介面情境均已完成。

無效樣本新增回歸後共 28 項 Python 離線測試通過，新增成功路徑、無效樣本誤解除保護的偵測、舊韌體不寫入及清理失敗不得通過。真實 ADC driver／SafetyTest 命令 Host 測試確認實際 valid、最後有效溫度、無效低溫保留三次過溫狀態及讀取邊界。靜態測試狀態仍為 48 bytes SRAMX，只有額外唯讀欄位與程式碼；目標 ARM 建置報表尚未取得。

2026-10-24 19:36:44～19:36:57（台灣時間）：使用者在 EVK 執行 `invalid-sample-cycle`，COM3、115200／8N1、站號 2，回報兩段 `invalid_sample_hold_pass`、溫度注入 `disabled`、`invalid_sample_software_cycle_pass` 與最終 PASS。無效高溫要求值 8500、實際 valid=false、count=0、trip=false；已過溫後無效低溫要求值 8499、實際 valid=false、count=3、trip=true，兩段各觀察至少 3.2 秒。

依腳本判定，無效期間最後有效值與有效樣本序號保持、Fault／Safety／Event／Snapshot 未誤變更、無效低溫時 Reset 被拒絕，最後有效低溫及正確重置後正常。JSONL 位於使用者電腦 `C:\safety_test_results\20261024_193644_508652.jsonl`；證據為貼出的終端輸出，完整 JSONL 尚未取得。EVK 無效樣本軟體保護記為通過，實體輸出及正式板感測器電路仍待測。

連續循環新增離線回歸後共 24 項 Python 測試通過，新增案例覆蓋兩輪八案例的排程、紀錄滿載與覆寫、重複序號偵測、序號倒退／疑似重啟偵測、首個失敗立即停止、輪數限制與 U32 跳零規則。各既有案例仍跑原本的 18 項回歸，無 MCU 程式變更。

2026-10-24 19:20:44～19:23:08（台灣時間）：使用者上傳 `貼上的文字 (1)(20261024-112405).txt`，內容為 EVK 執行 `repeat-cycle` 的終端輸出，COM3、115200／8N1、站號 2。確認 12 筆案例通過、3 筆輪次通過、66 筆重置拒絕通過、18 筆注入清理訊息，沒有 cleanup_failed；最終 `repeat_software_cycle_pass`（rounds=3、cases=12）及 `complete` PASS。

| 輪次 | Event 保留筆數／最新序號 | Snapshot 保留筆數／最新序號 | Safety live／latch／state／inhibit |
|---|---|---|---|
| 1 | 32／75 | 16／30 | 0／0／0／0 |
| 2 | 32／115 | 16／46 | 0／0／0／0 |
| 3 | 32／155 | 16／62 | 0／0／0／0 |

依腳本判定，各項結束 Fault=0 且兩項注入關閉，三輪全部保留紀錄的索引、類別與連續序號檢查均通過，滿載後序號繼續前進且保留筆數維持容量，未觸發序號倒退／筆數異常檢查。這不等於獨立量測證明沒有任何 MCU reset，也不代表長期可靠性驗收完成。

使用者電腦 JSONL 位於 `C:\safety_test_results\20261024_192044_582132.jsonl`；本次已取得終端紀錄，但未取得包含原始 TX／RX 及 retained_record 的完整 JSONL，逐筆原始內容未另行審核。EVK 連續循環與覆寫記為通過，實體 PWM／DO 仍待正式板量測。

重置防護新增離線回歸後共 18 項 Python 測試通過，新增案例確認兩項來源及 22 次拒絕、抓出錯誤 key 被接受的回歸、清理失敗不得回報通過，以及既有 Fault 時不寫入。未修改 MCU 程式，實體輸出仍須正式板量測。

2026-10-24 18:58:48～18:59:07（台灣時間）：使用者在 EVK 執行 `reset-guard-cycle`，COM3、115200／8N1、站號 2，終端輸出顯示 22 筆 `reset_guard_rejection_pass`、兩筆來源 `0x0201`／`0x0202` 的 `reset_guard_source_pass`、兩項注入均 `disabled`、`reset_guard_software_cycle_pass` 與最終 `PASS`。錯誤確認碼 result=6、無效代碼 result=4、未成立／重複重置 result=2、條件仍成立 result=3，均符合預期；依腳本判定，各次拒絕前後的 Fault／Safety 與 Event／Snapshot 最新序號不變，兩次正確重置後正常。

紀錄位於使用者電腦 `C:\safety_test_results\20261024_185848_987746.jsonl`。證據為貼出的終端輸出，完整 JSONL 尚未取得；EVK 重置命令防護記為通過，不取代正式板實體輸出量測。

雙重故障新增 Python 離線回歸後共 14 項測試通過：包含兩種解除順序、剩餘故障仍成立時禁止解除輸出、任一注入清理失敗仍嘗試清理另一項、Ctrl+C 清理、舊韌體不支援及既有故障時不寫入。沒有更動 MCU 程式。

2026-10-24 18:45:09～18:45:22（台灣時間）：使用者在 EVK 執行 `multi-fault-cycle`，COM3、115200／8N1、站號 2，終端輸出顯示兩輪 `multi_fault_order_pass`、過溫及低電壓注入均 `disabled`、`multi_fault_software_cycle_pass` 與最終 `PASS`。依腳本判定，同時故障、故障代碼／detail、拒絕帶故障重置、部分恢復仍鎖定、單一重置不影響剩餘故障、全部恢復後最後重置才解除等檢查通過。

| 解除順序 | 雙重故障 live／latch | 第一項重置後 live／latch | 剩餘故障時 state／inhibit | 成立時 Event／Snapshot 最新序號 |
|---|---|---|---|---|
| 先過溫、後低電壓 | 3／3 | 2／2 | 1／1 | 9／6 |
| 先低電壓、後過溫 | 3／3 | 1／1 | 1／1 | 19／10 |

紀錄位於使用者電腦 `C:\safety_test_results\20261024_184509_797979.jsonl`。證據為貼出的終端輸出，完整 JSONL 尚未取得；Event／Snapshot 計數屬同次 APP 執行的累計值，不代表每輪新增相同筆數。雙重故障 EVK 軟體互鎖記為通過，實體 PWM／DO 仍待正式板量測。

低電壓注入新增 Host 整合測試，連結真實測試命令、低電壓服務及 Fault 程式，在 EVK 與一般 SafetyTest 模式均通過 1／2／3 次成立邊界、99／100 次恢復邊界、命令 key／長度／忙碌／廣播限制與停用後來源恢復；正式 Modbus 對低電壓測試區回覆 Illegal Data Address。Python 離線案例共 8 項通過，包含低電壓成功、Safety 傳播失敗、清理失敗及不支援舊韌體時不寫入。目標 ARM 連結報表尚未取得。

2026-10-24 18:36:53～18:36:55（台灣時間）：使用者在 EVK 執行 `low-voltage-cycle`，COM3、115200／8N1、站號 2，終端輸出回報 1 次／2 次短脈衝通過（peak=1／2）、`low_voltage_software_cycle_pass`、注入 `disabled`、最終 `PASS`。依腳本判定，此次通過三次成立、Fault `0x0202`／detail 1、低電壓異常持續時拒絕重置、連續正常 100 次恢復後鎖存保持，以及 Reset 回復正常。

低電壓觸發時 Safety live=2、latch=2、state=1、inhibit=1；Event／Snapshot 各 2 筆、最新序號各 2。紀錄位於使用者電腦 `C:\safety_test_results\20261024_183653_570297.jsonl`。證據為使用者貼出的終端輸出，完整 JSONL 尚未取得；EVK 低電壓軟體流程記為通過，外部偵測電路與實際 PWM／DO 仍為正式板待測。

2026-10-24 EVK 隔離變更：Host 已確認 EVK application object 不再引用外部 ADC、感測器設定及 NVM 路徑，正式模式保留引用；低電壓服務在 EVK 忽略外部輸入、正式模式保留三次判定；不合法的 EVK 巨集組合會拒絕編譯。產品 driver、Python 四項離線案例與校正／正式 Modbus 回歸通過。

新增命令入口與真實 ADC driver 的 Host 邊界／三次計數測試，以及 Python 通訊、正常循環、失敗判定與清理的離線測試，均與實機結果分別記錄。

2026-10-24 17:57:25～17:57:33（台灣時間）：使用者於 EVK 執行 `python E:\myGitHub\myNXPs16_Prj\_Product\Tools\safety_modbus_test.py --port COM3`，115200／8N1、站號 2，終端輸出回報 `software_cycle_pass`、注入 `disabled`、最終 `PASS`。依腳本判定，此次通過 84.99°C 不觸發、85°C 三次成立、Fault detail、高溫拒絕重置、降溫後鎖定保持及重置恢復正常。

觸發時回報 Safety live=1、latch=1、state=1、inhibit=1；Event／Snapshot 各 2 筆、最新序號各 2。紀錄位於使用者電腦 `C:\safety_test_results\20261024_175725_721874.jsonl`。本次證據為使用者貼出的終端輸出，完整 JSONL 尚未取得；未另行取得 MCUXpresso 建置報表。EVK 過溫軟體流程記為通過，正式板與實體 PWM／DO 量測仍為待測。

2026-10-24：使用者的 SafetyTest 連結報表為主 SRAM 66900 bytes／64 KiB，溢位 1364 bytes，SRAMX 為零。子模組更新只移動六組表格、共預期 8640 bytes 至既有 `.bss.$SRAMX`，不擴大主 SRAM 或改動 heap／stack。既有 managed linker 的 `.bss_RAM3` 與 startup section table 負責初始化，保留末端 64-byte Bootloader mailbox。若其他編譯設定一致，主 SRAM 預期約降至 58260 bytes；最終大小必須以新的 MCUXpresso MAP 為準。
