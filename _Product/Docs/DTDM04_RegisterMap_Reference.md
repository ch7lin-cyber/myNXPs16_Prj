# DTDM04 舊 Register Map 參考註記

## 用途與限制

此文件保存使用者提供的 DTDM04 舊版 Register Map，作為本產品未來多機台串接及內部 CAN Bus 資料交換的需求參考。它不是本產品目前的 Register 規格，也不代表下列功能已實作或已定案。

DTDM04 的位址、資料寬度、預設值、編碼及控制語意不得直接搬入本產品。尤其舊表從 `0x2000` 開始，而目前產品已在 `0x2000..0x2008` 配置韌體及相容版本資訊；位址存在明確衝突。未完成架構與 Register Map 審查前，不新增、保留或重用這些舊位址。

## 舊參數與未來可能用途

| DTDM04 舊參數 | 舊位址 | 後續可能用途 | 設計時需重新確認 |
|---|---:|---|---|
| Temperature Scale | `0x2001` | UI 顯示單位或 CAN 傳送的顯示設定 | 溫度內部計算與跨機台傳輸應使用固定工程單位；顯示轉換由接收端處理或明確定義 |
| Channel Disabled | `0x2002` | 通道啟用狀態及通道資料有效性發布 | 禁能後是停止取樣、停止控制，還是僅不發布；需定義失效狀態與安全處置 |
| Control Stop After Power up | `0x2003` | 多控制器上電後的啟動／待機政策 | 各機台的本地啟動政策、遠端主控命令與 Safety/Fault 優先權需分開定義 |
| 4CH RUN/STOP | `0x2004` | 群組化 RUN/STOP 命令，或 CAN 內部控制命令 | 命令來源、目標機台、確認回覆、逾時、重複命令與失聯後行為 |
| 4CH AT | `0x2005` | 群組化 Auto-Tuning 命令 | 本產品是否採用 AT、支援哪些通道，以及多機台同步啟動條件 |
| LED Status(Device) | `0x2006` | 機台狀態／診斷資料發布 | 定義穩定的狀態 bit、錯誤來源與版本相容性；不以 LED 狀態取代 Fault/Safety 資料 |
| Response waiting Time | `0x2007` | 舊 RS-485 回覆延遲參數的參考 | 不直接沿用；本產品已有各通訊埠 response delay 設定，CAN 時序應獨立規劃 |
| RS-485 Station Shift | `0x2008` | 站號編址或串接機台識別的參考 | CAN Node ID、Modbus Station ID、旋鈕值及站號偏移需明確區分 |
| Rotary SW Value | `0x2009` | 本地旋鈕／硬體位址狀態 | 確認各產品的硬體來源、極性與是否允許由網路改寫 |
| Physical Position | `0x200A` | 模組鏈中的實體位置／拓樸資訊 | 定義由安裝設定、掃描結果或背板位置取得，以及拓樸變更處理 |
| Module Scanning Period | `0x200B` | 內部 CAN 節點掃描或資料更新週期 | 與既定的高／中／低資料更新期限、匯流排負載及 10/50/100 ms 需求一起評估 |
| Sampling Period of Analog Input | `0x200C` | 類比資料取樣設定或跨機台資料新鮮度描述 | 本產品 ADC ODR、應用排程與 CAN 發送週期是不同概念，不應以單一參數混用 |
| Power Supply Frequency | `0x200D` | 電源頻率／抗干擾濾波設定 | 定義對 ADC filter、量測品質及多機台設定一致性的影響 |
| Number of DO Modules / DI Modules | `0x200E..0x200F` | 節點能力或拓樸摘要 | 定義模組發現、最大數量、能力描述及節點離線時的資料有效性 |
| Alarm action in standby mode | `0x2010` | 待機期間各通道 Alarm 行為 | 與 Run/Stop、Alarm enable、故障優先權及遠端狀態同步一起定義 |
| Compensation for disturbance | `0x2011` | 外亂補償功能設定 | 後續對應本產品 Function Block／控制流程與跨節點 PV 來源 |
| Display dynamic SV | `0x2012` | 動態／靜態 SV 顯示或來源選擇 | 釐清此項是顯示行為、控制 SV 選擇，或兩者；跨機台時需定義責任節點 |
| Lead Address | `0x2013` | 首台／主機識別與模組串列歸屬 | 定義主從角色、群組識別、冗餘／主機失效接替及 CAN 拓樸關係 |

## 建議納入後續 CAN 規劃的資料群組

1. **節點身分與拓樸**：Node ID、Station ID、主／從角色、首台識別、實體位置、產品／模組型別、DI/DO 能力與數量。
2. **節點狀態**：Run/Stop、通道啟用、待機狀態、Alarm/Fault/Safety 摘要、通訊在線狀態及資料有效性。
3. **命令與回覆**：RUN/STOP、AT 或其他控制命令的來源、目標、序號、接受／拒絕回覆、逾時及失聯處置。
4. **量測與控制資料**：PV、SV、狀態與必要診斷；每筆資料需定義來源節點、通道、工程單位、更新期限、有效旗標、時間戳或序號。
5. **傳輸週期與優先權**：評估 DTDM04 Module Scanning Period 的概念如何對應本專案已討論的高／中／低更新需求；掃描、取樣與 CAN 發送週期須分別表示。
6. **全域設定**：溫度顯示單位、電源頻率、待機 Alarm 行為等；需定義由哪個節點管理、何時同步及設定不一致時的行為。

以上是候選需求清單，尚未形成 CAN PDO／訊息格式、Register 位址或正式行為規格。CAN 方案定案後，需再確認資料所有權、方向、週期、優先級、失聯逾時、啟動同步、版本相容性及 Safety 優先權，再更新正式 Register Map 與通訊文件。

## Alarm Parameters 舊表參考

使用者提供的 Alarm 表涵蓋三組 Alarm 的 Mode、Delay、Option、High／Low、Alarm 3 Loop Break Alarm（LBA）、Reset 與 Peak High／Low。舊表只用於了解需求脈絡；其位址 `0x1300..0x137B`、資料單位、Option bit 與各 Mode 編碼均不直接成為本產品規格。

### 可參考的功能概念

| 舊表概念 | 本產品架構中的設計方向 | 不直接沿用的部分 |
|---|---|---|
| NO、Deviation、Absolute、Hysteresis Mode | 以明確列舉的比較模式設定 L04 FbAlarm；目前 L04 已有 Disabled、Relative High/Low、Absolute High/Low、Hysteresis High/Low | 不沿用舊 mode 數字；相對 Alarm 的 High／Low 是相對 SV 的 signed offset，需由模式定義其方向 |
| Alarm Delay | 使用 assertion delay，指定單位與允許範圍；目前 FbAlarm 介面為毫秒，只有成立延遲，條件解除會取消計時 | 舊表秒為單位、0..100 秒的限制不照搬；不上下沿都套用延遲 |
| HOLD | 用獨立 Reset Mode 表示 Auto 或 Manual latch | 不把 latch 混在 Option bit；Reset 期間條件仍成立時應拒絕 Reset |
| 輸出反向 | 保留為 Alarm 邏輯輸出反向選項，位於比較／延遲／鎖存之後 | 反向 Alarm 不得覆蓋 Safety/Fault 優先權；Alarm 暫不直接驅動實體 DO/PWM |
| 待機 | 由明確的 Standby 條件與待機帶寬定義何時開始比較 | Control STOP、待機、輸出禁止是不同狀態，不合併成單一 bit |
| 峰值記錄／清峰值 | 每個 Alarm instance 分別保存有效旗標與 Peak High／Low，提供明確清除命令 | 峰值是否存 FRAM、重開機後是否保留、採樣有效性與更新時機另行定義；不假設舊表的只讀峰值規則 |
| Alarm Reset | 每組 Alarm 使用明確 Reset command，與設定值分離 | 不沿用一次寫入的 bit mask 與舊 Reset 位址；需定義 FC06／FC10 寫法、重複命令與拒絕原因 |
| Emergency Stop 選項 | 若產品需要急停，應設為獨立 Safety/Fault source，經 SystemRoutine／SafetyService 執行安全輸出處置 | 急停不可作為一般 Alarm Option，也不可因 Alarm 設定反向或待機而解除 |
| Loop Break Alarm（LBA） | 先定義輸出飽和、PV 未響應、觀察時間與故障分類，再決定是否新增專用 FB／System Routine | L04 FbAlarm 現有模式沒有 LBA；舊表 `18` 編碼及「輸出飽和後再檢查 Alarm 區間」不可直接當成已定義需求 |

### 與目前實作的界線

- PlatformCore L04 `FbAlarm` 已提供比較模式、Standby、assertion delay、Auto／Manual reset、反向、峰值與狀態結果；其時間與數值語意以 `FbAlarm.h`／`FbAlarm` 規格為準。
- 本產品 L05 Alarm／Logic adapter 預留每通道 3 組 Alarm，週期 20 ms；目前 Alarm 設定只存在 RAM API，沒有正式 Modbus Register、FRAM 欄位或實體輸出串接。
- 目前溫度 Alarm provider 使用 0.001°C 工程刻度與有效溫度 Snapshot；電壓／電流 Alarm 尚未納入該 provider。不同 Sensor 單位不可直接共用未定義刻度的 High／Low Register。
- 若 Alarm 會透過內部 CAN 發布，應傳送節點／通道／Alarm 識別、狀態與有效性；設定下發需另行定義權限、版本、確認與逾時。一般 Alarm 狀態不得等同安全急停或 Fault latch。

正式設計 Alarm Register 前，應以本產品 FbAlarm 與 L05 API 契約為基礎，再決定每通道組數、參數刻度／單位、設定套用／保存、狀態讀回、Reset／清峰值命令、事件紀錄與 CAN 發布方式。相關實作狀態見 [`L05_ALARM_LOGIC.md`](L05_ALARM_LOGIC.md)。
