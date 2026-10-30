# CH0 PID 固定 MV 串接測試

## 摘要

2026-10-30：已建立 CH0 Sensor Snapshot → L05 既有 Alarm／Logic 鏈內 PID callback → ProductPidControllerAdapter → L04 FbPid 固定 MV 測試後端 → L05 診斷查詢。Host 整合與 ASan／UBSan 通過；原 CKLIB 封裝 mock 測試、Alarm／Logic／PV 串接回歸通過。本輪尚未執行 MCU 編譯或目標板實測。

這是資料與呼叫鏈的測試，沒有 PID 算法，也不連結 CKLIB archive，不會自動呼叫 PWM／DAC／DO。實體 CKLIB 的架構相容性仍須另外解決。本版只接 CH0 原始溫度快照，不沿用 Alarm 的 RAW／PROCESSED 選擇，不修改原始量測值。

## 編譯與預設狀態

兩個巨集均預設為 0。需在整個測試組態的 C／C++ 編譯設定一致加入：

```text
FB_PID_TEST_BACKEND=1
PRODUCT_PID_CH0_TEST_ENABLED=1
```

第一個選擇 L04 固定 MV 後端，第二個註冊 CH0 L05 測試鏈。不能只在單一 `.c` 定義，因為 FbPid_t 的測試欄位依編譯巨集決定；修改設定後須 Clean／Rebuild。僅啟用 L05 而未選擇 L04 測試後端會編譯失敗。此版沒有新增專用 MCUXpresso 組態，也沒有改動既有 Debug／Release／SafetyTest 的巨集。

測試後端只接受 PID＋HEAT、AT target=PID 的設定；不模擬 AUTO_TUNE／ON_OFF／PID_FB，亦拒絕一般 SetParameter，避免把固定 MV 誤認為算法或整定結果。預設固定 MV=0、enable=false；不因編譯測試後端就開始發出有效 MV。

## 呼叫位置與週期

沿用目前 20ms Alarm／Logic callback 的順序：Alarm → 前半 Logic → **CH0 PID adapter** → 後半 Logic。沒有新增 PID 排程項目，也不在其他位置重複呼叫 adapter。每次接收該 callback 提供的實際 elapsed_ms。

PV 來源為 CH0 的原始 Sensor Snapshot。L05 檢查溫度型別、valid、input_error、conversion_flags、處理時間戳有效性及 200ms 資料年齡；來源取得失敗不補假 PV。原始時間戳與序號共同保留，重複發布不能刷新有效期限。此時間戳沿用「樣本首次處理時間」，不是 ADC 精確擷取時間。

PV／SV 公開刻度均為 0.001°C；固定 MV 使用 0.1%。測試後端不使用 PV／SV 進行數值控制，因此改變 PV／SV 不會改變固定 MV；資料品質仍決定輸出是否有效。

## 設定與查詢 API

所有 API 在主迴圈中序列呼叫，不從 ISR 或多個執行者同時呼叫。

| API | 用途與限制 |
|---|---|
| `ConfigureCh0PidTest(enable, sv, mv)` | 設定測試 SV、固定 MV 及開關；完整名稱為 `ProductFbScheduleIntegration_ConfigureCh0PidTest`。SV 範圍 -200000～3000000（暫沿用產品溫度範圍），MV 0～1000；非法設定不變更現況。有效設定立即清空舊輸出，下一排程才運算 |
| `ResetCh0PidTest()` | 完整名稱同前綴；立即清空舊輸出，並在下一排程送出一次 RESET。該次不產生有效 MV，後續有效週期恢復固定值 |
| `GetCh0PidTest(&diagnostics)` | 完整名稱同前綴；取得最後 provider 輸入、adapter 診斷及 `fixed_mv_test_backend=true`。輸入是最後一次取樣的診斷，不是 setter 的立即回讀 |

初始化 `ProductFbScheduleIntegration_Init()` 後、主迴圈開始執行前，可由測試入口加入：

```c
#if PRODUCT_PID_CH0_TEST_ENABLED
/* SV=30.000 C; fixed MV=10.0%; no physical output in this test chain. */
if (!ProductFbScheduleIntegration_ConfigureCh0PidTest(true, 30000, 100)) {
    /* 交由既有應用程式錯誤處理；不要忽略設定失敗。 */
}
#endif
```

上例為測試入口的使用方法，未自動插入產品啟動流程。產品既有排程初始化仍會把測試 enable 設為 false；重複 Init 也會重新停用並清除歷史。

透過 `GetCh0PidTest()` 觀察 `input.pv`／`input.sv`、`controller.last_elapsed_ms`、`callback_count`、`output.mv[0]`、`mv_count`、`output.valid` 及 `last_status`。有效運算時 MV1=100、mv_count=1、valid=true；MV2=0、auto_tune_completed=false。

## Host 測試摘要與細則

在專案根目錄執行（需要 C99 編譯器；CC 環境變數可指定）：

```text
python _Product/Tests/run_ch0_pid_test.py
python _Product/Tests/run_ch0_pid_test.py --sanitize
```

腳本只對這次編譯加入測試巨集，不修改 IDE 組態。測試以可控制的 Sensor Snapshot 替身驗證完整資料鏈；MCU 才會使用真正 Sensor 服務。測試程式為 `_Product/Tests/Integration/test_product_ch0_pid_test.c`。

| 測項 | 預期結果／本次結果 |
|---|---|
| 未初始化與預設停用 | 設定被拒絕或輸出無效；通過 |
| PV／SV 刻度與固定 MV | 25123／30000 原值傳遞，MV=100；通過 |
| 週期與重複排程 | 20ms 呼叫一次，41ms 不額外執行；通過 |
| MV 合法範圍 | 0 與 1000 可用，1001 拒絕並保留現況；通過 |
| 設定變更／停用 | 立即清空舊有效輸出，按下一排程更新；通過 |
| RESET | 下一排程輸出無效，之後恢復；通過 |
| 來源取得失敗／無效與恢復 | 無效時不產生有效 MV，恢復後可運算；通過 |
| Sensor 型別／錯誤／轉換旗標 | 非溫度或錯誤資料不能通過；通過 |
| 時間戳無效與資料過期 | age=200ms 接受，220ms 拒絕；通過 |
| 重新初始化 | 停用測試、清除舊狀態；通過 |
| uint32 時間回繞 | 回繞後的小時間差仍正確執行；通過 |
| 後端模式／數值參數 | 不支援 AT 或 PID 參數，明確拒絕；通過 |

本次以 C99、Wall／Wextra／Werror／pedantic 與 ASan／UBSan 執行；LeakSanitizer 使用 detect_leaks=0，不宣稱已做漏記憶體檢查。測試後端無 heap 配置。

## 後續階段

1. MCU 測試組態編譯、CH0 實際量測與 SWD 觀察 MV；確認週期、有效性與恢復。
2. 接入產品輸出限制、既有 SYSTEM／Safety／輸出服務，再測 PWM CH0。不能直接把此測試 MV 繞過既有輸出權責寫到硬體。
3. 取得相容 CKLIB，移除測試組態巨集，沿用 L05 provider／callback 並完成參數映射與真正閉迴路測試。固定 MV 通過不等同 PID 控制效果已驗證。
