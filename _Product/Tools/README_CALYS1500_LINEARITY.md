# CALYS 1500 / DUT 獨立線性度測試

程式：`calys1500_linearity_test.py`，與原本 `calys1500_loop_test.py` 分離，不更動十點測試與校正流程。

## 啟動（專案根目錄）

```powershell
python -m pip install pyserial
python _Product\Tools\calys1500_linearity_test.py --calys-port COM20 --dut-port COM17 --results calys_results
```

## 使用流程

1. [1.1] 設定 DUT PV 位址（預設 1000h）、Sensor Type 位址（預設 1004h）、Sensor 數字（預設 0x0001）。
2. 在 [1.1] 子選單可設定 PV 解碼為 Signed INT16（1 register）、Signed INT32（2 registers）或 IEEE754 Float32（2 registers，預設）；32-bit 可選 ABCD 或 CDAB，另有 Scale（預設 1.0）。[1.1.4] 可先測讀 PV。
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
