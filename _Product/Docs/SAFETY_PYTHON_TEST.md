# Python 系統安全上板測試

2026-10-24：改用 FC03／06／10 測試工具，MCU 正常運行並餵 Watchdog，不需 debugger call。第一個自動案例為 MCU 過溫循環；低電壓、其他故障及實體輸出量測尚未加入自動循環。

## 首次準備

1. 更新 `branch2_ver1`，執行 `git submodule sync --recursive`、`git submodule update --init --recursive`。本版本固定 PlatformCore `843cc36`，將 TC-B／C／D 執行期表格移至 SRAMX，避免 SafetyTest 主 SRAM 溢位。
2. SafetyTest 組態的 C compiler 同時定義 `PRODUCT_SAFETY_TEST_BUILD=1`、`PRODUCT_SAFETY_TEST_TARGET=1`。一般 Debug／Release 不定義。已有的測試函式 `--undefined` 連結選項可移除，現在正常程式流程會引用該函式。
3. Clean／Build SafetyTest，透過更新工具下載同次產生的 SafetyTest BIN，並啟動 APP。Bootloader 需為 00.01.11 或後續相容版本，APP 保持 104 KiB 分區。
4. 電腦執行一次 `python -m pip install pyserial`。關閉其他占用 DUT COM 埠的工具，結束 SWD 除錯連線。
5. 測試會真正觸發 Safety inhibit，請使用允許安全停機的測試設備。先確認沒有其他 Fault／Safety inhibit；程式不會為了開始測試而清除既有 Fault。

## 直接執行

```powershell
python E:\myGitHub\myNXPs16_Prj\_Product\Tools\safety_modbus_test.py
```

預設 RTU、站號 2、115200、8N1。只有一個 COM 埠時自動選用，有多個則選擇 DUT 編號。對不支援測試區的正式韌體，第一筆唯讀探測便停止，不進行注入。COM 埠存在不代表是 DUT；腳本不向其他 COM 埠輪流發送測試命令。

需要指定連線時：

```powershell
python _Product\Tools\safety_modbus_test.py --port COM8 --baud 115200 --station 2
```

可使用 USB CDC Modbus 或 RS485 Modbus RTU；USB 的 baud 欄位不代表 USB 實際傳輸速率。RS485 使用 APP 當前設定。此工具目前不提供 ASCII。

## 自動循環與判定

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

新增命令的靜態 RAM 狀態（待處理／已套用參數、執行狀態及 request 旗標，約 16 bytes）放置於 `.bss.$SRAMX`，由既有 managed linker／startup 的 `.bss_RAM3` 初始化。函式內暫存值仍使用正常 stack；程式碼放 Flash，Python 在電腦執行。搭配六組表格後 SRAMX 預期約使用 8656 bytes，實際大小以目標板 MAP 為準。

| 位址（HEX） | 功能 |
|---|---|
| 4960～4963 | FC10 一次寫四個 word：enable、valid、S16 溫度（0.01°C）、確認碼 5AFE |
| 4968 | 狀態 0 待命／1 待處理／2 成功／3 失敗 |
| 4969／496A／496B | 已套用 enable／valid／注入溫度 |
| 496C／496D | ADC driver 實際過溫計數／成立狀態 |
| 496E～496F | ADC driver 實際樣本序號，U32 高 word 在前 |

命令透過目前的 USB／RS485 request context 區分 addressed／broadcast；廣播不執行，Modbus 規則下也不回覆。部分寫入、FC06 測試寫入、錯誤確認碼及忙碌請求均拒絕。命令只更新 RAM，不寫 NVM；重新上電恢復真實溫度。正式 Debug／Release 的該區回覆 Illegal Data Address。

## 後續共用 FC03／06／10 操作

```powershell
python _Product\Tools\safety_modbus_test.py --port COM8 read 0x4810 16
python _Product\Tools\safety_modbus_test.py --port COM8 write06 0x4801 0
python _Product\Tools\safety_modbus_test.py --port COM8 write10 0x4809 0x0201 0xC1EA
```

數字預設十進位；HEX 一律加 `0x`。後續測試新增自動案例，重用同一組通訊與紀錄，不需再使用 debugger call。

## 驗證狀態

新增命令入口與真實 ADC driver 的 Host 邊界／三次計數測試，以及 Python 通訊、正常循環、失敗判定與清理的離線測試。上板結果尚待使用者執行本腳本確認，不能將離線模擬結果當成上板成功。

2026-10-24：使用者的 SafetyTest 連結報表為主 SRAM 66900 bytes／64 KiB，溢位 1364 bytes，SRAMX 為零。子模組更新只移動六組表格、共預期 8640 bytes 至既有 `.bss.$SRAMX`，不擴大主 SRAM 或改動 heap／stack。既有 managed linker 的 `.bss_RAM3` 與 startup section table 負責初始化，保留末端 64-byte Bootloader mailbox。若其他編譯設定一致，主 SRAM 預期約降至 58260 bytes；最終大小必須以新的 MCUXpresso MAP 為準。
