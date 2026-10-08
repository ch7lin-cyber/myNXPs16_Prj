# 工廠校正資料槽與 Sensor type 對映

本階段建立四通道 × 十種 Profile，共 40 組獨立的零點／跨度資料槽，
並讓正常量測選用相同 Profile。資料為 ADC 端 µV，尚未還原分壓。
S16 Debug / Release 編譯符號 `FACTORY_CALIBRATION_INPUT_COUNT=4`，
只配置四通道；平台預設仍支援 16 通道。

## 固定讀取位址

FC03，每組八個 holding registers；所有位址均為十六進位。
位址公式：`0x4C00 + channel * 0x80 + profile * 8`。

| Profile ID | 輸入路徑與 Gain | Sensor type | CH0 | CH1 | CH2 | CH3 |
|---:|---|---|---|---|---|---|
| 0 | TC / mV，Gain 32 | K、D、E、N、L、U、TXK、0～50mV | 4C00 | 4C80 | 4D00 | 4D80 |
| 1 | TC，Gain 64 | T、R、C | 4C08 | 4C88 | 4D08 | 4D88 |
| 2 | RTD，Gain 8、500µA | Pt100、Ni120 | 4C10 | 4C90 | 4D10 | 4D90 |
| 3 | TC，Gain 16 | J | 4C18 | 4C98 | 4D18 | 4D98 |
| 4 | 電壓，Gain 32 | 0～5V、0～10V | 4C20 | 4CA0 | 4D20 | 4DA0 |
| 5 | RTD，Gain 16、500µA | JPt100 | 4C28 | 4CA8 | 4D28 | 4DA8 |
| 6 | 電流，Gain 64 | 0～20mA、4～20mA | 4C30 | 4CB0 | 4D30 | 4DB0 |
| 7 | TC，Gain 128 | B、S | 4C38 | 4CB8 | 4D38 | 4DB8 |
| 8 | RTD，Gain 1、250µA | Pt1000 | 4C40 | 4CC0 | 4D40 | 4DC0 |
| 9 | RTD，Gain 32、500µA | Cu50 | 4C48 | 4CC8 | 4D48 | 4DC8 |

| 相對組別起點 | 內容 | 型別 |
|---:|---|---|
| +0、+1 | 已套用的實測零點 | signed int32 µV，高字在前 |
| +2、+3 | 已套用的實測跨度點 | signed int32 µV，高字在前 |
| +4 | 換算資料可用 | 0 / 1 |
| +5 | 已經 Factory Apply 校正 | 0 / 1 |
| +6、+7 | 保留 | 0 |

例如 CH0 電壓組：零點 `0x4C20～0x4C21`，跨度點 `0x4C22～0x4C23`。
每通道最後 48 個寄存器保留並回傳 0。所有資料槽禁止 FC06 / FC10 直接寫入。
這些正式校正資料位址不受 `PRODUCT_ADC_DEBUG_ENABLE` 開關影響。

## Factory mode 選擇介面

- `0x4703`：通道 0～3。
- `0x4704`：上表 Profile ID 0～9。
- `0x4702`：1 選擇、2 擷取零點、3 擷取跨度、4 Apply、5 Abort。
- `0x4709～0x470C` 仍是目前操作的 pending 點；新資料槽是各組 Apply 後的值。

舊 Profile ID 3 原為獨立 mV 組，現在為 TC Gain 16；mV 改用 ID 0。
舊 ID 5 原為獨立 10V 組，現在為 RTD Gain 16；10V 改用 ID 4。
外部校正工具需依新表更新，不能沿用舊的 3 / 5 定義。

## 各組目標校正點

| ID | 外部低點 | 外部高點 | 目標低點 µV | 目標高點 µV |
|---:|---|---|---:|---:|
| 0 | 0mV | 50mV | 0 | 50000 |
| 1 | 0mV | 30mV | 0 | 30000 |
| 2 | 100Ω | 300Ω | 50000 | 150000 |
| 3 | 0mV | 100mV | 0 | 100000 |
| 4 | 0V | 10V | 0 | 60417 |
| 5 | 100Ω | 300Ω | 50000 | 150000 |
| 6 | 0mA | 20mA | 0 | 34752 |
| 7 | 0mV | 15mV | 0 | 15000 |
| 8 | 1000Ω | 3000Ω | 250000 | 750000 |
| 9 | 50Ω | 150Ω | 25000 | 75000 |

以上依目前電阻、激勵電流及 Gain 設定。電壓／電流目標含分壓換算，
以整數 µV 四捨五入；RTD 目標為電阻 × 激勵電流，非 0µV 零點。
更改硬體參數後須重新核對校正點，舊 NVM 記錄若目標不符會被拒絕還原。

換算公式：`校正值 = 目標低點 + (實測值 − 實測低點) ×
(目標高點 − 目標低點) / (實測高點 − 實測低點)`。
無校正紀錄時，實測低／高點初始化為目標低／高點，因此保持直通，
`可用=1、已校正=0`。原本固定 30000µV 的限制已改成依 Profile 使用目標。

## 保存與開機還原

- Factory Apply：先保存並驗證 NVM 成功，再更新 RAM 的該組資料與已校正標記。
- NVM 寫入／驗證失敗：Apply 失敗、舊 RAM 校正不變；`0x4706=6`（storage），
  Modbus 回覆 Server Device Failure（0x04）。重新選擇 Profile 後可重做擷取。
- 開機在 NVM 初始化後，自動載入各組最新有效記錄，Factory mode 保持鎖定。
- 未校正、空白、版本／目標不符的記錄使用直通預設。CRC 損壞則嘗試另一副本。
- 已有校正資料時，禁止啟動全晶片 FRAM checkerboard 測試；該測試會破壞保存區。
  應在工廠校正之前完成 FRAM 測試。

FRAM 保存區為 `0x010000～0x0113FF`，與參數雙槽分離，共 5120 bytes。
每組預留 128 bytes（A/B 各 64），索引為 `channel * 10 + profile`。
單份使用 36 bytes，小端序固定格式，禁止直接 memcpy C 結構：

| byte offset | 內容 |
|---:|---|
| 0～3 | Magic CAL1 |
| 4～5 | 格式版本 1 |
| 6、7 | 通道、Profile |
| 8～11 | uint32 序號（支援回繞） |
| 12～15 | signed int32 實測低點 |
| 16～19 | signed int32 實測高點 |
| 20～23 | signed int32 目標低點 |
| 24～27 | signed int32 目標高點 |
| 28～31 | 前 28 bytes 的 CRC32 |
| 32～35 | Commit marker |

寫入另一副本前先清除其 Commit，寫入並驗證本文後才寫 Commit。
舊副本保留，開機以 CRC、版本、通道／Profile、目標與序號檢查選擇資料。

## 新增正式狀態寄存器

原本 `0x4700～0x470D` 與 FRAM 測試 `0x4710～0x471C` 皆保留。
以下 FC03 唯讀位址不受 DEBUG 開關影響：

| 位址 | 內容 |
|---|---|
| 0x4720～0x4721 | 目前選擇 Profile 的目標低點，signed int32 µV |
| 0x4722～0x4723 | 目前選擇 Profile 的目標高點，signed int32 µV |
| 0x4724 | Storage ready（1 才能保存） |
| 0x4725 | 開機還原組數 |
| 0x4726 | Storage error：0 OK、1 geometry/read、2 FRAM test busy、3 write/verify |
| 0x4727 | 發現無效且帶 Commit 副本的累計次數 |

工廠操作前先切換該通道 Sensor type、等待新 ADC 設定與讀值穩定，再選 Profile。
依表提供低／高點，各自手動等待穩定後擷取並 Apply。RTD 使用三線精密電阻接法；
不以溫度模擬器的補償值當作直接 ADC 電壓。自動穩定判定與 CJC 校正尚未加入。
校正後仍需量程端點與中間點驗證，不以兩點吻合代替全量程驗證。

驗證指令：`python _Product/Tests/run_factory_calibration_tests.py`。
Host 測試涵蓋 40 組保存／還原、各組端點、直通預設、三個寫入階段中斷、
CRC 回退、FRAM busy、讀取失敗、DEBUG 兩種設定與 Modbus 介面。
實際 FRAM 與校正源仍需在目標板驗證。
