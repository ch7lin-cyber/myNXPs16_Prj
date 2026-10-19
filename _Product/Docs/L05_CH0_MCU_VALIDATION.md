# CH0 MovingAverage MCU 驗證操作

狀態：方法及 Host 支援已建立；**MCU 未執行，所有 MCU 結果為 NOT RUN**。
適用 branch2_ver1 的 L03 Snapshot → L05 scheduler/adapter → L04 MovingAverage。
這條鏈只讀 PV，結果留在 L05，不控制 PWM/DAC；本次不修改 Windows UI。

## 1. 建置與證據

1. 記錄 `git rev-parse HEAD`、`git submodule status`、板號、MCU、Sensor Type、
   接線、輸入設備、MCUXpresso/SDK/GCC 版本、最佳化等級、實際 CPU 時脈及測試者日期。
2. 先用現有 Debug build，SWD 下載 ELF；停於 `ProductFbScheduleIntegration_Process()`。
   在 Variables/Expressions 展開 `g_ch0_moving_average_adapter`：
   `output.average/sample_count/valid`、`last_sample_sequence`、
   `has_last_sample_sequence`、`callback_count`、`last_elapsed_ms`、`last_status`。
   `g_schedule`、`g_task_states` 可確認 task 的 phase、last_call。
   不要在 GDB 呼叫函式以讀結果；Debugger 直接讀記憶體，避免 target function call 副作用。
   既有 GetCh0MovingAverage/GetCh0Diagnostics 仍可供應用取得資料。
3. 單張快照不足以核對完整序列。需記錄時，僅在測試 build 的 C Compiler
   Preprocessor symbols 加入 `PRODUCT_FB_CH0_TRACE_ENABLED=1`，Clean/Rebuild。
   預設 0：不配置 Trace、不讀 DWT、不改 callback。既有 L05 source path 即可編譯，無新增 .c。
   Trace 是 512 筆、滿後停止、無 malloc/printf/通訊；一般 ARM ABI 約 32 KiB，
   **以 build map/sizeof 確認實際 RAM，保留 stack/heap 空間**；若 RAM 不足先不要下載。
4. 檢查 Expressions 有 `g_product_fb_ch0_trace`，schema_version=1、count=0
   （Init 後）、capture=1。資料從整合 Init 起開始，滿 512 筆約 10.24 秒。
   暫停記錄只寫 `capture=0`；不可手改 count/init_ms 或覆寫 records 後續接舊資料。
   要新測試請 reset/restart 走正常 Init。Init 不會清除 records 尾端，只有 `[0,count)` 有效。

## 2. 功能實測：先真實輸入，再做可重現的邊界注入

### A. 真實感測器／校正器

接上 CH0 與正確 Sensor Type，保持有效穩定輸入，再做安全範圍內階躍。
停於 adapter callback 完成後，確認 Snapshot 的 `engineering_value` 被消費，
Trace provider_ok=1、input_valid=1；序號變動時 accepted=1。
平均直接使用 **int32 engineering_value 的原始尺度**；不要自行假設是 float PV 或 °C。
每 20 ms 消費當下最新 Snapshot，並不保證涵蓋 ADC 每一筆樣本。

依測試接線選擇可安全產生斷線/無效量測的方式，確認 Snapshot valid=false 後下一 callback
清窗；恢復後從一筆重建。若實際產品診斷有確認延遲，要等 valid 變化再判斷清窗。
拔線未必對每一種 Sensor Type 都會變無效，不能只看拔線動作判定 adapter 失敗。
若 10 秒不足完成動作，可在 Init 後先停用 capture，在開始測試前重新 reset 啟動；
完整功能 CSV 必須從空視窗記錄，不能任意在中間才啟用 capture。

### B. SWD 邊界注入（獨立於真實輸入驗證，不當成 ADC 實測）

在 `ReadCh0Sample()` 的 `sample->sample_sequence = snapshot.sample_sequence;`
設 breakpoint；此時 GetSnapshot 已成功，且仍未複製給 adapter。
每次命中以 Variables 或 GDB 修改**區域變數 snapshot** 的 sequence/engineering_value/valid，
然後 Resume 到下一次命中。不要改 L03 全域狀態，也不要改 Safety/輸出。
最佳化讓區域變數不可寫時，這輪功能驗證使用 -O0/-Og；記錄編譯設定。
每次同時設定三個欄位，例：

```gdb
set var snapshot.sample_sequence = 1
set var snapshot.engineering_value = 100
set var snapshot.valid = 1
```

| callback | sequence | PV | valid | accepted | count | average | output valid |
|---|---:|---:|---:|---:|---:|---:|---:|
| 1 | 1 | 100 | 1 | 1 | 1 | 100 | 1 |
| 2 | 2 | 200 | 1 | 1 | 2 | 150 | 1 |
| 3 | 3 | 300 | 1 | 1 | 3 | 200 | 1 |
| 4 | 4 | 400 | 1 | 1 | 4 | 250 | 1 |
| 5 | 5 | 500 | 1 | 1 | 5 | 300 | 1 |
| 6 | 6 | 600 | 1 | 1 | 5 | 400 | 1 |
| 7 | 6 | 999 | 1 | 0 | 5 | 400 | 1 |
| 8 | 6 | 0 | 0 | 0 | 0 | 0 | 0 |
| 9 | 6 | 0 | 0 | 0 | 0 | 0 | 0 |
| 10 | 6 | -40 | 1 | 1 | 1 | -40 | 1 |
| 11 | 7 | 60 | 1 | 1 | 2 | 10 | 1 |

每次都是 callback 所看到的輸入，不是校正器物理單位；第 7 筆故意同序號不同 PV，應忽略。
第 10 筆刻意沿用無效前序號，必須重新接受。
最後一筆 Resume 執行完成後，再停住，設 capture=0 並匯出。
可在每次 callback 後核對既有 API 對應結構；輸出 count 應永不超過 5，status=0。
本輪 breakpoint 的 cycles/elapsed_ms 不作時序驗收。

## 3. 不設 breakpoint 的排程與執行時間測試

另 reset 一輪，使用接近正式產品的最佳化/連結設定，只增加 Trace。
在正常 Init 完成、第一個 callback 之前暫停，由 GDB 啟用 Cortex-M DWT cycle counter：

```gdb
set {unsigned int}0xE000EDFC = (*(unsigned int*)0xE000EDFC) | 0x01000000
set {unsigned int}0xE0001004 = 0
set {unsigned int}0xE0001000 = (*(unsigned int*)0xE0001000) | 1
```

這些只由 debugger 設定，firmware 不自行開啟硬體計時。
確認 DWT 可用且 Resume 後 CYCCNT 前進；cycles_valid=0 表示不能判定時間。
不支援或受安全權限限制時，以 MCUXpresso 可用的硬體 profile/trace 另測並附原始記錄，
不可將 cycles=0 當作執行時間為零。
Resume 後全程不用單步、不設 breakpoint、不使用週期性 live watch，等待 >11 秒再 Halt；
count 應為 512、capture=0。正常輸入與 Modbus 等預期最大負載各跑至少 3 輪。

- `elapsed_ms`：相鄰 callback 的實際 scheduler now_ms 差。
- `lateness_ms`：距**最早尚未執行的 20 ms release** 的延遲。晚到時合併執行一次、保留相位。
  因此可能 >20，不是對 20 取餘數；不要只看平均週期掩蓋漏期。
- `cycles / CPU_Hz × 10^6`：snapshot 讀取 + adapter + L04 的經過微秒，包含期間 IRQ 搶占；
  不含 Trace 寫入和 scheduler 外圍。不代表整個主迴圈 WCET。
- DWT 差值只支援單次 <2^32 cycles，量測期間不可改時脈/低功耗；若計數器在 sleep 停止，
  結果僅代表活動 cycle，不是牆鐘時間。now_ms 是既有毫秒時基，無法證明 sub-ms 抖動。
- Trace 本身有 RAM/時序成本；量測是有觀測開銷的上板證據，不能宣稱無 Trace 的 WCET 證明。

開跑前在記錄中訂定 CPU_Hz、最大 adapter_us 及最大 release_lateness_ms。
可先以 adapter <=1000 us、lateness <=10 ms 作工程試驗門檻（**非已核准產品規格**），
再依主迴圈負載/控制需求核定。不得事後放寬門檻來宣稱通過。

## 4. 匯出及分析

Halt 後 `set var g_product_fb_ch0_trace.capture=0`。
選 GDB console（非 UART console）；GDB 工作目錄須可寫，Windows 路徑用正斜線：

```gdb
pwd
source E:/myGitHub/myNXPs16_Prj/_Product/Tests/MCU/export_ch0_trace.gdb
```

script 使用 GDB logging/printf，不需要 GDB Python；產生 `ch0_trace.csv`。
若 IDE console 不接受 source，可在同一個 GDB session 載入命令檔；不要另開會 reset 目標的 session。
保留 CSV、ELF、map、probe 設定、測試紀錄與 git SHA，將每輪 CSV 另命名，避免覆蓋。
匯出期間保持 Halt，避免 64-bit average 或多欄位讀取不一致。

在 PowerShell 或 shell 執行（只需 Python 3 標準庫）：

```powershell
python _Product/Tests/MCU/analyze_ch0_trace.py ch0_functional.csv
python _Product/Tests/MCU/analyze_ch0_trace.py ch0_timing.csv --cpu-hz 100000000 --max-us 1000 --max-lateness-ms 10
```

100000000 只是例子，請換成該板當次實際 CPU Hz。
工具逐列重算五筆平均、去重、無效清窗、恢復、排程相位與漏列，報告最大間隔抖動、release 延遲、adapter 時間。
`functional=PASS` 需要一份完整 capture 覆蓋有效/五筆/重複/無效/恢復；
沒有全部刺激則 INCOMPLETE，正常穩定輸入時序輪可以不含所有功能刺激，不能當功能 FAIL。
時間資料/門檻不足時 timing=INCOMPLETE。exit code：0 完整通過、1 不符斷言或門檻、2 資料/覆蓋不足。
Host mock 也能產生相同數字；工具不認證 MCU 來源，須由實測紀錄及 ELF/板號證據確認。

## 5. PASS / FAIL 與結果表

| 項目 | PASS 條件 | MCU 結果 |
|---|---|---|
| CH0 真實有效 PV | 實際 CH0 Snapshot 有效，Trace 與當次輸入及工程尺度一致 | NOT RUN |
| 20 ms 排程 | 完整連續 CSV 相位及 elapsed 正確；最大 release 延遲符合事先門檻 | NOT RUN |
| 五筆與滑窗 | 上表 count/平均正確；最近五個已接受樣本平均誤差 <=1e-6 原始單位 | NOT RUN |
| 去重 | 同序號 callback 不增加 count、不改平均 | NOT RUN |
| 無效／恢復 | 無效 callback 立即 count=0/valid=0；恢復含相同序號從 count=1 起 | NOT RUN |
| 時間與抖動 | 3 輪正常及 3 輪最大負載，DWT 有效、全程無 Halt、各最大值均符合事先門檻 | NOT RUN |

任一功能錯誤、資料漏列或超門檻記 FAIL，附行號與原始資料；缺刺激/缺 CPU 時脈/缺計時器/受 debugger 暫停干擾記 INCOMPLETE，重測。
總結必須分開「功能」、「時序」、「真實感測器」，全部具備真實 MCU 證據後才能填 MCU PASS。

## 6. Host 支援驗證

```sh
python _Product/Tests/MCU/run_host_checks.py
```

會以嚴格 C99 編譯並執行 Trace 關閉/開啟兩種整合測試、Trace 滿停止/回繞/重設測試，
及 Python 分析器測試。需要 gcc、Python 3、已初始化的 PlatformCore 子模組。
這些都是 Host 結果，不能替代本文件 MCU 結果。
