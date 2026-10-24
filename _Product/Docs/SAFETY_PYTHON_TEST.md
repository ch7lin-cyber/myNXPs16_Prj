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
| 4968 | 狀態 0 待命／1 待處理／2 成功／3 失敗 |
| 4969／496A／496B | 已套用 enable／valid／注入溫度 |
| 496C／496D | ADC driver 實際過溫計數／成立狀態 |
| 496E～496F | ADC driver 實際樣本序號，U32 高 word 在前 |
| 4970～4973 | FC10 一次寫四個 word：enable、原始低電壓 active、異常樣本上限（0=持續）、確認碼 5AFE |
| 4974 | 低電壓命令狀態 0 待命／1 待處理／2 已套用 |
| 4975～4977 | 已套用 enable／原始 active 要求／異常樣本上限 |
| 4978 | 本段實際注入的異常樣本數，65535 飽和 |
| 4979／497A | 本段最大成立／恢復計數（正式服務回報） |
| 497B | bit0=目前 confirmed；bit1=本段曾 confirmed；bit2=目前 raw；bit3=有限異常樣本已耗盡 |
| 497C～497D | 低電壓服務取樣呼叫序號，U32 高 word 在前 |
| 497E／497F | 正式成立／恢復門檻，目前 3／100 |

命令透過目前的 USB／RS485 request context 區分 addressed／broadcast；廣播不執行，Modbus 規則下也不回覆。部分寫入、FC06 測試寫入、錯誤確認碼及忙碌請求均拒絕。命令只更新 RAM，不寫 NVM；重新上電恢復真實溫度。正式 Debug／Release 的該區回覆 Illegal Data Address。

## 後續共用 FC03／06／10 操作

```powershell
python _Product\Tools\safety_modbus_test.py --port COM8 read 0x4810 16
python _Product\Tools\safety_modbus_test.py --port COM8 write06 0x4801 0
python _Product\Tools\safety_modbus_test.py --port COM8 write10 0x4809 0x0201 0xC1EA
```

數字預設十進位；HEX 一律加 `0x`。後續測試新增自動案例，重用同一組通訊與紀錄，不需再使用 debugger call。

## 驗證狀態

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
