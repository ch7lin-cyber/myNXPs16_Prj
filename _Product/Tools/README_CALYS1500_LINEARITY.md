# CALYS 1500 / DUT 獨立線性度測試

程式：`calys1500_linearity_test.py`，與原本 `calys1500_loop_test.py` 分離，不更動十點測試與校正流程。

## 功能完成狀態（2026-11-07）

狀態定義：
- **已實作／待驗證**：GitHub 程式已有對應邏輯，但尚未完成自動化測試或 CALYS/DUT 實機確認。
- **部分完成**：已有部分機制，仍有功能或相容性缺口。
- **尚未完成**：尚無符合需求的完整操作流程。
- **已完成（程式修改）**：原始碼及選單調整已提交；不代表實機測試通過。

| 功能 | 狀態 | 備註／驗證重點 |
|---|---|---|
| 獨立 `calys1500_linearity_test.py` | 已完成（程式修改） | 與原十點測試程式分離 |
| [1.1.1] PV Address／[1.1.2] Sensor Address | 已實作／待驗證 | 預設 1000h／1004h；1004h 的實際用途未核對 |
| [1.1.3] Sensor Type 手動數字／CALYS 編碼連動 | 已實作／待驗證 | 預設手動 0x0001；連動只採用 CALYS code |
| [1.1.4] 讀 PV 測試 | 已實作／待驗證 | 需確認 DUT Modbus 回覆與資料格式 |
| [1.1.5] INT16／INT32／IEEE754 Float32 | 已實作／待驗證 | 1／2 Register 解碼；需增加單元測試 |
| [1.1.6] ABCD／CDAB Word Order | 已實作／待驗證 | 僅適用 32-bit |
| [1.1.7] PV Scale | 已實作／待驗證 | 預設 1.0；需測正負值及異常值 |
| [1.1.8] Sensor Write／Apply | 部分完成 | 預設不寫入；寫入後有讀回，但 Apply 位置／生效結果須依 DUT 韌體核對 |
| [1.1.9] DUT Port／Slave | 已實作／待驗證 | 需實機連線 |
| [1.1] 設定直接顯示於主選單 | 已完成（程式修改） | 直接輸入 1.1.1～1.1.9 操作 |
| [1.2.1] TC／RTD Sensor Type 選擇 | 已實作／待驗證 | 保留原 CALYS 編碼，排除未驗證對映 |
| [1.2.2] CJC DIS／INT／FIX 設定 | 部分完成 | 有讀回／核對程式，但 SCPI 回覆格式未經實機驗證 |
| [1.2.3] 起點／終點及完整量程預設 | 已實作／待驗證 | 範圍依 Sensor low/high |
| [1.3.1] 升溫步階 0.1°C | 已實作／待驗證 | Decimal 避免浮點累積誤差 |
| [1.3.2] PV 取樣間隔 0.5 秒 | 已實作／待驗證 | 通訊延遲可能造成超時 |
| [1.3.3] 每階取樣 2 次 | 已實作／待驗證 | 需核對取樣數與時序 |
| [1.3.4] 每階穩定等待 1 秒 | 已實作／待驗證 | 固定等待，尚未依 PV 穩定程度自動判斷 |
| [1.3] 設定直接顯示於主選單 | 已完成（程式修改） | 直接輸入 1.3.1～1.3.4 操作 |
| [1.4] 測試前檢查及 START 確認 | 已實作／待驗證 | CALYS／DUT 讀回及寫入確認流程 |\n| [1.4] 60 點／約 60 秒預測試與獨立 CSV | 已實作／待驗證 | 全量程含端點 60 點；若通訊延遲則超過 60 秒，不跳點 |\n| [1.4] 預測試成功後人工確認再進完整測試 | 已實作／待驗證 | 60 筆無通訊錯誤才允許二次 START；精度合理性仍須人工判讀 |
| 測試階數／預估時間／動態剩餘時間 | 已實作／待驗證 | ETA 為估算，需長時間測試核對 |
| 逐階設定 CALYS、等待、讀 PV | 已實作／待驗證 | CALYS SCPI 實機回覆尚未確認 |
| CSV 即時寫入及日期／流水號／Sensor 檔名 | 已實作／待驗證 | 需檢查異常及重複執行 |
| CSV 設定值／DUT PV／差值／狀態 | 已實作／待驗證 | `calys_actual` 為 N/A，不假裝是實際量測 |
| 每階平均及最大正／負／絕對誤差摘要 | 已實作／待驗證 | 需測空資料、部分失敗及中止 |
| Ctrl+C 中止保留 CSV／摘要 | 已實作／待驗證 | 需確認不同中止時點 |
| 中止後互動式安全輸出選擇 | 尚未完成 | 目前預設保留最後設定值，未提供互動選擇 |
| 完整 CALYS 狀態讀回與核對 | 部分完成 | 已有 SCPI 查詢及比較，但尚未經儀器驗證 |
| Mock CALYS／Mock DUT 自動化測試 | 尚未完成 | 需涵蓋正常、逾時、異常、中止、CSV |
| CALYS 1500／DUT 實機短量程與全量程測試 | 待驗證 | 建議先 0～10°C、步階 1°C |

**整體結論：主要程式流程與選單已提交，但尚未完成全面驗證；不可標示為實機驗證通過或正式驗收完成。**

## [0] 測試設定檔管理（2026-11-10）

主選單新增：

```text
[0] 測試設定檔管理
  [0.1] 載入設定檔 (JSON)
  [0.2] 恢復系統預設值
  [0.3] 儲存目前設定 (JSON)
  [0.4] 顯示目前設定
```

預設設定檔：`_Product/Tools/calys1500_linearity_settings.json`（相對於程式所在目錄）。可在輸入提示指定其他 JSON 路徑。

- **0.1 載入**：要求 JSON `version=1`、完整欄位及正確資料型態；檢查 Sensor code、PV 型態、Word Order、位址、COM、Slave、CJC、範圍、步階、取樣參數。全部成功才替換記憶體設定；錯誤則保留原設定。**不連線、不寫入 DUT、不改 CALYS 輸出**。
- **0.2 恢復預設**：需輸入 `RESET`；只恢復程式內預設值，不覆寫 JSON，也不操作硬體。若先前有命令列 COM 設定，重設後也會恢復 Config 的 COM 預設值。
- **0.3 儲存**：先驗證目前設定，再寫入 JSON；檔案已存在時須輸入 `OVERWRITE`。包含 DUT、CALYS、PV 解碼、Sensor、CJC、起訖溫度、取樣及輸出設定，不包含測試 CSV 資料。
- **0.4 顯示**：以 JSON 格式列出目前記憶體設定。
- 可直接從主選單輸入 `0.1`～`0.4`，也可先輸入 `0` 進入設定檔子選單。

**狀態：程式已實作，尚待 Mock 測試與實機操作驗證。** 設定檔載入只代表欄位合法，不代表 DUT Register Map、CALYS SCPI 或硬體接線已確認。開始測試前仍需 [1.4] 的測試前檢查。

## DUT Sensor Type 立即寫入（2026-11-10）

- `[1.1.3]`：選擇 Sensor Type 後立即以 **Modbus FC06** 寫入 `Sensor Address`，再 FC03 讀回確認；無 Apply。
- `[1.1.4]`：選擇 Sensor Type、輸入 Apply 值（預設 `A5A5h`），顯示寫入計畫並要求 `1` 確認。以 **Modbus FC10** 一次寫入 `Sensor Address`、原 CJC Source、Apply，保留 CJC 原值，之後讀回 Sensor Type。輸入 `0` 或其他內容取消。
- `[1.1.8]`：原本的讀 PV 測試移至此項。
- 預設 Sensor Address 改為 **1007h**（CH0，沿用十點測試工具）；CHn 通常為 `1007h + CH×10h`，使用前應確認實際 Register Map。
- `[1.4]`：**不再寫入 Sensor Type／Apply**，只讀回 DUT Sensor Type 並與目前設定比對；不一致則阻止測試。
- 既有 JSON 欄位 `write_sensor`／`apply_address` 暫為向下相容保留欄位，不再控制 [1.4] 寫入；新設定檔應使用 Sensor Address 1007h 或實際 DUT 位址。
- **狀態：已實作，尚待 DUT 實機驗證。** FC03 讀回一致不代表量測鏈已完成切換，仍需確認新 PV 樣本及實際量測。

## CALYS 1500 實機回覆修正（2026-11-10）

實機記錄（CALYS1500 1507D I03 0148 A）確認：

| 查詢 | 回覆 | 處理 |
|---|---|---|
| `SOUR:FUNC?` | `TCOUPLE` | 正規化為 TC |
| `SOUR:TC:TYPE?` | `K` | 用於 Sensor Type 核對 |
| `SOUR:TC:RJUN:TYPE?` | `DISABLED` | 正規化為 DIS |
| `SOUR:TC?` | 逾時、無回覆 | 已停止使用，不再阻擋輸出設定 |

`verify_output()` 現僅透過 `ERR?` 確認 SCPI 沒有回報錯誤，**不是 CALYS 設定值讀回，也不是實際輸出量測**。測試 CSV 的 `calys_actual` 仍為 `N/A`。設定值查詢替代指令尚待原廠手冊或實機驗證。此修正尚未完成完整實機回歸測試。

## [1.2.1] Sensor Type 立即套用修正（2026-11-10）

選擇 TC／RTD 及 Sensor code 後，立即向 CALYS 發送 `configure()` 指令並讀回核對；**只有設定成功才更新主選單的 Sensor 與預設量程**。先以 `SOUR:FUNC?` 讀取目前功能，不要求它與準備選擇的 Sensor 相同。新增 `TCOUPLE → TC` 等 SCPI Function 名稱正規化，避免把儀器的 `TCOUPLE` 誤判成不相容。

**待實機驗證：** CALYS 韌體是否支援 `SOUR:RTD:TYPE?`、`SOUR:RTD?` 等狀態查詢；若查詢失敗，程式不會宣稱已完成核對，需依實際 TX/RX 及儀器手冊調整。

## [1.2.4] 輸出值與 [1.2.5] 自動通訊測試（2026-11-10）

- `1.2.4`：連接 CALYS，讀取目前 Function／Sensor／CJC／設定值，輸入輸出值並以 `OUTPUT` 確認；重新設定所選 Sensor/CJC、核對讀回後設定輸出，最後查詢輸出設定值。**會改變 CALYS 實際輸出，請先確認接線及負載安全**；結束後 LOC 不會自動歸零。
- `1.2.5`：只進行 CALYS 身分、目前 Function 及 `ERR?` 查詢，不設定輸出或改變 DUT。
- `--calys-timeout 10`：CALYS SCPI 查詢逾時（秒），預設 10 秒，與 DUT timeout 分開。
- `--calys-trace`：列印 CALYS TX／RX，方便診斷通訊。
- 上述兩個參數亦納入 JSON 設定檔 `calys_timeout`／`calys_trace`，載入仍不連線或操作硬體。舊版 JSON 缺少新欄位時，需重新儲存新版完整設定檔。
- **狀態：程式已實作，待 CALYS 1500 實機驗證 SCPI 查詢格式及設定值讀回。**

啟動範例：

```powershell
python _Product\\Tools\\calys1500_linearity_test.py --calys-port COM20 --dut-port COM17 --calys-timeout 10 --calys-trace
```

## 啟動（專案根目錄）

```powershell
python -m pip install pyserial
python _Product\Tools\calys1500_linearity_test.py --calys-port COM20 --dut-port COM17 --results calys_results
```

## 使用流程

1. [1.1] 設定 DUT PV 位址（預設 1000h）、Sensor Type 位址（預設 1004h）、Sensor 數字（預設 0x0001）。
2. 在主選單 [1.1.5]～[1.1.7] 可設定 PV 解碼為 Signed INT16（1 register）、Signed INT32（2 registers）或 IEEE754 Float32（2 registers，預設）；32-bit 可選 ABCD 或 CDAB，另有 Scale（預設 1.0）。[1.1.4] 可先測讀 PV。
3. DUT Sensor Type 預設為手動數字，亦可選擇沿用 CALYS Sensor code。**預設不寫入 DUT Sensor Type**；需在 [1.1.8] 顯式啟用並確認。Apply 位址由使用者指定，未設定不會自行推測。
4. [1.2.1] 選擇 TC/RTD，再從已驗證 CALYS Sensor 表選型別；切換後溫度範圍重設為該 Sensor 的 low/high。[1.2.2] 設定 CJC DIS/INT/FIX 並讀回核對；[1.2.3] 可縮小起訖溫度以進行短測。
5. [1.3] 設定步階（0.1°C）、取樣間隔（0.5 秒）、每階筆數（2）、穩定等待（1 秒）。
6. [1.4] 進行測試前檢查：CALYS 身分、Function/Sensor/CJC 讀回、DUT PV 解碼、可選 Sensor 寫入及 Apply 確認；顯示總階數、總樣本與最低預估時間。輸入 START 才開始。
7. 每階設定 CALYS、核對設定值回讀、等待穩定、讀取 DUT PV。逐筆 flush CSV，顯示已耗時與動態預估剩餘時間。Ctrl+C 會保存部分結果與摘要。

## [1.4] 兩階段測試（2026-11-07 更新）

1. 測試前檢查 CALYS Sensor/CJC、DUT PV 讀回與選用的 Sensor 寫入設定，輸入 `START` 後執行 **60 點預測試**。
2. 將起點到終點含端點平均分為 60 點，每點設定 CALYS、核對設定值並讀取 DUT PV 一次；以每點約 1 秒為目標，**60 秒不是硬性上限**。若通訊較慢，依實際時間完成，不跳點。
3. 預測試獨立輸出 `*_pretest.csv` 與 `*_pretest_summary.csv`，每筆即時 flush；包含設定值、DUT PV、差值與通訊狀態。
4. 若預測試不足 60 筆或有任何通訊／讀值錯誤，**禁止進入完整測試**。若 60 筆讀取成功，仍需操作員檢視資料，輸入第二次 `START` 才開始完整測試；其他輸入即取消。
5. 完整測試依 [1.3] 的小步階、取樣間隔、每階取樣數及穩定等待執行，獨立輸出 `*_full.csv` 與 `*_full_summary.csv`。Ctrl+C 保留已記錄資料。

**驗證限制：** 預測試的自動通過條件僅是 60 筆通訊與 PV 解碼成功，不代表精度、線性度或穩定度合格；數值合理性由操作員檢視。CALYS 設定值讀回不是獨立量測的實際輸出。預測試預設不使用完整測試的穩定等待時間，快速掃描可能受到 DUT 響應延遲影響。

## CSV

原始 CSV 檔名：`YYYYMMDD_HHMMSS_流水號_TC-K.csv`。

欄位：`timestamp`、`step_index`、`sample_index`、`calys_setpoint`、`calys_actual`、`dut_pv`、`delta`、`sensor_type`、`dut_sensor_type`、`read_status`、`error`。

`delta = dut_pv - calys_setpoint`；`calys_actual` 固定為 `N/A`，因為 SCPI 設定值回讀不等於獨立量測的實際輸出。每階 `_summary.csv` 提供平均 PV、平均誤差、最大正／負誤差及最大絕對誤差。

## 注意事項與實機驗證

- 目前 `calys1500_protocol.py` 新增的 SCPI 狀態讀回查詢尚未經實機確認；若儀器不支援 `SOUR:FUNC?`、`SOUR:TC:TYPE?`、`SOUR:TC:RJUN:TYPE?` 或輸出設定值查詢，會停止測試，不應繞過核對。
- **1004h 尚未確認為目前 S16 韌體 Sensor Type 位址**；既有十點測試使用 `1007h + CH×10h`，本工具預設不自動寫入 1004h。
- 中止後 CALYS 預設保留最後輸出值，解除 REM/LOC；請確認接線安全。程式提供內部 `safe_output="zero"` 設定，但零度不一定適用所有 Sensor（例如 B 型低限 100°C），不應視為通用安全狀態。
- 推薦先將起訖溫度改成 0～10°C、步階 1°C、每階 2 筆，完成短測再使用完整量程。
- 取樣間隔為目標排程，Modbus/SCPI 實際通訊可能使間隔超時；CSV timestamp 是實際記錄時間。
- CSV/摘要為測試結果，不包含合格公差與自動 PASS/FAIL。
