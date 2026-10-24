# DTDM04 Ramp Soak Parameters 對照

## 摘要

實作增補：L04 FbProfileSequencer 已由佔位改為程序核心，完成時間式Ramp／Soak、循環／連接、命令、到溫等待／逾時及工作量保護，Host／ASan／UBSan通過。採本專案首段／零時間／等待契約，非完整DTDM04相容結論；以下未實作敘述為原盤點背景。詳見 [正式規格與方塊圖](../../_Modules/PlatformCore/L04_FunctionBlock/Control/FbProfileSequencer/README.md)。L05、Register／NVM持久化與MCU仍待接入。

2026-10-22：依使用者提供的 Ramp Soak Parameters 表，配合 DTDM04 v1.5 第 5.1.4 節（頁 5-5～5-8）整理程序資料、狀態與分層。已知四個 Pattern（0～3）、每個最多十六個 Step（0～15），各步驟有目標值及時間；樣式另外設定有效步驟、重複次數及下一個樣式／結束方式。

**FbProfileSequencer 仍為空架構，尚未實作。** FbSetpointRamp、FbInBandTimer 是可重用元件，不代表完整程序功能已完成。本次只更新規格與待辦，不新增程式或 Register。

以本專案架構為主：程序 FB 產生動態 SV、程序狀態與結束原因；是否停止控制及實體輸出，交 L05 與既有 SYSTEM／Safety／輸出流程處理。

## 原表資料配置

位址依原表視為十六進位；Table Index 是另一個索引，不是 Modbus 位址。這次表頭標為 Pattern0～3，不是 CH1～4，不能把四個 Pattern 直接當成四個通道；每通道是否各有獨立程序表／銀行，仍需由實際資料模型或工具畫面確認。

| 欄位 | Pattern p（0～3）的原位址 | 原表設定 |
|---|---|---|
| Step s 的 SV（s=0～15） | 0x1600＋8×s＋p | S16，−32000～32000，預設 0；溫度 0.1°C／°F，類比量無單位。 |
| Step s 的時間 | 0x1604＋8×s＋p | U16，0～900，預設 0；單位分或秒。 |
| Actual Steps | 0x1680＋p | U8，0～15，預設 0；需按最後步驟編號語意解析，不能直接當作 count。 |
| Loop Times | 0x1684＋p | U8，0～99，預設 0。手冊為額外重複次數，0 表示原執行一次。 |
| Linking Pattern | 0x1688＋p | 0～3 接下一個 Pattern；4=END；5=STOP；預設 4。 |
| Reserved | 0x168C 起 | 不新增推測功能；不直接當作額外程序資料。 |

「有效步驟」名稱看似數量，但值域 0～15 對應十六個 Step，手冊範例也描述執行至指定步驟。建議協定層使用 last_step_index，轉成 L04 step_count=last_step_index+1；在完整相容性驗證前保留該命名差異，避免 0 被誤認成空程序。

## 已取得的關聯 Control Loop 參數

| 來源欄位 | 用途／責任 |
|---|---|
| Auto Control Mode=Ramp Soak PID | L05 選程序動態 SV 作為控制目標，不改變 PID 的基本追蹤責任。 |
| 時間單位 0=Min、1=Sec | 由 L05／參數轉換層換成固定內部單位；不讓運算依賴 UI 顯示格式。 |
| Starting Pattern／Starting Step | 程序啟動位置；需驗證 Pattern 存在且 Step 在有效範圍內。 |
| Starting Slope | 處理首步／零時間等啟動規則；原手冊與專案語意需明訂。 |
| Waiting Temperature／Waiting Time | 步驟結束的到溫帶等待與逾時，不等同固定 Soak 時間。 |
| Resume Ramp Soak Status | L05／NVM 保存與恢復程序狀態，非 FB 自行讀 FRAM。 |

來源對照見 [Control Loop 文件](DTDM04_CONTROL_LOOP_MAPPING.md)。

## Ramp／Soak／Waiting 的差別

- Ramp：步驟目標與前一設定點不同，在設定時間內產生移動的 SV。
- Soak：相鄰設定點相同，在該時間內維持 SV。
- Waiting：步驟銜接時檢查實際 PV 是否落在目標±等候溫度內；未到溫則等待，逾時產生狀態／警報請求。

手冊另有首個執行步驟的特殊敘述：先控制至目標再保持，且整體時間為 T；首步時間為 0 又涉及起始斜率。不能概括成「所有 Soak 都是 PV 到溫後才開始完整持溫 T」，也不能在未定案前一律把 T=0 視為跳步。

程序一般段可使用時間內插求動態 SV；FbSetpointRamp 則是每秒斜率的運算元件。即使使用 Ramp FB，也須依段起點／目標／時間轉換，不能直接把段時間當成 rise_rate。FbInBandTimer 的帶內累積／暫停政策與等待倒數不同，也不能直接取代程序等待狀態機。

## END 與 STOP 的區別

| Linking 值 | 原手冊概念 | 在本專案的處理方向 |
|---|---|---|
| 0～3 | 連接指定 Pattern | 程序 FB 切換樣式並處理重複／剩餘時間。 |
| 4 END | 程序結束，維持最後設定值 | 程序發布 completed／最後 SV；是否繼續控制由 L05／既有狀態決定。 |
| 5 STOP | 程序終止並關閉控制輸出 | 程序發布 stop_requested／結束原因，由既有控制／輸出權責受理；L04 不寫 GPIO 或繞過 Safety。 |

循環重複須完成後才連接下一 Pattern。相互連接或自連可形成長時間甚至無限程序；實作須限制一次 Execute 的狀態轉移次數，避免零時間循環阻塞主迴圈，並定義錯誤／續執政策。

## 建議介面責任（尚未定版或實作）

| 類別 | 內容 |
|---|---|
| Config | 外部唯讀 Pattern／Step 表、起始位置、首步政策、等待帶與逾時設定；容量由產品決定。 |
| Input | 經過時間、有效 PV、Start／Hold／Resume／Reset 等命令與必要的控制允許狀態。 |
| State | 現在 Pattern／Step、段起始 SV、段已用時間、重複計數、等待狀態與恢復資訊。 |
| Output | 動態 SV、目標 SV、目前段／樣式、剩餘時間、運行／保持／等待／結束狀態及 stop／timeout 請求。 |

一個實例代表一條程序執行狀態，不綁定硬體通道。L05 可將程序 SV 接到單迴路 PID，或 Cascade 外迴路；是否支援共用程序表、多迴路同步須由產品另訂。每個實例的執行狀態仍獨立。

## 缺項與待確認

- [ ] 程序表歸屬：四個 Pattern 是共用配方，還是各通道獨立資料銀行。
- [ ] 有效步驟的索引／count 遷移規則與啟動位置驗證。
- [ ] 首步起點取 PV、固定初值或前次 SV；首步 T=0 與非首步 T=0 的規則。
- [ ] Hold／Resume 與 Stop／Start 的差別，ENABLE 停用的凍結／清除政策。
- [ ] 跳步、等待逾時、無效 PV／過期資料時如何處理與恢復。
- [ ] Waiting Temperature 的有號語意及 Waiting Time=0 行為，不直接猜成無限等待或不等待。
- [ ] END／STOP 後的重新啟動與最後 SV 保留政策。
- [ ] 超長 dt 跨步／跨 Pattern 的剩餘時間處理，以及單次執行工作量上限。
- [ ] 斷電保存的版本、表格一致性、目前階段及剩餘時間；恢復程序不等於自動恢復加熱許可。
- [ ] 完成 L04 狀態機、Host 邊界測試、L05 接線、Register／NVM 與 MCU 驗證。

原表 0.1°C 目標轉成專案 0.001°C 時須使用足夠位寬；完整原表範圍換算為 −3200000～3200000，但實際目標還要受所選來源／產品 SV 範圍限制。原表 900 分＝54000000 ms 可裝入 uint32，累計整條多次循環程序仍需另作溢位設計。

此次資料已足夠建立程序 FB 的需求草案，但以上狀態契約仍需定義；不把表格接收完成標成程式完成。
