# Function Block 與 DTDM04 功能盤點

## 摘要

L05增補：PV處理adapter已接正式Sensor Snapshot provider、20ms CH0排程與Alarm的RAW／PROCESSED明確來源選擇，涵蓋單位／時效／時間差、整組設定與來源變更歷史清除，Host整合與回歸通過。預設Alarm仍RAW；PID、Register／NVM、實體輸出及MCU仍未完成，不宣稱正式閉迴路上線。時間戳是樣本首次處理時間，非ADC精確擷取時間。

更新日期：2026-10-25；原始盤點為2026-10-22。此為本機原始碼／契約與Host驗證狀態，不代表完整設備相容或MCU實測。

**正式PID callback、Register／NVM、實體輸出與MCU實測仍待接入。**

目前25個具有.c／.h的運算模組：
- 24個已有專案核心與Host測試；其中本輪新增／補齊的12個核心皆以cc與ASan／UBSan驗證，原內插核心59項回歸亦通過，未重跑全部歷史測試。
- 1個FbPid已有CKLIB封裝與mock測試；實體.a仍為空檔，後端數值效果未驗證。
- 原七個佔位已補核心，不能再以「七個尚未實作」描述目前狀態。
- FbSignalLinearization提供輸入／輸出兩入口，計為一個共用模組；另有僅文件規劃的FbC7gLogic，合計26項。

本輪完成：輸入／輸出折線包裝、Switch Over、跨通道警報、PID候選選組、Ramp／Soak核心、串級SV換算、前饋、批次統計、響應觀測、記錄觸發、阻值估算及電壓補償。核心完成僅代表各自公開契約；原表未明定的行為採明確專案定義，不默認DTDM04相容。

主要剩餘缺口：L05資料路由／排程、CKLIB刻度／時間能力、整組參數套用與AT回寫、串級AT／校準／anti-windup／副控保護、Model Output公式與用途、產品Register／NVM／輸出及MCU。C7G Logic仍獨立規劃。本輪核心與文件納入Git提交保存；功能完成度仍以公開契約與未完成項目區分。

## 判定原則

以本專案架構為主。DTDM04 提供功能概念，不搬入其輸出控制權與例外優先順序。L04 只做獨立運算；L05 選來源、配置實例並安排運算；SYSTEM／Safety／既有輸出服務保留最終權責。Alarm 或程序完成狀態不得直接繞過 Safety 操作硬體。

單一 FB 有多個輸入不等於綁定多個產品通道，例如 Switch Over 可以是一個接收兩個 PV 的獨立實例。組數、來源模組與硬體數量由產品設定。

狀態資料庫、Register、NVM、事件動作分派與主／擴充模組通訊不是單一 L04 演算法的完成條件，應另外追蹤。

## 現有全部運算 FB

| FB | 程式狀態 | DTDM04 對照及限制 |
|---|---|---|
| FbAlarm | 已實作／Host 驗證 | 4.2.1～4.2.2；八種比較、成立延遲、待機、峰值。MANUAL／Reset／Peak Clear 為專案契約，與原 HOLD／STOP 不完全相同。L05 已有 12 組配置。 |
| FbLogic | 已實作／Host 驗證 | 5.2.4；四輸入、反向、ON／OFF 延遲。OFF 強制零為專案契約。L05 已有 16 組與前／後半組順序。 |
| FbSetpointRamp | 已實作／Host 驗證 | 可作 5.1.5 斜率控制的基礎。不是完整程序執行器；現有零斜率為保持，須另確認產品如何解讀手冊設定。 |
| FbInBandTimer | 已實作／Host 驗證 | 可作到溫帶與持溫計時的基礎；不能直接等同 5.1.4 的步驟結束等候／逾時警報。 |
| FbOutputLimiter | 已實作／Host 驗證 | 限幅與變化率限制的通用元件；不取代 SYSTEM 最終輸出許可。 |
| FbWeightedSum | 已實作／Host 驗證 | 可用於 PID MV＋FF MV 等加總，但本身沒有前饋時序。 |
| FbDivide | 已實作／Host 驗證 | 通用數學運算，非 DTDM04 專用功能。 |
| FbSelector | 已實作／Host 驗證 | 四輸入索引／最小／最大選擇；沒有 Switch Over 的切換帶與故障替代狀態。 |
| FbMovingAverage | 已實作／Host 驗證 | 移動平均；已有 CH0 L05 驗證鏈。不能視為等同手冊的濾波演算法。 |
| FbLinearization | 已實作／Host 驗證 | 分段線性插值，X 嚴格遞增，範圍外 CLAMP／ERROR；尚非完整 DTDM04 輸入／輸出折線功能。 |
| FbLeadLag | 已實作／Host 驗證 | 通用超前／落後元件；不能因有此元件就認定前饋或串級完成。 |
| FbDeadTime | 已實作／Host 驗證 | 通用純延遲，不等同布林 ON／OFF 延遲或程序等待功能。 |
| FbPid | 封裝已實作；實體驗證未完成 | AUTO_TUNE／ON_OFF／PID／PID_FB 四模式，CKLIB 引用位於 L04；有產品 adapter mock 測試。不是空架構，也不能宣稱實體演算法已驗證。 |
| FbProfileSequencer | 程序核心已實作／Host驗證 | 專案Ramp／Soak、循環／連接、等待／命令已做；L05、持久化恢復及MCU待接。 |
| FbCascadeCoordinator | SV換算核心已實作／Host驗證 | 主控MV→副控SV映射、界限、Offset及濾波已做；完整雙PID協調待接。 |
| FbPowerVoltageCompensation | L04平方補償已實作／Host驗證 | 專案電阻負載模型，產品輸出鏈及anti-windup未接。 |
| FbHeaterResistance | L04阻值診斷已實作／Host驗證 | 專案診斷，非DTDM04缺項；RMS來源／時間窗與產品處置待接。 |
| FbBatchMetrics | L04統計核心已實作／Host驗證 | 專案批次統計；產品事件／來源／保存／MCU未接。 |
| FbProcessResponse | 已知步階觀測核心已實作／Host驗證 | 專案製程分析，非自動辨識／整定；產品實驗與資料串接待接。 |
| FbRecordTrigger | L04觸發核心已實作／Host驗證 | 週期／事件請求，非資料記錄服務；L05／實際擷取保存／MCU未接。 |
| FbC7gLogic | 僅獨立規劃文件 | C7G 功能，與 DTDM04 FbLogic 分開。 |
| FbSignalLinearization | L04包裝已實作／Host驗證 | 輸入Value／Deviation／Bypass、輸出內插／步階／Off、Offset及限制；來源與Register未接。 |
| FbInputSwitchOver | L04已實作／Host驗證 | 雙來源切換／平均及故障政策；L05品質／時序與產品來源未接。 |
| FbMultiChannelAlarm | L04已實作／Host驗證 | MAX−MIN、延遲與單次動作；SYSTEM目標／命令回報未接。 |
| FbPidGroupSelector | L04候選選組已實作／Host驗證 | 四組參數驗證與SV選組；不是後端已套用ACK。 |
| FbFeedforward | L04波形核心已實作／Host驗證 | 延遲／兩段補償／回零與Force；FF＋PID、熱冷及追蹤未接。 |

以上不將 AlarmConfigurationEventConsumer、SafetyConfigurationEventConsumer、FunctionBlockLifecycle 或 CKLIB 標頭計為獨立運算 FB。

## 12 類 Register 群組的功能缺口

| 群組 | 手冊依據 | 現況 | 缺項及放置位置 |
|---|---|---|---|
| 輸入折線表 | 3.3.5，頁 3-9～3-11；使用者已提供 Input Module 與折線欄位 | 專案折線包裝已實作／Host驗證 | Value／Deviation、Offset、旁路已做，原Low／High端點語意待確認；14 組中間點與邊界是否構成 16 節點待確認。詳見 [輸入折線對照](DTDM04_INPUT_LINEARIZATION_MAPPING.md)。 |
| Ramp Soak Parameters | 5.1.4，頁 5-5～5-8；使用者已提供四樣式／十六步驟欄位 | 程序核心已實作／Host驗證 | 已整理有效步驟索引、額外循環、END／STOP、首步與等待差異；專案狀態契約已定；產品配方歸屬、L05與斷電保存仍未完成。詳見 [程序對照](DTDM04_RAMP_SOAK_MAPPING.md)。 |
| Control Loop Parameters | 使用者已提供完整群組欄位，跨第 3、5 章 | 有 PID 封裝及部分基礎元件 | 已拆分 PV 路由、單／串級、控制演算法、雙 SV、程序、輸出候選值與 NVM 恢復責任；詳見 [逐欄對照](DTDM04_CONTROL_LOOP_MAPPING.md)。Deadband／等待溫度／斜率時間單位仍有疑點。 |
| Input Switch Over Parameters | 3.3.4，頁 3-8～3-9；使用者已提供欄位 | 專用Switch Over已實作／Host驗證 | 手冊確認低／高來源選擇及中間區兩來源平均，不等同一般遲滯。專案故障／恢復／等號契約已定；L05來源／時效未接；詳見 [輸入切換對照](DTDM04_SWITCH_OVER_MAPPING.md)。 |
| Logic Operator Parameters | 5.2.4，頁 5-25～5-26 | L04 與基本 L05 串接已做 | 現有來源為常數／Alarm／Logic／DI，尚未完整納入手冊所有程序／控制狀態；Register 及 MCU 驗證另列。 |
| Alarm Parameters | 4.2.1～4.2.2，頁 4-9～4-10 | L04 與 12 組 L05 已做 | Register、NVM、事件與 DO 分派待整合；MANUAL／STOP 差異是專案選擇，不以改回手冊語意作為補缺。 |
| Multi-channel Alarm Parameters | 4.2.3，頁 4-11；使用者已提供 mAlm1 欄位 | 跨通道比較核心已實作／Host驗證 | 已確認 8×4 選擇 mask、MAX−MIN、0～10 秒動作延遲及 Hold 一次動作；專案重置／重新武裝已定；跨模組品質provider與SYSTEM動作目標待接。詳見 [多通道警報對照](DTDM04_MULTI_CHANNEL_ALARM_MAPPING.md)。 |
| Feedforward | 5.1.8，頁 5-11～5-13；使用者已提供三階段參數表 | 前饋核心已實作／Host驗證 | 已確認 D1／T1／T2／T3／Tx、有號 0.1% 補償與 PID MV 加總；曲線／事件／Force採專案契約；FF＋PID／熱冷與anti-windup未接。詳見 [前饋對照](DTDM04_FEEDFORWARD_MAPPING.md)。 |
| PID Parameters | 5.1.1～5.1.3，頁 5-1～5-5；使用者已提供四組完整欄位 | CKLIB 封裝＋mock 驗證 | 四組候選選擇已做，後端整組套用與AT回寫未完成；時間精度及 2DOF 係數範圍與 CKLIB 檔頭不一致。詳見 [PID 對照](DTDM04_PID_MAPPING.md)。 |
| Model Output Parameters | 使用者已提供 0x1990～19AB 欄位，手冊文字未找到直接對應 | 已確認為模型輸出，演算法未確認 | 含開關、百分比、增益、時間常數、基礎溫度與唯讀結果；缺單位／公式／來源用途，且 Base Temperature 的 U16 與負值範圍矛盾。詳見 [模型輸出對照](DTDM04_MODEL_OUTPUT_MAPPING.md)。 |
| 輸出折線表 | 使用者已提供 Output Linearization Parameters（10 Sets）；4.1.2 提到 ACO 來源 | 模式包裝已實作／Host驗證 | 已確認每組十個 X／Y 點、內插／步階、來源路由、Offset 與單位；左端步階、Offset與顯式點數／邊界／Off契約已做，L05來源／Register未接。詳見 [逐欄對照](DTDM04_OUTPUT_LINEARIZATION_MAPPING.md)。 |
| Cascade Control Loop Parameters | 5.1.7，頁 5-9～5-10；使用者已提供兩組欄位 | PID封裝與SV換算核心已做 | 已確認絕對／相對界限、Offset、濾波、AT／校準與副控保護溫度。專案映射與時間濾波已定；原Filter Num未確認，AT／校準／保護協調待接；詳見 [串級對照](DTDM04_CASCADE_MAPPING.md)。 |

Control Loop、Model Output 與輸出折線欄位皆已補入逐欄對照；Model Output 還缺演算法與單位，輸出折線已有專案契約，但產品路由與設備相容仍待驗證。本報告不能作為所有 Register 都已完成相容性審查的證明。

## 容易誤判為已完成的項目

### Ramp Soak 不是 Ramp 加一個計時器

手冊 5.1.4 包含四樣式、每樣式十六步驟、起始樣式／步驟、起始斜率、各段目標與時間、有效步驟、額外循環次數及樣式連接；還有執行／停止／保持／恢復與結束狀態。

另外，手冊的等候溫度／等候時間是步驟銜接時的到溫等待，逾時會產生警報，不能只靠 FbInBandTimer 的帶內累積時間就宣稱相同。首步、零時間、跳步與結束保持也須明確定義。程序結束只能產生狀態／動作請求，最終輸出仍由本專案既有權責處理。

### Cascade 不只是兩個 PID

手冊含副控 SV 的絕對／相對轉換上下限、固定補償、SV 濾波、主／副控 AT 選擇及一鍵校準。兩個 PID 可重用，但協調／切換／飽和處理仍缺。單／串級模式先採 STOP 時切換為專案建議，尚未實作；運轉中無擾切換不可宣稱完成。

### 輸入折線與通用 Linearization 的差異

現有 FbLinearization 接收至少兩個點，嚴格驗證 X 遞增；使用者表格提供 14 組中間點及 Low／High 欄位，總插值節點數仍需確認。手冊另有固定值或補償值模式，並描述非遞增點跳過。若沿用目前嚴格拒絕政策，應明列專案差異，不能默認相容。

補償模式可由插值求 Offset 再與輸入相加，但還須決定總和／上下限的順序與錯誤政策。這些未因有 WeightedSum 就自動完成。

### PID 封裝與產品 PID 功能的差異

FbPid.c 已實際呼叫 CKLIB，產品 adapter 只呼叫 L04，分層符合既有要求。測試中的 CKLIB 函式是 test doubles；libcklib.a 仍為零位元組佔位。這只能證明封裝／控制流程，不證明 PID 數值效果。

FbPid公開PV／SV已統一0.001°C，L04邊界暫轉目前CKLIB的0.1°C，最接近值且半格離零取整；L05不得重複換算。下一版待Colin將CKLIB統一0.001°C，實體版本確認後移除轉換。PB等SetParameter刻度與實體解析度仍待核對，不能宣稱後端已有0.001°C精度。MV仍0.1%。

AT SV 比例、熱／冷 PID 參數與限幅已有參數入口；但四組 PID 管理、根據 SV 自動切換與 AT 結果持久化需要額外流程。不得把封裝 API 存在當作整個功能群組完成。

## 既有元件間還需統一的契約

| 項目 | 實際差異 | 建議處理 |
|---|---|---|
| ENABLE | Alarm／Logic 停用清除執行狀態；Ramp／InBand 等有保持狀態的契約 | 逐 FB 明訂暫停／停用／Reset，不以一次全域替換破壞既有行為。 |
| 時間 | Alarm／Logic／PID 為 ms；Ramp／InBand 等為秒 | L05 顯式換算，避免直接傳同一數值。 |
| 溫度 | Sensor／Alarm／PID公開引腳0.001°C；目前CKLIB後端0.1°C | L04集中暫時轉換；下一版Colin統一後端時核對版本並移除，PB等參數另查。 |
| 有效性 | 不同 FB 對無效輸入可能清除或凍結狀態 | L05 檢查 status／valid，避免使用診斷用舊值作控制結果。 |
| 最終輸出 | FB 有輸出限幅不等於 Safety 許可 | 既有 SYSTEM／Safety／輸出服務權責優先，禁止旁路。 |

## 待辦與建議順序

- [x] 取得 Control Loop Register 群組欄位，完成責任分類並標出單位疑點。
- [x] 取得 Model Output 群組欄位，確認為模型輸出並記錄型別／單位疑點。
- [ ] 補齊 Model Output 運算式、來源、用途、單位與狀態契約。
- [x] 取得並對照輸出折線表欄位，確認十點、步階、Offset、來源及單位。
- [x] 定義 L04 輸出折線的顯式點數、左端步階、CLAMP／ERROR、Off 與 Offset 契約並完成 Host 測試；非設備相容性結論。
- [ ] 統一正式控制鏈的溫度／時間換算、來源有效性與 ENABLE／Reset 契約。
- [x] 完成 FbProfileSequencer L04核心與步驟／暫停／恢復／銜接等待／逾時契約；斷電接續及產品串接另列未完成。
- [x] 取得 Ramp Soak 欄位，整理 Pattern／Step、循環與連接方式；程序核心已補，產品串接待接。
- [x] 新增輸入 Switch Over 單實例 FB 與兩來源故障政策並通過 Host 驗證；L05 與 MCU 仍待接入。
- [x] 取得 Switch Over 欄位並核對手冊，確認中間區平均，修正先前僅以切換／遲滯描述的不足。
- [x] 新增跨通道溫差 Alarm 單實例 FB；來源陣列由外層提供，Host 驗證通過，產品 provider／命令分派待接。
- [x] 取得多通道警報欄位，確認一次動作 Hold 與一般 Alarm 不可直接混用。
- [x] 新增事件觸發 Feedforward 單實例 FB並通過Host驗證，與SYSTEM Event維持分層；產品串接另列未完成。
- [x] 取得 Feedforward 參數，整理三階段、Tx 與有號 MV 加總責任；核心已補，產品串接待接。
- [ ] 完成 FbCascadeCoordinator，補單迴路／串級配置與切換規則。
- [x] 取得 Cascade 欄位，拆分 SV 運算、AT／校準及副控保護責任。
- [ ] 釐清 Cascade MV→SV 公式、濾波編號、一鍵校準與保護恢復契約。
- [ ] 完成 PID 群組選擇與 AT 結果管理；依責任評估獨立選組 FB 或 L05 配置模組，不硬塞進基礎 PID。
- [x] 完成獨立 L04 PID 四組候選參數選擇與驗證；正式套用／AT結果管理仍保留上列未完成狀態。
- [x] 取得 PID 四組欄位與 2052h 編碼並對照 CKLIB；200Ch 連動、時間精度、作用中參數區讀寫及切組狀態仍待確認。
- [x] 完成 L04 輸入／輸出折線模式與參數包裝，保留通用插值核心；產品設定儲存與來源路由仍未完成。
- [ ] 接入正式 PID callback、Register／NVM、既有實體輸出路徑，再做 MCU 驗證。

上述待辦區分L04核心與產品整合。先前參考盤點文件已推送（主專案`d8e828a`、PlatformCore`f562ade`）；本輪新增／補齊的核心與更新文件另以Git提交保存。L04測試通過不代表產品控制鏈、CKLIB實體效果或MCU已驗證。

## 查核來源

- DTDM04 v1.5：3.3.4／3.3.5、4.1／4.2、5.1.1～5.1.8、5.2.4。
- PlatformCore：L04_FunctionBlock 各 Fb*.c／Fb*.h、Tests/Unit/L04_FunctionBlock、CKLIB/Lib/README.md。
- 產品：product_alarm_logic_adapter、product_fb_schedule_integration、product_pid_controller_adapter、Tests/Unit/test_product_pid_controller_adapter.c。
- [L05 Alarm／Logic 串接](L05_ALARM_LOGIC.md)、[FB 文件索引](FB_DOCUMENT_INDEX.md)。
