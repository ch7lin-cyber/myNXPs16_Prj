# Python 測試工具使用方法

適用分支：`branch2_ver1`。下列範例使用 CALYS＝COM20、DUT＝COM8、Modbus 站號＝2，DUT 經 RS-485 轉接器連線。

## 1. 安裝依賴

請使用實際執行工具的同一個 Python 安裝 pyserial：

```powershell
python -m pip install pyserial
```

兩支工具及 `calys1500_protocol.py` 請保留在同一個 `_Product\Tools` 目錄。若 `python` 不在 PATH，可改用 Python 執行檔的完整路徑。

## 2. CALYS／DUT 測試工具

### 2.1 銅線測試，關閉 CALYS 冷接點補償

以下是目前使用的完整啟動指令，可直接貼到 Windows CMD 或 PowerShell：

```powershell
python D:\Python\pyTestCode\NXP\myNXPs16_Prj\_Product\Tools\calys1500_loop_test.py --calys-port COM20 --dut-port COM8 --calys-write-delay 0.3 --calys-timeout 10 --calys-trace --cjc DIS
```

| 參數 | 作用 |
|---|---|
| `--calys-port COM20` | CALYS 通訊 port；固定使用 115200／8N1 |
| `--dut-port COM8` | DUT 的 RS-485 port |
| `--calys-write-delay 0.3` | 每筆 CALYS 指令送出後等待 0.3 秒 |
| `--calys-timeout 10` | CALYS 查詢回覆逾時為 10 秒 |
| `--calys-trace` | 顯示 CALYS TX／RX |
| `--cjc DIS` | 在 TC 模式停用 CALYS 輸出端冷接點補償 |

**`--cjc DIS` 只設定 CALYS，不會關閉 DUT 的 CJC。** RTD、電壓與電流模式不使用此 TC 冷端設定。

銅線測試且 CALYS CJC 停用時，K 型 0／250／1100°C 的標準輸出電勢約為 0／10.153／45.119 mV。先比較 ADC Microvolts；DUT PV 若仍加入 CJC，不會直接等於 CALYS 設定溫度。這是功能核對參考，精度偏差交由校正流程處理。

### 2.2 使用 CALYS 內部冷接點補償

將上述指令的 `--cjc DIS` 改成 `--cjc INT`。此值也是未指定 `--cjc` 時的預設值。

以 K 型熱電偶線測完整輸入功能時，可使用 INT；仍需核對接線與 DUT 的 CJC 設定。

### 2.3 固定 CALYS 冷端為 0°C

```powershell
python D:\Python\pyTestCode\NXP\myNXPs16_Prj\_Product\Tools\calys1500_loop_test.py --calys-port COM20 --dut-port COM8 --calys-write-delay 0.3 --calys-timeout 10 --calys-trace --cjc FIX --fixed-cjc 0
```

`--fixed-cjc` 只有 `--cjc FIX` 時會設定 CALYS 固定冷端溫度。

### 2.4 DUT 通訊與十點測試參數

可在啟動指令後追加：

| 參數 | 預設值 | 作用 |
|---|---:|---|
| `--slave` | 2 | DUT Modbus 站號 |
| `--dut-baud` | 115200 | DUT baud rate；格式為 8N1 |
| `--dut-timeout` | 2 秒 | DUT 讀寫回覆逾時 |
| `--dut-read-retries` | 2 | 可重試的讀取通訊錯誤之額外重試次數；不重試寫入／Apply |
| `--dut-request-delay` | 0.05 秒 | CALYS 工具對 DUT 呼叫前的額外等待 |
| `--dwell` | 2 秒 | DUT 設定後等待，以及每個輸出點的等待時間 |
| `--fresh-timeout` | 10 秒 | 等待 DUT 新成功 ADC 樣本的期限 |
| `--results` | calys_results | CSV 輸出資料夾 |
| `--config` | calys1500_ports.json | Port 設定檔 |

共用的 Modbus client 另有固定 50 ms 請求前延遲，因此目前 CALYS 工具預設每筆 DUT 請求前的等待合計約 100 ms。

### 2.5 操作選單

```text
0 CALYS 1500 相關
----0.1.0 自動通訊測試
----0.1.1 改 CALYS port
----0.1.2 改 DUT port
--0.2 改 CALYS Sensor
--0.3 設輸出值
1 選通道／Sensor（直接寫 DUT）
----1.1.0 十點自動測試
2 自動校正（預留）
q 結束
```

- `1`：選通道及 Sensor，立即對 DUT 用 FC10 寫入 Sensor type、保留欄位及 `0xA5A5` Apply Key；讀回正確才更新選擇。讀回不是 ADC 硬體套用完成的證明，請用診斷確認。
- `0.2`：只改 CALYS Sensor；不寫 DUT。
- `0.3`：設定 CALYS 輸出，並重新套用本次啟動的 CJC 模式；不寫 DUT。
- `1.1.0`：重新設定 CALYS 及 DUT，執行十點測試，逐點等待新成功 ADC 樣本，結果寫入 CSV。
- `2`：校正入口仍為預留，尚未寫入校正資料。
- `q`：關閉連線，CALYS 以 LOC 解除遠端控制；輸出值不會自動歸零。

手動在 CALYS 面板改 CSF 後，若工具再次設定 TC Sensor／輸出，會依啟動參數覆寫 CSF。因此要停用補償，請明確加 `--cjc DIS`。

## 3. Modbus RTU 診斷工具

```powershell
python D:\Python\pyTestCode\NXP\myNXPs16_Prj\_Product\Tools\modbus_rtu_test_menu.py --port COM8 --slave 2 --baud 115200 --timeout 3
```

| 參數 | 作用 |
|---|---|
| `--port COM8` | DUT port |
| `--slave 2` | Modbus 站號 |
| `--baud 115200` | Baud rate；格式為 8N1 |
| `--timeout 3` | 回覆逾時 3 秒；未指定時預設 1 秒 |

此工具每筆請求送出前固定等待 **50 ms**。選單 `1` 讀通道 PV 與完整 ADC 診斷；`3` 讀指定 Holding Register；`6` 寫入 Register；`0` 結束。

**這支工具不支援 `--cjc DIS`**，不會設定 CALYS，也不會變更 DUT 的 CJC。加入該參數會出現不認得參數的錯誤。

## 4. 工作目錄與切換工具

`calys_results` 與 `calys1500_ports.json` 的預設位置取決於啟動時的工作目錄，而不是 Python 檔案所在位置。如要固定放在 Tools 下：

```powershell
cd D:\Python\pyTestCode\NXP\myNXPs16_Prj\_Product\Tools
python calys1500_loop_test.py --calys-port COM20 --dut-port COM8 --calys-write-delay 0.3 --calys-timeout 10 --calys-trace --cjc DIS
```

使用 RTU 診斷工具前，先以 `q` 關閉 CALYS 測試工具，避免兩支工具同時存取 COM8。之後再啟動 CALYS 工具繼續測試。
