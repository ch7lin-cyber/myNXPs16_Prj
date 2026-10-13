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

銅線測試且 CALYS CJC 停用時，K 型 0／250／1100°C 的標準輸出電勢約為 0／10.153／45.119 mV。先比較 ADC Microvolts；DUT source=1 若仍加入 CJC，不會直接等於 CALYS 設定溫度。這是功能核對參考，精度偏差交由校正流程處理。

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
    [0.1.0] 自動通訊測試
    [0.1.1] 改 CALYS port
  [0.2] 改 DUT port
  [0.3] 改 CALYS Sensor type
  [0.4] 設定輸出值

[1] 選通道／Sensor（直接寫 DUT）
    [1.1.0] 十點自動測試

[2] 自動校正（依接線類別；預設只預覽）／選通道
  [2.1] 電壓／mV
  [2.2] RTD 三線
  [2.3] 熱電偶 TC
  [2.4] 電流
q 結束
```

- `1`：選通道及 Sensor，立即對 DUT 用 FC10 寫入 Sensor type、目前 CJC source 及 `0xA5A5` Apply Key；讀回正確才更新選擇。讀回不是 ADC 硬體套用完成的證明，請用診斷確認。
- `0.2`：修改 DUT port。
- `0.3`：只改 CALYS Sensor type；不寫 DUT。
- `0.4`：設定 CALYS 輸出，並重新套用本次啟動的 CJC 模式；不寫 DUT。
- `1.1.0`：重新設定 CALYS 及 DUT，執行十點測試，逐點等待新成功 ADC 樣本，結果寫入 CSV。
- `2`：選 CH，再以 `[2.1]`～`[2.4]` 選接線類別，並以 `[P0]`、`[P1]` 等 ID 選擇類別內的唯一韌體 Profile 與代表 Sensor。P0 必須從 `[2.1]` 選 `0-50mV`，工具固定輸出 `0 mV／50 mV`；禁止以 TC 溫度端點取代電氣校正點。工具等待每點 5 筆新鮮穩定 ADC 樣本，請韌體擷取零點／跨度，顯示 Profile target 與增益修正比例並保存 JSON。預設只預覽並 Abort，不寫 NVM。
- 要允許寫入 NVM，需另外以 `--allow-calibration-apply` 啟動；預覽後還要逐筆輸入 `APPLY_CHn_PROFILE_p`。Apply 命令不重送，結果會讀回 Profile 校正槽驗證。若來源值、CALYS/DUT 端子和參考標準尚未驗證，請勿開啟此參數。低／高來源值要依該 Profile 的韌體 target µV 及已驗證標準輸入；除 P0 固定點外，程式目前不推測或換算未驗證的來源值。
- 初步分組：TC Profile 0/1/3/7、RTD Profile 2/5/8/9、電壓／mV Profile 0/4、電流 Profile 6。TC 與電壓／mV 共用 Profile 0，執行期間同槽只處理一次。物理端子文字仍須依現場接線圖確認。
- 穩定判定預設每點 5 筆、最大範圍 20 µV、逾時 30 秒；可用 `--calibration-timeout`、`--calibration-stability-uv` 調整。校正 JSON 存入 `--results`。
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


## 5. 十點測試結果與帶正負號的差值

選單 `1.1.0` 執行完成後，會在終端顯示逐點結果表，並輸出：

- 原本的 `.csv`：加入所有比較欄位，每個點立即保存。
- 同名 `_summary.txt`：十點結果表、最小／最大差值、最大絕對差；失敗或中止時也保存已取得結果。

差值一律定義為 **實讀 − 預期**：正號表示偏高，負號表示偏低。

| 比較 | 輸出方式 |
|---|---|
| TC／RTD 溫度 | CALYS 設定 degC、DUT PV degC、帶正負號的溫度差 |
| 電壓／電流 | V／mV／mA 各自標示單位；保留原始工程 PV，另依韌體的比例換回物理單位比較 |
| ADC 電壓 | 預期 ADC uV、實讀 ADC uV、帶正負號的 ADC 差值 |

CSV 新欄位：`pv_signal`、`pv_signal_unit`、`expected_pv`、`pv_delta`、`pv_native_delta`、`expected_adc_uv`、`adc_delta_uv`、`adc_reference_basis`、`calys_cjc_temperature`。

`pv` 保留原始 Modbus Float32，不改既有欄位意義；`pv_signal` 是用來與設定值比較的物理訊號。類比電壓／電流的 PV 線性對映為 −32000～32000，直接使用 `SensorConversionService.c` 的工程值，不除以 10。各 Sensor 的下限對應 −32000、上限對應 32000，例如 4～20 mA 的 4 mA 對應 −32000、20 mA 對應 32000；`pv_signal` 再換算回 mV／mA／V，以比較實際訊號誤差。溫度 Sensor 的 PV 仍使用 °C。

### ADC 理論值依據

- K 型：NIST ITS-90 正向電勢公式。
- Pt100／Pt1000：IEC 60751 Callendar–Van Dusen 電阻公式。
- 其他 TC／RTD：固定版本 PlatformCore 的分段曲線反算，報告明確標示為近似參考；不可當成獨立的高精度標準。來源：`platForm@5c8584ddead2320f727881cf96295cde51f1eb43`。
- 0～50 mV：直接輸入，mV × 1000＝ADC uV。
- 0～5／10 V：輸入電壓 × 6200／1026200，再轉 uV。
- mA：輸入 mA × 1000 × 49.9 Ω × 36800／1056800＝ADC uV。
- RTD：R × I，Pt1000 為 250 uA，其餘 RTD 為 500 uA。此值是目前韌體對外提供的等效微伏值，不是 REFIN 實測電壓。

前端參數依 `ProductAdcConfig.h`，變更硬體分壓／shunt／激勵時須同步調整理論模型。

### TC 的 CJC 對比較結果的影響

`--cjc DIS`：預期 ADC 電壓為標準 E(設定溫度)。DUT 來源為 1 時，PV 仍含 DUT CJC，因此直接溫度差也包含該補償，ADC 比較可獨立觀察前端電壓偏差。

`--cjc FIX --fixed-cjc 25`：預期 ADC 電壓為 E(設定溫度) − E(25°C)。

`--cjc INT`：若未提供 CALYS 冷端溫度，預期 ADC 與 ADC 差值會顯示 `N/A`，附原因；不會假設 INT 是 0°C。可將 CALYS 面板顯示的冷端溫度作為本次測試的固定參考傳入，例如：

```powershell
python D:\Python\pyTestCode\NXP\myNXPs16_Prj\_Product\Tools\calys1500_loop_test.py --calys-port COM20 --dut-port COM8 --cjc INT --calys-cjc-temperature 26.5
```

此新參數只供結果計算，不修改 CALYS 或 DUT 的 CJC。若測試期間 CALYS 冷端溫度漂移，固定參考也會產生差異。

DUT `input_error != 61` 的測試點仍保留讀值，但標示為輸入異常，不納入有效點差值統計。沒有設定合格公差，因此不自動判定精度 PASS／FAIL，亦不寫入校正值。PV 與 ADC 分別讀取，並非同一瞬間的原子快照，請讓輸出充分穩定。

更新時請同時取得 `calys1500_loop_test.py` 與新增的 `calys1500_expected.py`，放在同一個 Tools 目錄。

參考：
- https://nvlpubs.nist.gov/nistpubs/Legacy/MONO/nistmonograph175.pdf
- https://www.ti.com/lit/pdf/sbaa275


## DUT CJC 來源選擇

DUT 新增每通道來源：0 關閉、1 內部 TMP20（預設，CH0/1→CJC_0，CH2/3→CJC_1）、2/3 外部入口預留。位址為 0x1008＋CH×0x10，使用 Pending／Apply。Python 切 Sensor 時先讀來源並保留；不會因選 Sensor 而改變補償模式。

銅線十點測試需同時設 CALYS `--cjc DIS` 與 DUT source=0；恢復感測器量測時設 DUT source=1。外部模式尚無資料來源，選到會回 CJC 無效 (9)，不使用 TMP20 替代。

操作、NVM 相容性與驗證狀態見 [CjcSourceSelection.md](../Docs/CjcSourceSelection.md).
