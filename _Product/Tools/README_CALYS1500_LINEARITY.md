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
| [1.4] 測試前檢查及 START 確認 | 已實作／待驗證 | CALYS／DUT 讀回及寫入確認流程 |
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
