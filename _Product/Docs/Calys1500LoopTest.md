# CALYS 1500 與 DUT 十點迴路測試

目前實作 CALYS 遠端輸出、DUT Sensor type 直接寫入、十點讀回、帶正負號的差值，以及 CSV／文字結果摘要。
不執行合格判定，不修改零點／跨度或 Factory NVM。

## 執行

Python 3.10 以上，安裝串列通訊套件：

```sh
python -m pip install pyserial
python _Product/Tools/calys1500_loop_test.py --calys-port COM20 --dut-port COM8 --calys-write-delay 0.3 --calys-timeout 10 --calys-trace --cjc DIS
```

目前測試使用 CALYS COM20、DUT COM8（RS-485 轉接器）；請依實際裝置修改。
`--cjc DIS` 適用目前的銅線電勢測試，只關閉 CALYS TC 輸出端 CJC，不會關閉 DUT CJC。
完整 Windows 啟動指令及兩支工具的參數見 [Tools/useMethod.md](../Tools/useMethod.md)。
請將 `calys1500_loop_test.py`、`calys1500_protocol.py`、`calys1500_expected.py` 與 `modbus_rtu_test_menu.py` 放在同一個 Tools 目錄。
CALYS 固定 115200、8N1、無流量控制；DUT 預設 Modbus RTU、115200、8N1、slave 2。
DUT 可用 `--dut-baud`、`--slave` 修改。兩台設備必須使用不同 port。
DUT 回應 timeout 預設 2 秒（`--dut-timeout`）；CALYS 工具每次 DUT 呼叫前預設等待 50 ms
（`--dut-request-delay`），共用 RTU client 另固定等待 50 ms，因此預設合計約 100 ms。讀取若遇到 timeout、不完整封包或 CRC 錯誤，
最多重試 2 次（`--dut-read-retries`），重試前至少等 150 ms。
有效的 Modbus 例外回應不重試；Sensor／Apply 寫入不自動重送。
選單修改 port 後保存至 `calys1500_ports.json`，下次啟動沿用；命令列參數優先。
只列出對映、不需要 pyserial 或硬體：

```sh
python _Product/Tools/calys1500_loop_test.py --list-sensors
```

## 選單

| 選項 | 功能 |
|---|---|
| 0 | CALYS 子選單提示 |
| 0.1.0 | 指定 port 自動連線、辨識 CALYS 1500、查詢通訊錯誤 |
| 0.1.1 | 列出可用 ports，修改 CALYS port |
| 0.2 | 修改 DUT port |
| 0.3 | 選 CALYS Sensor type 並設定輸出模式 |
| 0.4 | 設定輸出值，接受正負及小數；依 Sensor 規格檢查範圍 |
| 1 | 選 CH0～CH3／Sensor 後立即寫 DUT 與 Apply Key，讀回確認成功才更新選擇 |
| 1.1.0 | 對選定通道／Sensor 設定 DUT，執行十點輸出／讀回 |
| 2 | 列出所有校正預留入口 |
| 2.1.0～2.1.3 | TC／mV Gain 16、32、64、128，僅提示未實作 |
| 2.2.0 | 電壓共用 Gain 32，僅提示未實作 |
| 2.3.0 | 電流共用 Gain **64**，依現有韌體，僅提示未實作 |
| 2.4.0～2.4.3 | RTD Gain 1、8、16、32，僅提示未實作 |
| q | 結束、送 LOC 解除 CALYS 遠端控制 |

`1` 現在直接寫 DUT；`0.3`／`0.4` 只設定 CALYS，不寫 DUT。
`1.1.0` 開始前仍會重新設定 CALYS 與 DUT。寫入或讀回失敗不會將候選通道／Sensor 標示為成功。
DUT Sensor type 讀回不代表 ADC 已套用完成，請查看 ADC configuration trace 與成功樣本。

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
校正先選 CH，再輸入畫面標示的 `[2.1]`～`[2.4]` 接線類別，接著以 `[P0]`、`[P1]` 等 ID 選該類別的一個代表 Sensor／Profile。Profile 0 必須從 `[2.1]` 選擇 `0-50mV`，並固定使用 `0 mV／50 mV` 電氣標準點；TC 溫度輸出只能做量程驗證，不能覆蓋 P0 校正。同一 Profile 可由操作員主動重校。工具從韌體讀目標 µV 和校正槽，不複製另一份目標常數。

預設是預覽模式：送 CALYS 低／高點、檢查 5 筆新鮮穩定 Raw ADC 樣本、由韌體擷取 pending 點、檢查跨度並輸出 JSON 記錄，最後 Abort，不寫 NVM。重校時不以舊校正造成的 PV Input Error 否決健康 Raw ADC。除 P0 固定點外，低／高 CALYS 設定值由操作人輸入，必須依校正標準和產品接線核對。若產品板上的端子對映、來源值及參考標準已驗證，啟動時加 `--allow-calibration-apply`，並於預覽後輸入 `APPLY_CHn_PROFILE_p` 才會讓韌體寫入。`0x4C00` 校正槽僅讀回，不直接寫入。詳見 [Calys1500AutoCalibrationDesign.md](Calys1500AutoCalibrationDesign.md)。

## 對映與範圍

範圍取自 `product_temperature_range_resolver.c`；Gain 取自現有產品 ADC 設定。

| 代碼 | Sensor | 十點輸出範圍 | Gain | CALYS 對映 |
|---|---|---|---|---|
| 11 | B | 100～1800 °C | 128 | B |
| 15 | C | 0～2300 °C | 64 | C |
| 23 | D_X | 0～2300 °C | 32 | 未驗證 |
| 26 | E | 0～600 °C | 32 | E |
| 46 | J | -200～1200 °C | 16 | J |
| 48 | K | -200～1300 °C | 32 | K |
| 58 | N | -200～1300 °C | 32 | N |
| 62 | OFF_X | 無 | 無 | 不輸出 |
| 80 | R | 0～1700 °C | 64 | R |
| 84 | S | 0～1700 °C | 128 | S |
| 93 | T | -200～400 °C | 64 | T |
| 100 | L | -200～850 °C | 32 | L |
| 101 | U | -200～500 °C | 32 | U |
| 102 | TXK_X | -150～800 °C | 32 | 未驗證 |
| 113 | Pt100 | -200～850 °C | 8 | PT100 |
| 114 | Pt1000 | -200～850 °C | 1 | PT1000 |
| 115 | JPt100_X | -20～400 °C | 16 | 曲線對映未驗證 |
| 116 | Ni120 | -80～300 °C | 8 | NI120 |
| 117 | Cu50 | -50～150 °C | 32 | CU50 |
| 120 | 0～5 V | 0～5 V | 32 | 電壓 |
| 121 | 0～10 V | 0～10 V | 32 | 電壓 |
| 122 | 0～50 mV | 0～50 mV | 32 | 電壓 |
| 123 | 0～20 mA | 0～20 mA | 64 | 電流 |
| 124 | 4～20 mA | 4～20 mA | 64 | 電流 |

`_X` 代表缺少已驗證的直接 SCPI／韌體曲線對映，禁止自動輸出；
不代表已證明 CALYS 硬體沒有該功能。手冊列有不同 Pt100 曲線，但 JPt100 仍需核對。
儀器若因型號／韌體／範圍拒絕設定，ERR? 非零即停止，不跳過錯誤繼續測試。

## 十點與讀回

共 **10 個點，包含最低及最高點**，步距為 `(最高－最低)/9`。
輸出命令保留 Sensor 單位的小數三位，CSV 保存此設定值。
K 型為 -200、-33.333、133.333、300、466.667、633.333、800、966.667、1133.333、1300 °C。

每點輸出通過 ERR? 後，等待 `--dwell` 秒（預設 2 秒），
再確認成功樣本計數增加；最長等待 `--fresh-timeout` 秒（預設 10 秒）。
DUT 設定使用 FC10 寫 `0x1007 + CH*0x10`：Sensor、目前 CJC source、Apply=0xA5A5。
設定後讀回 Sensor；此讀回確認事件已排入，並非 HAL 套用完成證明。
逐點新成功樣本確認用來排除沒有持續採樣的狀態；通道硬體對映仍需實際接線驗證。

CSV 保存時間、通道、Sensor、Gain、輸出設定值及單位、PV、input error、RAW、
ADC 微伏值、成功樣本數、儀器身分、冷端／供電設定、等待時間與流程狀態。
成功樣本數使用該點新樣本輪詢取得的計數，不在 RAW 讀回後重複讀取；
`dut_read_retries` 保存每點通訊重試次數。
`recorded` 表示取得讀值且 input_error=61，**不代表精度合格**；沒有設定合格公差，不做 OK／NG。
`input_error != 61` 時改標示為 `input_error`，保留數值，但不納入有效點差值統計。
電壓／電流／mV 的 PV 是韌體工程量；工具保留原始 PV，另換回物理單位後比較。
各 Modbus 區塊分次讀取，並非同一瞬間的原子快照；RAW／微伏與 PV 可能差一個採樣週期。
結果檔預設 `calys_results/`，每個測試點立即 flush；路徑相對於啟動時的工作目錄。
完成或中止時另外顯示結果表，並保存同名 `_summary.txt`。
Ctrl+C 或通訊錯誤停止流程，保存已完成點及失敗點，方便重測。

## 結果比較與 ADC 理論值
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

## RTU 診斷工具

```powershell
python D:\Python\pyTestCode\NXP\myNXPs16_Prj\_Product\Tools\modbus_rtu_test_menu.py --port COM8 --slave 2 --baud 115200 --timeout 3
```

每筆請求送出前固定等待 50 ms；RTU 工具預設 timeout=1 秒，上例設為 3 秒。
RTU 診斷工具不支援 `--cjc DIS`，不操作 CALYS，也不切換 DUT CJC。
切換工具前先關閉原工具，避免同時使用 COM8。
`response timeout` 是請求未在期限內取得回覆；「等待新成功樣本逾時」則是讀取有回覆但成功計數未增加，兩者需分開判讀。

## 接線與 CALYS 參數

請依 Sensor 重新接線：TC／mV、電壓、電流、RTD 的輸入端子與模式不同。
CH0～CH3 使用修正後的韌體通道對映，不在 Python 再交換 CH0／CH2。
RTD 使用 CALYS 的電阻模擬輸出及相應 DUT 接法，DUT 提供激勵電流。
`CONT,1MA` 是 CALYS 電阻模擬的激勵電流設定範圍，並非強制 DUT 輸出 1 mA。

TC 預設 `--cjc INT`（CALYS 內部冷端）；應使用對應熱電偶線。
可選 `--cjc DIS` 或 `--cjc FIX --fixed-cjc 23.5`，需與實際冷端接法一致。
電流輸出供電可用 `--current-supply ON|OFF`，預設 ON，依實際迴路接法選擇。
CALYS 的使用者管理若啟用，無參數 REM 可能被拒絕；目前工具不保存或提供登入密碼。
串列等待可用 `--calys-timeout` 調整（預設 5 秒）。
每筆指令後預設等待 150 ms，避免儀器尚未完成解析就接到下一筆；
可用 `--calys-write-delay` 修改。開啟 port 後至少等待 300 ms。
身分查詢逾時最多嘗試三次，輸出值設定不會自動重送。
接收會累積分段回應直到 LF，忽略前導空白行，整體仍受 timeout 限制。

若遇到 `*IDN?` 逾時，先使用 0.1.1 確認 CALYS 的 port，
再執行 0.1.0。可以用以下參數查看指令及回應：

```sh
python _Product/Tools/calys1500_loop_test.py --calys-port COM12 --dut-port COM8 --calys-write-delay 0.3 --calys-timeout 10 --calys-trace
```

逾時訊息包含實際 port、格式、timeout 及收到的原始 bytes；
`RX=b''` 代表該次等待完全沒有收到回應，需確認 port、USB 連線及儀器通訊設定。

**中止／結束只送 LOC，CALYS 可能繼續保持最後輸出值，不會自動歸零。**
需要取消輸出時，使用 CALYS 面板控制。

## 後續建議

1. 打通實際迴路後，加入每點多筆取樣、平均值、最大／最小值及上升／下降掃描。
2. 將各 Sensor 的誤差規格與穩定判定獨立設定，再加入 OK／NG。
3. 校正階段先讀 Factory register map 的目標跨度與 profile，不由 Python 複製另一份常數；
   保存後重開 DUT，讀回 NVM 校正係數確認。
4. 校正記錄加入儀器序號／校驗日期、韌體版本、環境溫度；CJC 校正獨立處理。

## 協定依據與驗證

AOIP《CALYS 150 / CALYS 1500 SCPI commands》DE/15/160 V1.3：
https://www.aoip.fr/support/CALYS%20150_SCPI%20commands_V1.3_EN.pdf

使用 LF 結尾、REM／LOC 遠端模式、ERR? 錯誤查詢，以及 SOUR 系列輸出指令。
V／A 值明確送至對應輸出函式，mV／mA 在 Python 先除以 1000，避免隱含量程單位混淆。

```sh
python -m unittest discover -s _Product/Tests -p test_calys1500_loop.py
```

測試包含十點端點／小數、單位換算、非法值、_X 阻擋、SCPI 錯誤與關閉、
指令等待、分段／空白回應、身分查詢重試與逾時診斷、
Modbus 暫時性讀取失敗恢復、例外／寫入不重送、讀取重試上限、
模擬 DUT 設定及讀回、CSV 中斷保存、新樣本逾時、校正入口無寫入。
新增報告功能已以模擬資料檢查溫度／V／mV／mA、差值正負、ADC 理論範圍、CJC 模式與中途失敗保存；完整十點新報告仍待實機驗證。
2026-10-12 的 K 型 0／250／1100°C 單點紀錄已確認切換與有效取樣，微伏小幅差異留待校正；CJC 精度與 RTD 線路／CALYS 模擬仍未完成驗證。


## DUT CJC 來源選擇

DUT 新增每通道來源：0 關閉、1 內部 TMP20（預設，CH0/1→CJC_0，CH2/3→CJC_1）、2/3 外部入口預留。位址為 0x1008＋CH×0x10，使用 Pending／Apply。Python 切 Sensor 時先讀來源並保留；不會因選 Sensor 而改變補償模式。

銅線十點測試需同時設 CALYS `--cjc DIS` 與 DUT source=0；恢復感測器量測時設 DUT source=1。外部模式尚無資料來源，選到會回 CJC 無效 (9)，不使用 TMP20 替代。

操作、NVM 相容性與驗證狀態見 [CjcSourceSelection.md](CjcSourceSelection.md).
