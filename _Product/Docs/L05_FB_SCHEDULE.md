# L05 產品 FB 排程預留

## 狀態清單

- [x] 建立產品獨立的排程設定表，預留 18 個 FB 項目。
- [x] 建立主迴圈使用的固定記憶體週期排程器。
- [x] 驗證參數檢查、到期時序、延遲合併、相位保持、實際 dt、時間回繞及停用項目。
- [x] 以 CH0 有效工程 PV 建立 L05 callback adapter，整合 PlatformCore `FbMovingAverage`。
- [x] 以 `sample_sequence` 防止重複消費；無效資料清除平均視窗，恢復後重新累積。
- [x] 串接 ProductApplication 初始化及主迴圈排程呼叫。
- [x] 建立 CH0 L03 快照→L05 排程／adapter→L04 MovingAverage 的唯讀驗證鏈。
- [ ] 整合其他 L04 FB 與產品控制／輸出鏈。
- [ ] 量測 MCU 執行時間、最大延遲與產品整合驗證。

目前只啟用 MovingAverage 驗證項目，其餘 17 個排程項目仍停用。
測試視窗為 5 筆；週期 20 ms。輸入為 CH0 snapshot 的 `engineering_value`，
FB 結果只保留在 L05 記憶體中供驗證，不寫 PWM／DAC、不改變 SafetyService。
新增 L05 原始檔已納入 MCUXpresso source path；PlatformCore 的 Utility
include/source path 亦已加入 `.cproject`，供 `FbMovingAverage` 編譯及連結。

## 建議起始週期

| 類別 | 起始週期 | 說明 |
|---|---|---|
| PID、設定值斜率、程序、保證浸泡、輸出限制 | 20 ms | 同一控制鏈採一致時基及明確執行順序；依製程與新樣本率調整 |
| 數學、線性轉換、選擇、濾波、電壓補償 | 跟隨消費端控制週期 | 通常在控制鏈內直接呼叫，不必各自建立獨立任務 |
| 批次統計 | 20 ms | 收集每筆有效控制樣本；結束時再彙總 |
| 加熱器電阻 | 100 ms | 使用時間對齊且有效的電壓／電流資料；比較需考慮溫度 |
| 製程響應 | 100 ms | 適合慢速熱製程起始設定；高速製程需更密取樣 |
| 記錄觸發 | 100 ms | 一般趨勢起始值；快速暫態可改為 10–20 ms，事件不可只靠慢速輪詢 |

此值是產品規劃起始值，不是已驗證規格或 L04 固定常數。
目前 ProductAdcConfig.h 定義一般 ADC ODR=50 SPS（20 ms），poll=10 ms；
輪詢 10 ms 不代表每 10 ms 都有新 PV。RTD 使用不同濾波設定，需依實際新樣本間隔確認。
慢速爐體可評估 50–100 ms 控制；快速加熱才評估 10–20 ms，且須具備對應量測與執行能力。
取樣可先以製程最短重要時間常數的 1/10～1/20 作起點，再用閉迴路與延遲測試確認。

## 使用契約

產品提供 tasks[]、states[] 與 ProductFbSchedule_t；Init 完成後，主迴圈以
單調遞增的毫秒 now_ms 呼叫 Process。呼叫頻率決定排程解析度與最大釋放延遲。
目前尚未建立正式產品 task table 實例，設定巨集可展開成表供後續接入。

同一表內任務依表順序執行。多迴路可使用多個 context 或多張表，不將通道數寫死在排程器。
不同產品可使用自己的設定表與週期，不需改動 L04 方塊。

所有 task table 必須在使用期間保持不變；調整設定／啟用狀態後重新 Init。
callback 必須短小、不阻塞，不在 Tick ISR 呼叫，不使用動態記憶體。
晚到時僅呼叫一次，不補跑多次舊樣本；elapsed_ms 是實際距前次呼叫的時間，不是假設的標稱週期。
若 PID 設計只支援固定 dt，adapter 需另外限制時間抖動與處理超時，不可任意套用可變 dt。

新樣本／有效性判斷由 L05 adapter 負責。量測驅動的 FB 不應重複消費同一筆 PV；
純時間驅動的程序／計時則須依自己的契約處理。資料過期、超長 dt 和輸出安全處置另由產品定義。
安全保護不應等待製程診斷的慢速排程；此排程器不替代既有 SafetyService。

## 主機測試

2026-10-10：使用 C99、`-Wall -Wextra -Werror -pedantic` 編譯並執行
`_Product/Tests/Unit/test_product_fb_schedule.c`，全部 assert 通過。
2026-10-16：新增 `_Product/Tests/Integration/test_product_fb_schedule_integration.c`，
以相同嚴格 C99 選項編譯排程器、L05 adapter、整合層及 PlatformCore
`FbMovingAverage.c`，全部 assert 通過。測試用 mock 提供 L03 快照；這不是 MCU 實測。

重現方式（在 `myNXPs16_Prj` 根目錄執行）：

```sh
gcc -std=c99 -Wall -Wextra -Werror -pedantic \
  -I _Product/Tests/Integration/Mocks -I _Product/Config \
  -I _Product/Source/L05_Application -I _Product/Source/L03_System \
  -I _Modules/PlatformCore/L04_FunctionBlock/Utility \
  _Product/Tests/Integration/test_product_fb_schedule_integration.c \
  _Product/Source/L05_Application/product_fb_schedule.c \
  _Product/Source/L05_Application/product_fb_moving_average_adapter.c \
  _Product/Source/L05_Application/product_fb_schedule_integration.c \
  _Modules/PlatformCore/L04_FunctionBlock/Utility/FbMovingAverage.c \
  -lm -o /tmp/test_product_fb_schedule_integration
/tmp/test_product_fb_schedule_integration
```

### Integration 測項

| 測項 | 輸入／操作 | 預期結果 | 結果 |
|---|---|---|---|
| IT-01 排程未到期 | 初始化於 100 ms，處理 119 ms | callback 尚未執行 | PASS |
| IT-02 週期到期 | 處理 120 ms | callback 執行一次，`elapsed_ms=20`，納入一筆 PV | PASS |
| IT-03 重複樣本 | 保持同一 `sample_sequence` 再執行排程 | callback 可執行，FB 樣本數及平均值不變 | PASS |
| IT-04 新樣本 | 新序號且 PV=200 | 新增一筆，視窗平均正確 | PASS |
| IT-05 有效→無效 | `valid=false` | 清除舊視窗，輸出標成無效 | PASS |
| IT-06 持續無效 | 再次排程無效快照 | 視窗維持清空，不殘留舊 PV | PASS |
| IT-07 錯誤恢復 | 恢復有效；包含序號沿用情況 | 從一筆新視窗重新開始 | PASS |
| IT-08 延遲合併 | 220 ms 後直接處理 280 ms | 單次 callback，回報實際 `elapsed_ms=60`，不補跑 | PASS |
| IT-09 快照讀取失敗 | Provider 回傳 false 後再恢復 | 清窗後可重新納入有效資料 | PASS |
| IT-10 狀態重設 | 再次 Init 並於週期內處理 | callback 計數、週期相位及 FB 視窗重設 | PASS |
