# CALYS 1500 自動校正設計

## 目的與邊界

`calys1500_loop_test.py` 的選單 2 對單一 DUT 通道執行兩點 ADC 微伏校正，修正量測鏈的 Offset 與 Gain。校正值位於 ADC µV 層，TC 加入 DUT CJC 之前；不以溫度 PV 誤差直接推算 ADC 校正值，也不校正感測器曲線或 CJC。

校正流程自動設定 CALYS 輸出、等待新鮮且穩定的 DUT ADC 樣本、擷取低／高點、檢查並計算結果。資料先寫入 CSV 作為可審查記錄。只有操作人明確輸入確認字串後，工具才送出 DUT Factory Apply 命令並讓韌體寫入 NVM。預設中止及任何檢查失敗都不 Apply。

## 既有韌體 Profile 對映

自動校正應依韌體 Profile ID 選取，不以一組泛用 Gain 清單推測 Profile。以 `_Product/Docs/FactoryCalibrationSlots.md` 的映射為準：

| Profile | 路徑與 Gain | Sensor 對映 | 校正目標低／高（ADC µV） |
|---:|---|---|---:|
| 0 | TC／mV，Gain 32 | K、D、E、N、L、U、TXK、0–50 mV | 0 / 50000 |
| 1 | TC，Gain 64 | T、R、C | 0 / 30000 |
| 2 | RTD，Gain 8、500 µA | Pt100、Ni120 | 50000 / 150000 |
| 3 | TC，Gain 16 | J | 0 / 100000 |
| 4 | 電壓，Gain 32 | 0–5 V、0–10 V | 0 / 60417 |
| 5 | RTD，Gain 16、500 µA | JPt100 | 50000 / 150000 |
| 6 | 電流，Gain 64 | 0–20 mA、4–20 mA | 0 / 34752 |
| 7 | TC，Gain 128 | B、S | 0 / 15000 |
| 8 | RTD，Gain 1、250 µA | Pt1000 | 250000 / 750000 |
| 9 | RTD，Gain 32、500 µA | Cu50 | 25000 / 75000 |

工具啟動後讀 `0x4703`、`0x4704`、`0x4705`、`0x4706`、`0x4707`、`0x4720–0x4727`，並以唯讀校正槽 `0x4C00 + CH*0x80 + Profile*8` 保存 Apply 前的校正狀態。目標點以韌體 `0x4720–0x4723` 為準，不在 Python 另複製一套常數。韌體 `ProductApplication` 將 AD7124 原始 `sample.microvolts` 更新到 FactoryCalibrationService；校正 PV 時使用的 calibrated µV 是另一條路徑。因此再次校正擷取的是未校正 ADC µV，不會把舊的校正結果疊加進新係數。

## 校正點與輸出來源

低／高點直接採用各 Profile 定義的 ADC µV 目標。每個 Profile 需有明確、已驗證的 CALYS 輸出模式及接線：

- TC／mV：以已確認的 mV 輸出與前端電壓路徑校正；不得把 CALYS 熱電偶溫度顯示值當 ADC µV。指定 profile 前須確認 DUT Sensor 設定不改變待校正的類比路徑。
- 電壓：輸出 0 V 與 Profile 對應高點電壓，依已驗證分壓模型換算 ADC 目標。
- 電流：輸出 0 mA 與 20 mA，接妥完整電流迴路；4–20 mA Sensor 共用該 ADC Profile，但校正點仍依 Profile 的 0–20 mA ADC 目標。
- RTD：使用精密電阻或已驗證的 RTD 模擬接法，依 Profile 目標電阻及 DUT 激勵電流產生 R×I 等效 µV。CALYS 輸出電阻與 DUT 三線端子對映未逐 Profile 驗證前，不開放自動 Apply。

每種來源在程式內須有 `profile -> CALYS mode, output low/high, DUT sensor code, wiring note, reference model` 的明確資料表；若來源、曲線或接線尚未驗證，該 Profile 顯示「僅可量測／不允許 Apply」。不能用 `_X` 或近似曲線推導校正標準。

## 執行狀態流程

1. **選擇**：選 CH、已知 Sensor 和 Profile；檢查三者映射一致。確認 CALYS 與 DUT 是不同 COM Port。
2. **前置檢查**：連線識別 CALYS；確認 DUT Online、Profile 可用、FRAM storage ready (`0x4724=1`)、沒有輸入故障、沒有 FRAM 測試占用。顯示目前是否已有校正及套用前資料。
3. **解鎖與選 Profile**：依韌體 Factory mode 寫兩個 `0x1234` key 到 `0x4700/0x4701`，選通道 `0x4703`、Profile `0x4704`，命令 `0x4702=1`。讀回狀態／錯誤確認選取成功。
4. **低點擷取**：CALYS 輸出低點；等候設定 dwell，之後每 100 ms 讀成功樣本數、PV input error、ADC live µV、ADC fault 狀態。要求成功樣本數增加，且連續 5 筆 µV 變化均在穩定門檻內，才寫 `0x4702=2` 擷取零點。
5. **高點擷取**：同樣程序輸出高點，確認穩定後寫 `0x4702=3` 擷取跨度。
6. **預覽與檢查**：讀 `0x4709–0x470C` pending 點、Profile 目標、狀態與錯誤。要求 measured span 與 target span 同方向、非零、未飽和且增益修正比例位於可配置限制。Firmware 目前擷取跨度另要求至少 1000 µV。展示 offset、gain 修正比例、Apply 前後端點與中間驗證安排。違反任一項即 Abort。
7. **Apply 關卡**：產生含 CH、Sensor、Profile、舊／新點、預估修正比例的確認摘要；只有輸入明確字串 `APPLY CHn PROFILE p` 才送 `0x4702=4`。輸入其他文字則送 Abort `0x4702=5`，保留 CSV 預覽，不改 NVM。
8. **讀回驗證**：確認 command error=0、Factory calibrated=1、校正槽低／高點符合預覽、storage error=0。完成後 CALYS 再輸出至少三個驗證點（低、中、高），各點都等待新成功樣本並記錄 ADC µV、PV、input error；另產生校正報告。
9. **收尾**：任何例外、Ctrl+C、通訊逾時或穩定超時皆嘗試 Abort、停止 CALYS 輸出或回到安全狀態（依 CALYS 指令支援情況），寫明 CSV 階段與錯誤。不得把中斷當成功；LOC 仍須保證執行。

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

交付時更新選單 2、`useMethod.md`、`FactoryCalibrationSlots.md`，CSV 增加校正階段、Profile target、measured zero/span、修正比例、穩定統計、Apply 結果及驗證點欄位。選單原有 TC／mV Gain 子選項需改成韌體 Profile 對映，避免把 mV Gain 16 或沒有對應 Profile 的假設放進工具。

## 尚待驗證項目

- CALYS 1500 每個 Profile 對應輸出模式、可輸出端點與端子接線。
- 不同 Sensor type 共用 Profile 時，CALYS 的 stimulus 及 DUT Sensor type 是否能確實選到相同 ADC 前端配置；每組要透過 trace 與有效樣本確認。
- 韌體目前 `FactoryCalibrationService_UpdateLiveMicrovolts()` 接收 `AnalogInputService` 的原始 `sample.microvolts`；因此重校不會疊加舊係數。仍須以 host test 和目標板確認每個 Profile 選擇與 live µV 都對應同一個 ADC 裝置／輸入。
- 目標板上每個 Profile 的 stabilizing dwell、µV 門檻與合理 gain correction bounds。

在以上事項確認前，自動校正只做 Dry-run 和預覽；不對所有 Profile 一次性開放 Apply。
