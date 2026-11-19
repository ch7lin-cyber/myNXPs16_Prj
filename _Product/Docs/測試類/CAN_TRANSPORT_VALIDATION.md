# CAN0 Transport 回歸與 MCU 驗證

本次範圍：Classic CAN Transport、自動化 Host 測試、獨立 bench 診斷。
CANopen、PDO 分配及既有 Safety 決策不變。

## 驗證狀態（2026-11-03）

- Host：`python3 _Product/Tests/run_can_tests.py` 通過。
- 真實 SDK header/device header：driver 的產品、bench normal、internal
  loopback 組態通過 host GCC syntax-only 檢查；這不是 ARM firmware link。
- MCU 編譯／燒錄、單板 loopback、兩節點 ACK、Bus-off 恢復：**尚未實測**。
- GitHub Actions 設有獨立 `CAN transport host regression` 工作流程；不需要
  PlatformCore 子模組。CI 實際結果以該 commit 的 Actions 為準。

## Host 執行與 Mock 邊界

需求：Python 3、GCC、binutils（nm）。從 repo 根目錄：

```sh
python3 _Product/Tests/run_can_tests.py
```

執行真實 `product_can_driver.c`，由 include path 替換 `fsl_mcan.h` 與 SDK
函式實作，不複製 driver 演算法。RAM shim 只把 Message RAM 指向 Host 陣列；
不改產品 RAM 位址。啟用 `-Wall -Wextra -Werror` 及 UBSan。測試不使用 NDEBUG。

三種組態為 `(TEST_BUILD, INTERNAL_LOOPBACK)=(0,0),(1,0),(1,1)`。
另驗證 `(0,1)` 編譯必須失敗，且產品 binary 沒有診斷入口／watch symbol。

| 測試 | 預期 |
|---|---|
| Init 前 API、timing／Message RAM 設定失敗 | 拒絕操作；RAM 設定失敗 deinit；重新 init 清除狀態 |
| 11-bit ID、DLC | TX ID 0/7FFh、長度 0..8 成功；800h/FFFFh、9/255、NULL 拒絕並計數 |
| RX 非法格式 | XTD/RTR/FDF/BRS 或 DLC 9..15 拒絕，不截短為合法封包 |
| TX queue | 8 筆成功、第 9 筆失敗並計數；3 輪 FIFO／回繞；Send 後來源修改不影響已複製資料 |
| TX SDK 失敗／busy | write 失敗不丟 queue head；pending 時不重送；completion W1C 只算一次 |
| RX queue | 16 筆保留、第 17 筆丟棄並計數；3 輪 FIFO／回繞；Read 失敗不無限迴圈 |
| 診斷 | TEC=200/REC=100、Warning/Passive 優先序、Protocol Error、sticky FIFO Loss、W1C 不重複計數 |
| Bus-off | 8 筆排隊不提交；滿載有界；解除 BO 後依序續送；已 pending 的硬體請求不重複提交 |
| BO interrupt | 進入／離開皆可能出現；bus_off_events 只計 PSR 的進入邊緣，recoveries 計離開邊緣 |
| bench 入口 | 正確資料匹配、responder 回覆、錯誤資料 FAIL、5 s timeout、毫秒 wrap、明確 recovery／stop |

Mock 模擬 W1C、借用的 RX RAM 指標、可注入 read/write 失敗、獨立的 pending
及 completion。它不模擬 arbitration、ACK、bit timing、transceiver、硬體 FIFO
寫入競爭或真實 Bus-off 恢復。解除 mock BO 只證明軟體續送，不證明 MCU 恢復。

## 組態與入口

| 用途 | 全專案 C preprocessor symbols |
|---|---|
| 一般產品 | 不定義兩項（皆預設 0） |
| 實體兩節點 bench | `PRODUCT_CAN_TEST_BUILD=1`、`PRODUCT_CAN_INTERNAL_LOOPBACK=0` |
| 單板內部自測 | `PRODUCT_CAN_TEST_BUILD=1`、`PRODUCT_CAN_INTERNAL_LOOPBACK=1` |

在 MCUXpresso 建立獨立的 CAN bench build configuration，於 C compiler 的
preprocessor 設定加入上述 symbols，Clean + Build；不要只設定單一 source。
沿用原 board pin mux、15 MHz MCAN clock、SRAMX 前 4 KiB 保留及 linker。
`product_can_test.h` 與 driver 在同目錄，診斷實作與 driver 同一 translation
unit，application foreground 已在 driver Process 後呼叫診斷 Process。
一般產品不產生測試入口或測試收發行為；Loopback 預設關閉。

SDK `MCAN_Init()` 在 internal loopback 設定 CCCR.TEST/MON、TEST.LBCK，
並沿用 Classic CAN，external loopback 始終關閉。現有 SDK 支援此設定；
MCU 實際能否成功收發仍由以下測試判定。

Watch 加入 `g_can_test_watch`。啟動前暫停 CPU，先寫 `requested_frames`，
最後寫 `command`，再 Resume；避免 CPU 與 debugger 同時寫多個欄位。

| 欄位／命令 | 意義 |
|---|---|
| command=1 | 開始 internal loopback；只有 loopback build 接受 |
| command=2 | normal build requester，TX 321h，期待 RX 322h |
| command=3 | normal build responder，RX 321h，回 TX 322h |
| command=4 | 若最新 PSR 為 BO，明確清 INIT 啟動 MCAN recovery sequence；不保證恢復成功 |
| command=5 | 停止診斷判定；不清空 transport queue 或取消硬體 TX；重跑請 reset |
| state | 0=idle、1=running、2=資料匹配與 TX completion 達標、3=FAIL |
| requested_frames | 1..100000；0 視為 1 |
| matched／submitted | 完整匹配 RX 數／成功排入 TX 數 |
| unexpected／timeouts | 非預期封包／5 秒無進展；任一會 FAIL |
| driver | 每輪更新的 ProductCanDiagnostics_t 快照，idle／PASS 後仍更新 |

獨立 bench 模式由診斷器獨占 RX consumer；不要讓其他程式同時取 RX 或送 TX。
所有 CAN API 限單一 foreground context，不供 ISR 或並行呼叫。
開始前須無 pending TX；每次驗證 reset，使 counters、queue、序號從零開始。
在 active run 再送 start 或不相容 command 會 FAIL；診斷不會自動重新 init。

封包固定 DLC=8。前 4 bytes 是從 0 遞增的 little-endian uint32 sequence，
後 4 bytes 為 `A5 5A 00 FF`：

| 序號 | 321h 請求與 322h 回覆的相同 DATA |
|---|---|
| 0 | `00 00 00 00 A5 5A 00 FF` |
| 1 | `01 00 00 00 A5 5A 00 FF` |
| 999 | `E7 03 00 00 A5 5A 00 FF` |

## 單板 internal loopback

1. 建置 loopback bench，燒錄後 reset。CAN 線先離線；內部 loopback 不依賴
   外部節點、ACK 或終端電阻。Safety 與輸出行為仍依原產品程式，保持原 bench 安排。
2. 寫 requested_frames=1、command=1，Resume。5 秒內應 state=2、matched=1、
   submitted=1、driver.tx_queued=1、tx_completed=1、rx_received=1。
3. reset，改 requested_frames=1000 重複。預期 matched/submitted、tx_queued、
   tx_completed、rx_received 均為 1000；tx_queue_pending=0、tx_in_flight=false。
4. 所有 queue full/drop、invalid、protocol error、warning/passive/BO/FIFO loss
   事件為 0，TEC/REC=0、unexpected/timeouts=0 才算 PASS。
5. state=3、timeout、資料不符、counter 不符均 FAIL，保存 watch、CCCR/TEST/PSR/ECR
   與 clock 設定。Loopback PASS 只證明控制器／Message RAM／軟體路徑；不證明
   pin mux、實體 transceiver、外部 ACK 或線路正常。

## 兩節點接線與基本 ACK／接收

1. 兩板皆 normal bench build（loopback=0），同為 500 kbit/s；確認兩板實際 MCAN
   source clock 與 bit timing，transceiver 電壓相容且已離開 standby。CANH 接 CANH、
   CANL 接 CANL，依 transceiver 要求接共同參考地，採線型 bus、短支線。
2. bus 兩端各 120 Ω，不是每節點都加；全部斷電後量 CANH–CANL 約 60 Ω。
   先確認板上是否已有終端電阻。上電後再測通訊；使用非 silent 的接收節點。
3. reset 兩板。B 先 command=3，5 秒內 A command=2，兩者 requested_frames=1。
   若操作不及 5 秒，可同時 Resume 已暫停並設好 command 的兩板。
4. 分開記錄證據：A TX 321h 完成且 B RX 完整資料；B TX 322h 完成且 A RX
   完整資料。兩板各 tx_completed=1、rx_received=1、matched=1、state=2。
5. **實際 ACK**：使用 CAN analyzer／示波器觀察每方向 ACK slot；analyzer 設
   listen-only，不能由 analyzer 代替 peer ACK。只接 A+B 時 normal TX completion
   是成功傳送的控制器證據，接收資料與 ACK slot 是另外兩項證據，全部保存。
6. 所有錯誤、丟包、重複／序號錯誤為 0，TEC/REC=0 才 PASS。
   Send=true 只表示排隊，不能當作 ACK 或接收成功。

ACK 負向案例：reset A+B，B 斷電或 silent，analyzer 也 listen-only，A command=2。
預期不能完成往返、5 秒後 FAIL，無 RX 回覆，通常 LEC=3、TEC／錯誤事件上升。
記錄重傳與 error passive；只移除 ACK **不保證進入 Bus-off**（error passive
下的 ACK error 有特殊計數規則）。重新啟動 B 後 reset 雙方再做正向案例。

## Bus-off 與錯誤恢復（獨立案例）

1. 使用具 CAN error injection 的設備／可控錯誤注入台，按設備指引產生會增加
   TEC 的錯誤；不要靠直接短接供電或 CAN 線來製造故障。記錄實際 PSR.BO、
   TEC/REC、LEC、BO entry 次數。未觀察到 BO 就標示「未驗證」，不能 PASS。
2. requester 運行時進入 BO，確認 controller_state=BUS_OFF、bus_off=true；
   軟體 pending 在後續 Process 仍保留且不新增硬體 TX request。
   必要時暫停診斷（command=5），從 foreground 測試程式呼叫 Send 排入已知序號
   封包；不能從 ISR 或 debugger 在 driver 執行中重入。最多 8 筆，第 9 筆拒絕。
   已在硬體 pending 的封包另計，不能混同軟體 queue。
3. 移除錯誤源，確保另一正常節點／線路可 ACK。command=4 明確要求開始恢復；
   此命令清 INIT，後續恢復由硬體 bus idle sequence 決定。**一般產品沒有新增
   自動 recovery 策略**。保持 foreground Process 運行，記錄 PSR.BO 解除時間。
4. BO 解除後 bus_off_recoveries 增加 1、bus_off_events 不因 exit 再增加。
   確認軟體 queue 依 FIFO 送出、無重複提交、另一節點完整接收到預期序列，
   最後 pending=0、in_flight=false。已 pending 的硬體 TX 是否保留、重傳或取消
   以 MCU 寄存器與 trace 實測記錄，不能從 Host mock 推定。
5. 若軟體保留 queue 但 MCU request 無法完成，或必須 reset/init 才恢復，記錄
   為本案例 FAIL／待調查，不能用清 queue 的 Init 來宣稱恢復通過。
6. reset 後重新做 1000 筆正常往返；回復後數據可靠性也必須通過。

## 可靠性與判定紀錄

雙方 requested_frames=1000，B command=3、A command=2。每次只一筆往返，
完整比對 ID、DLC、全部 8 bytes、遞增序號。預期兩板各 TX queued/completed、
RX received、matched/submitted=1000；unexpected/timeouts=0、pending=0、無 in-flight。
queue full/drop、invalid、FIFO loss、protocol/warning/passive/BO 事件全部為 0。
連做 10 輪（每輪 reset），保存 elapsed time、counter 快照與 analyzer trace。
此為低負載基本可靠性，不代表已驗證 450 PDO 排程或滿 bus 負載。

| 個別測項 | 結果（待填） | 必須保存的證據 |
|---|---|---|
| 單板 1／1000 筆 | NOT RUN | build symbols、watch、clock、CCCR/TEST |
| A→B 實際 ACK | NOT RUN | listen-only trace ACK slot、A TX completion |
| B 接收 | NOT RUN | B 321h payload／counter |
| B→A ACK 與 A 接收 | NOT RUN | ACK slot、322h payload／counter |
| 無 ACK 負向案例 | NOT RUN | timeout、LEC/TEC、error trace |
| Error Passive | NOT RUN | PSR.EP、TEC/REC、event counter |
| 真實 BO 與恢復 | NOT RUN | entry/exit PSR、INIT request、時間、queue／peer trace |
| 1000×10 往返 | NOT RUN | 雙方各輪 snapshot、trace、錯誤／重複／丟包數 |

state=2 只是內建資料與 TX completion 條件達標；仍須符合各案例的錯誤計數、
ACK 與實體證據才可將該案例標 PASS。未執行、缺少證據與 FAIL 分開記錄。
