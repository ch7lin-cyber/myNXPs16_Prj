# DTDM04 Control Loop Parameters 對照

## 摘要

2026-10-22：依使用者提供的 Register Map 欄位進行分類，補足先前 Control Loop 群組名稱不明的部分。此群組是控制迴路的整體配置，涵蓋來源、演算法、SV、程序與輸出策略，**不是一個新的 Control Loop 演算法 FB**。

本文件為設計對照，不代表下列欄位已加入產品 Register 或全部完成。以現有專案架構為準；SYSTEM／Safety／輸出服務保留最終權責。DTDM04 位址僅供追溯，不直接複製到本專案。

## 控制模式的四個不同面向

| 面向 | 來源欄位 | 在本架構的意義 |
|---|---|---|
| 控制結構 | Loop Mode | 單迴路或 Cascade，由 L05 選控制鏈。 |
| 自動運算／程序選擇 | Auto Control Mode | 手冊 0=PID、1=ONOFF、2=Ramp Soak PID；可在產品內拆為演算法選擇＋SV 來源，避免將程序與串級互斥。 |
| SV 選擇 | SetValue Selection、SV1／SV2 | 固定 SV 選擇；程序模式由動態 SV 提供。Cascade 外迴路接受選定的 SV，內迴路 SV 由協調器產生。 |
| 運行輸出模式 | Auto/Manual | 自動結果或手動候選 MV；最終仍經既有安全許可。不是 Alarm 的 AUTO／MANUAL 重置模式。 |

單迴路／Cascade 與固定／程序 SV 在本專案分開配置；這是架構設計，不代表手冊已明訂所有組合都支援。PID Standard／Fast 亦不等於 CKLIB PID／PID_FB 兩種 controller 類型。

## 逐組欄位與責任

下列位址皆為 DTDM04 十六進位起始位址；一般四通道連續四個 word，Loop Mode 僅兩個組別。Reserved 不建立運算功能。

| 欄位 | 原位址 | 主要責任／可重用元件 | 現況或待補 |
|---|---|---|---|
| PV Source Assignment | 0x1400～1403 | L05 來源路由：預設通道、In1～4、Switch PV1～2 | 現有 Alarm 固定讀對應 Snapshot；正式 PID 尚無此完整來源配置，Switch Over FB 未實作。 |
| Loop Mode | 0x1404～1405 | L05 單／串級控制鏈選擇 | 模式切換與 FbCascadeCoordinator 未完成；不照搬固定 CH 配對。 |
| Auto Control Mode | 0x1408～140B | L05 選擇 ONOFF／PID 與固定／程序 SV | PID／ONOFF 封裝有，ProfileSequencer 尚為佔位。 |
| Control Action | 0x140C～140F | L04 FbPid 的 HEAT／COOL／HEAT_COOL | 已有設定入口；Reverse／Direct 的映射仍須以實際被控對象方向定義，不只依名稱硬轉。 |
| PID Mode Selection | 0x1410～1413 | FbPid 的 PID_MODE 參數／AT 策略 | 已有 backend 參數入口，Standard／Fast 的確切 CKLIB 值與效果需驗證；不能拿 PID_FB 代替 Fast。 |
| SetValue Selection | 0x1414～1417 | L05 固定 SV1／SV2 選擇 | 尚未建立正式選擇與 Register 配置。 |
| SV1／SV2 | 0x1418～141F | L05 迴路設定資料，傳入控制 FB | 現有 Alarm 可設定每通道單一 SV；不等同完整雙 SV 功能。 |
| SV-H／SV-L | 0x1420～1427 | 產品參數驗證與來源量程管理 | 須依所選 PV 來源與量綱驗證，非一律依同編號 Sensor。驗證 SV-L≤SV-H 且 SV1／SV2 在範圍內；Apply 前檢查整組一致性。 |
| Input Fail MV1／MV2 | 0x1428～142F | 既有 SYSTEM／Safety／輸出策略 | 不放入 PID 計算。手冊「不受 MV 上下限」僅指其控制限幅，不能當作繞過本專案 Safety 的依據。 |
| Standby MV1／MV2 | 0x1430～1437 | 產品運行狀態與既有輸出策略 | 控制待機與 Alarm STANDBY 不同；不能共用同一旗標或規則。 |
| Auto/Manual | 0x1488～148B | L05 控制輸出候選選擇＋既有輸出服務 | 尚無完整模式切換流程；手動也不能直接驅動硬體。 |
| Manual MV1／MV2 | 0x1438～143F | 手動候選輸出值，經既有安全路徑 | 0～1000 為 0.1%；無擾切換、恢復及持久化政策尚待定義。 |
| PDOF | 0x1440～1443 | FbPid 參數 FB_PID_PARAMETER_PDOF | 已有封裝入口；實際語意／值域及 Ti=0 行為由實體 CKLIB 驗證。 |
| Deadband | 0x1444～1447 | FbPid 依 ONOFF／PID 選不同 backend 參數 | 入口已分流；所提供表格的單位需再釐清，見下節。 |
| Out TS／Out2 TS | 0x1448～144F | FbPid ONOFF 感度參數 | 已有 OUT1_TS／OUT2_TS 入口；冷卻側有效條件及溫度換算由產品驗證。 |
| SP Ramp Rate | 0x148C～148F | L04 FbSetpointRamp＋L05 單位換算／起始值選擇 | 通用 Ramp 有，產品串接未做；現有零斜率為保持，不可未確認就等同手冊零值。 |
| Slope／Ramp Soak time unit | 0x1450～1453 | L05 將 Min／Sec 轉成明確的內部時間 | 不把同一無單位數值直接傳給 FB；斜率欄固定寫 /min 與此選項的關係仍需核對。 |
| Starting Pattern／Starting Step | 0x1454～145B | FbProfileSequencer 的程序與啟動參數，由 L05 管理表格 | 尚未實作；4 樣式／16 步驟可作產品容量，不固定在通用 FB。 |
| Starting Slope | 0x1468～146B | 程序首段政策＋斜率運算 | 尚未實作程序首段／零時間規則；手冊原有 0.01／0.1°C 不直接決定本專案內部刻度。 |
| Waiting Temperature／Waiting Time | 0x146C～1473 | 程序銜接等待與逾時結果 | FbInBandTimer 只是基礎元件；完整等待狀態／逾時動作尚未完成。 |
| Resume Ramp Soak Status | 0x1490～1493 | 程序 FB 匯出可恢復狀態；L05／NVM 儲存與開機恢復政策 | 尚未實作。需保存程序版本、樣式／步驟、剩餘時間、階段與動態 SV 等足夠資訊；恢復程序不代表可自動恢復實體加熱。 |
| MV1／MV2 Upper／Lower Limit | 0x1474～1483 | FbPid 控制限幅參數，必要時搭配 FbOutputLimiter | 已有 PID 參數入口；產品適用模式及最終安全限制應分開，手冊的手動／ONOFF 例外不直接移植。 |
| AT SV Ratio | 0x1484～1487 | FbPid AUTO_TUNE_SV_RATIO | 已有入口；產品命令、AT 結果管理及實體控制效果仍待完成。 |

0x145C～1467、0x14A0 起的 Reserved 保留為未定義；不因位址空缺自動填入新功能。

## 需要明確標示的資料疑點

1. **Deadband 單位：**使用者表格統一寫 0.1°C／°F，但 DTDM04 手冊 5.1.1（頁 5-2）描述 PID 加熱／冷卻死區以輸出百分比表示，ONOFF 則涉及溫差。應依模式分開定義，不能使用同一刻度無條件轉換，也不能對所有 Deadband 都乘以 100。
2. **Waiting Temperature 單位：**表格寫 °C/min／°F/min，但手冊 5.1.4 的用途是目標值±等候溫度，因此是溫差帶，不是變化率。負值範圍與特殊語意仍待確認，不能先取絕對值或默認允許負容差。
3. **時間選擇與斜率：**1450 選 Min／Sec，斜率欄卻寫固定 /min；需確認原手冊與實際設定是否同時影響斜率。先保持來源標註，不推測轉換。
4. **Loop Mode 位址：**只有 1404／1405，符合兩組串級配對，而非四通道各一個 word。產品配置可更彈性，但不能把來源表的空格誤解析成 1406／1407。
5. **SV 上下限與來源：**來源可換成別通道或 Switch PV，量程應跟來源關係一起驗證；Sensor Type 改變時不能無聲套用來源產品預設值 8500／−2000。
6. **名稱相似但責任不同：**Auto/Manual 輸出模式、Alarm reset_mode、自動整定模式與 SINGLE／CASCADE 是不同設定，不共用列舉或 Register。

上述疑點可以先列入待辦，不阻擋一般程序／串級介面設計；實作精確相容轉換前才需逐項確認。

## 融入本專案的規則

- SV／溫差採專案 0.001°C 契約，Register 建議使用實際 °C 的 Float32；斜率另註明 °C/s 或 °C/min；MV 維持獨立百分比刻度。FbPid 現有 0.1°C 邊界仍是待辦，不能宣稱已改。
- 設定以 Pending／Apply 做整組驗證；SV-L／SV-H、來源及模式互相關聯，不只檢查單欄範圍。
- 單／串級與自動／手動切換需各自定義狀態銜接；第一版可採 STOP 時切換，但尚未實作。
- Input Fail／Standby／Manual 只能產生產品定義的候選輸出；Safety 許可、硬體限制與故障處置沿用本專案，不搬用手冊的旁路規則。
- Resume 屬可選的產品恢復能力，先完成程序狀態機，再做 NVM schema 與開機政策。

## 新確認的待辦

- [ ] 控制迴路的 PV 路由與來源相關量程驗證。
- [ ] SINGLE／CASCADE、演算法、SV 來源、自動／手動的獨立配置與相容性檢查。
- [ ] SV1／SV2 與 SV 限制的整組 Apply。
- [ ] 程序銜接等待、逾時狀態、程序狀態保存與恢復。
- [ ] 與既有輸出權責整合 Input Fail／Standby／Manual 候選值。
- [ ] 核對 Deadband、Waiting Temperature 與斜率時間單位。

本次只更新文件，未變更程式、Register 位址或輸出控制行為。
