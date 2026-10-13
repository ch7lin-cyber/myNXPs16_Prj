# CALYS 1500 自動校正設計

## 目的與邊界

`calys1500_loop_test.py` 的選單 2 先依 CALYS／DUT 的實際接線方式選擇校正類別。操作人依畫面接好該類別的線一次後，程式在不要求重新接線的前提下，依序處理此接線類別所涵蓋的 Sensor type 與韌體 Profile。每個通道的每個 Profile 只校正一次；不同接線類別若重複包含同一 Profile，工具須辨識已完成並跳過，除非之後確認硬體設計應拆成獨立 Profile。兩點 ADC 微伏校正修正量測鏈的 Offset 與 Gain。校正值位於 ADC µV 層，TC 加入 DUT CJC 之前；不以溫度 PV 誤差直接推算 ADC 校正值，也不校正感測器曲線或 CJC。

校正流程自動設定 CALYS 輸出、等待新鮮且穩定的 DUT ADC 樣本、擷取低／高點、檢查並計算結果。資料先寫入 CSV 作為可審查記錄。只有操作人明確輸入確認字串後，工具才送出 DUT Factory Apply 命令並讓韌體寫入 NVM。預設中止及任何檢查失敗都不 Apply。

## 接線類別與韌體 Profile 對映

第一層選單依「校正時的外部接線」分組；類別標題要顯示 CALYS 端子和 DUT 端子接線圖／文字，接線確認後再進入組內 Sensor。初步分類如下，端子對映須依實際 CALYS 1500 模擬功能與 DUT 電路圖驗證後定稿：

| 校正類別 | 操作時接線方式 | 組內 Sensor type | 涵蓋韌體 Profile |
|---|---|---|---|
| 熱電偶 TC | TC 模擬用正／負端子接到 DUT 熱電偶輸入線對；組內不重接線 | K、J、T、E、N、R、S、B、L、U、TXK、C、D | 0、1、3、7 |
| RTD 三線 | CALYS RTD 三線模擬端依圖接到 DUT L1/L2/L3；同類 RTD 共用此線組 | Pt100、JPt100、Ni120、Cu50、Pt1000 | 2、5、8、9 |
| 電壓／mV | CALYS V 輸出與 COM 接 DUT 電壓輸入線對 | 0–50 mV、0–5 V、0–10 V | 0、4 |
| 電流 | CALYS mA 輸出與 COM 串入 DUT 電流輸入回路 | 0–20 mA、4–20 mA | 6 |

組內各 Sensor 可以改變 CALYS 輸出模式與 DUT Sensor type，但不得要求移動測試線。每個輸出點仍使用所選 Sensor 的校正刺激與對應 Profile target。若實際儀器端子／DUT 端子在組內不相同，必須拆成不同接線類別；不要為了方便在選單中共組。

目前已實作選單 2 的引導式兩點擷取與預覽，依本文件的接線類別和韌體 Profile 運作；完成的預覽／Apply 記錄為 JSON。低／高 CALYS 設定值由操作人輸入，程式不假設不同 Profile 的刺激轉換曲線。Apply 預設關閉，只有加 `--allow-calibration-apply` 並輸入逐次確認字串才會送出。現場線路、端子標示及標準源對應仍待產品板核實，這項人工確認不能代替實際校驗。

自動校正應依韌體 Profile ID 選取，不以一組泛用 Gain 清單推測 Profile。以 `_Product/Docs/FactoryCalibrationSlots.md` 的映射為準：

| Profile | 路徑與 Gain | Sensor 對映 | 校正目標低／高（ADC µV） |
|---:|---|---|---:|
| 0 | TC／mV，Gain 32 | K、D、E、N、L、U、TXK、0–50 mV | 5000 / 45000 |
| 1 | TC，Gain 64 | T、R、C | 3000 / 27000 |
| 2 | RTD，Gain 8、500 µA | Pt100（代表）、Ni120（驗證） | 27858 / 176643 |
| 3 | TC，Gain 16 | J | 10000 / 90000 |
| 4 | 電壓，Gain 32 | 0–10 V（代表） | 6042 / 54375 |
| 5 | RTD，Gain 16、500 µA | JPt100 | 53891 / 116903 |
| 6 | 電流，Gain 64 | 0–20 mA（代表） | 3475 / 31277 |
| 7 | TC，Gain 128 | B、S | 1500 / 13500 |
| 8 | RTD，Gain 1、250 µA | Pt1000；CALYS -220～740 °C 的 10%／90% | 126176 / 819359 |
| 9 | RTD，Gain 32、500 µA | Cu50 | 21790 / 38910 |

每個接線類別中的 Sensor/Profile 均須檢查韌體 Profile 對映；同一 Profile 僅 Apply 一次。工具啟動後讀 `0x4703`、`0x4704`、`0x4705`、`0x4706`、`0x4707`、`0x4720–0x4727`，並以唯讀校正槽 `0x4C00 + CH*0x80 + Profile*8` 保存 Apply 前的校正狀態。目標點以韌體 `0x4720–0x4723` 為準，不在 Python 另複製一套常數。韌體 `ProductApplication` 將 AD7124 原始 `sample.microvolts` 更新到 FactoryCalibrationService；校正 PV 時使用的 calibrated µV 是另一條路徑。因此再次校正擷取的是未校正 ADC µV，不會把舊的校正結果疊加進新係數。

## 校正點與輸出來源

低／高點直接採用各 Profile 定義的 ADC µV 目標。每個 Profile 需有明確、已驗證的 CALYS 輸出模式及接線：

- TC／mV：以已確認的 mV 輸出與前端電壓路徑校正；不得把 CALYS 熱電偶溫度顯示值當 ADC µV。指定 profile 前須確認 DUT Sensor 設定不改變待校正的類比路徑。
- 電壓：輸出 0 V 與 Profile 對應高點電壓，依已驗證分壓模型換算 ADC 目標。
- 電流：輸出 0 mA 與 20 mA，接妥完整電流迴路；4–20 mA Sensor 共用該 ADC Profile，但校正點仍依 Profile 的 0–20 mA ADC 目標。
- RTD：使用精密電阻或已驗證的 RTD 模擬接法，依 Profile 目標電阻及 DUT 激勵電流產生 R×I 等效 µV。CALYS 輸出電阻與 DUT 三線端子對映未逐 Profile 驗證前，不開放自動 Apply。

每種來源在程式內須有 `profile -> CALYS mode, output low/high, DUT sensor code, wiring note, reference model` 的明確資料表；若來源、曲線或接線尚未驗證，該 Profile 顯示「僅可量測／不允許 Apply」。不能用 `_X` 或近似曲線推導校正標準。

## 執行狀態流程

1. **選擇與接線**：選 CH 和接線類別；畫面顯示該組 CALYS／DUT 端子接線，操作人接線並確認。接著選擇組內一個或多個 Sensor type。工具列出組內所有唯一 Profile，並略過本次已完成或本通道已校正的重複 Profile；切換 Sensor／CALYS 輸出時不要求重接線。確認 CALYS 與 DUT 是不同 COM Port。
2. **前置檢查**：連線識別 CALYS；確認 DUT Online、Profile 可用、FRAM storage ready (`0x4724=1`)、沒有輸入故障、沒有 FRAM 測試占用。顯示目前是否已有校正及套用前資料。
3. **解鎖與選 Profile**：依韌體 Factory mode 寫兩個 `0x1234` key 到 `0x4700/0x4701`，選通道 `0x4703`、Profile `0x4704`，命令 `0x4702=1`。讀回狀態／錯誤確認選取成功。
4. **低點擷取**：CALYS 輸出低點；等候設定 dwell，之後每 100 ms 讀成功樣本數、PV input error、ADC live µV、ADC fault 狀態。要求成功樣本數增加，且連續 5 筆 µV 變化均在穩定門檻內，才寫 `0x4702=2` 擷取零點。
5. **高點擷取**：同樣程序輸出高點，確認穩定後寫 `0x4702=3` 擷取跨度。
6. **預覽與檢查**：讀 `0x4709–0x470C` pending 點、Profile 目標、狀態與錯誤。要求 measured span 與 target span 同方向、非零、未飽和且增益修正比例位於可配置限制。Firmware 目前擷取跨度另要求至少 1000 µV。展示 offset、gain 修正比例、Apply 前後端點與中間驗證安排。違反任一項即 Abort。
7. **Apply 關卡**：產生含 CH、Sensor、Profile、舊／新點、預估修正比例的確認摘要；只有輸入明確字串 `APPLY CHn PROFILE p` 才送 `0x4702=4`。輸入其他文字則送 Abort `0x4702=5`，保留 CSV 預覽，不改 NVM。
8. **讀回驗證**：確認 command error=0、Factory calibrated=1、校正槽低／高點符合預覽、storage error=0。完成後 CALYS 再輸出至少三個驗證點（低、中、高），各點都等待新成功樣本並記錄 ADC µV、PV、input error；另產生校正報告。
9. **接線類別收尾**：完成組內 Profile 後輸出該類別摘要，標示成功、略過及失敗 Sensor/Profile。操作人確認要換到另一接線類別時，才顯示下一組接線圖並要求手動改線。任何例外、Ctrl+C、通訊逾時或穩定超時皆 Abort 目前 Factory 操作並記錄 CSV；不得把中斷當成功；CALYS LOC 仍須保證執行。

## 穩定與拒絕條件

穩定判定建議採連續 5 筆成功 ADC µV 樣本的最大值減最小值；20 µV 僅為初始候選值，需用目標板量測決定並允許 CLI 設定；每點有獨立 timeout。樣本必須是新的成功計數，`input_error` 必須為韌體正常碼 61；ADC active fault 必須為 0；RAW 不得等於 0 或 `0xFFFFFF`；轉換結果必須有效。TC 若有 CJC 來源誤差仍不把它吸收到 ADC profile，並在報告註明 CJC 狀態。

穩定門檻與最大 Gain 修正比例應是明確可設定、記入報告的政策值，不可默默硬編碼成儀器精度保證。首次在真實板上執行時採嚴格預設，任何讀取逾時、資料不同步、映射不符、Profile target 不符、Storage not ready 或修正比例超界都拒絕 Apply。

## Modbus 命令與 Apply 寫入邊界

校正程序只寫 Factory mode 控制介面；不可 FC06／FC10 直接寫 `0x4C00` 以上唯讀校正槽。正式操作命令為 `0x4700/0x4701` unlock、`0x4703` channel、`0x4704` Profile、`0x4702` action。零點／跨度擷取和 Apply 由韌體驗證及 NVM service 處理。外部工具應逐筆讀回狀態，禁止對 Apply 命令自動重送；遇到 Apply 回應逾時時先查狀態及校正槽，再決定結果，不可重複 Apply。

## 測試分階段

1. 單元測試：假 CALYS/DUT transport 驗證狀態機、穩定視窗、超時、CRC/timeout retry、profile mapping、取消及 Apply 確認字串；所有失敗路徑斷言不送 Apply。
2. Dry-run：真實連線與輸出／擷取，但停在預覽，不送 `0x4702=4`；核對 CALYS setpoint、DUT live µV、兩端差、CSV。
3. 單一代表 Profile：先用 0–50mV 或電流 Profile，在硬體與參考源都確認後明確 Apply；以低／中／高點驗證。
4. 逐 Profile 放行：每種接線、輸出模式及韌體映射單獨驗證後，才解除該 Profile 的 Apply 禁止。

選單 2、`useMethod.md`、`FactoryCalibrationSlots.md` 已更新為接線類別 → Sensor/Profile 流程。JSON 包含 Profile target、measured zero/span、修正比例、穩定統計、Apply 前後槽值。尚未加入 Apply 後的低／中／高驗證掃描；Apply 完成後應另跑 `1.1.0` 十點測試並審查 ADC µV 和 PV 差值。

## 尚待驗證項目

- CALYS 1500 每個 Profile 對應輸出模式、可輸出端點與端子接線。
- 每個接線類別的 CALYS／DUT 實際端子對映、同組切換 Sensor 是否不必改線，需逐類別用電路圖及 CALYS 顯示圖確認。
- 不同 Sensor type 共用 Profile（尤其 TC 與 0–50mV 共用 Profile 0）時，ADC 前端與物理接線是否確實相同；若不同，韌體共享校正槽與免重接線需求衝突，須先調整 Profile 設計，不可在同槽重複 Apply。
- 韌體目前 `FactoryCalibrationService_UpdateLiveMicrovolts()` 接收 `AnalogInputService` 的原始 `sample.microvolts`；因此重校不會疊加舊係數。仍須以 host test 和目標板確認每個 Profile 選擇與 live µV 都對應同一個 ADC 裝置／輸入。
- 目標板上每個 Profile 的 stabilizing dwell、µV 門檻與合理 gain correction bounds。

在以上事項確認前，自動校正只做 Dry-run 和預覽；不對所有 Profile 一次性開放 Apply。
