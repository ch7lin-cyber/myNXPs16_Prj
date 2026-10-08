# CALYS 1500 與 DUT 十點迴路測試

第一階段實作 CALYS 遠端輸出、DUT Sensor type 設定、十點讀回及 CSV。
不執行合格判定，不修改零點／跨度或 Factory NVM。

## 執行

Python 3.10 以上，安裝串列通訊套件：

```sh
python -m pip install pyserial
python _Product/Tools/calys1500_loop_test.py --calys-port COM12 --dut-port COM8
```

COM12、COM8 是範例，請依 Windows 裝置管理員修改。
CALYS 固定 115200、8N1、無流量控制；DUT 預設 Modbus RTU、115200、8N1、slave 2。
DUT 可用 `--dut-baud`、`--slave` 修改。兩台設備必須使用不同 port。
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
| 0.1.2 | 修改 DUT port |
| 0.2 | 選 CALYS Sensor type 並設定輸出模式 |
| 0.3 | 設定輸出值，接受正負及小數；依 Sensor 規格檢查範圍 |
| 1 | 先選 CH0～CH3，再選韌體 Sensor type |
| 1.1.0 | 對選定通道／Sensor 設定 DUT，執行十點輸出／讀回 |
| 2 | 列出所有校正預留入口 |
| 2.1.0～2.1.3 | TC／mV Gain 16、32、64、128，僅提示未實作 |
| 2.2.0 | 電壓共用 Gain 32，僅提示未實作 |
| 2.3.0 | 電流共用 Gain **64**，依現有韌體，僅提示未實作 |
| 2.4.0～2.4.3 | RTD Gain 1、8、16、32，僅提示未實作 |
| q | 結束、送 LOC 解除 CALYS 遠端控制 |

0.2 只設定 CALYS；1.1.0 才會寫 DUT Sensor type。
校正選項不開串列 port、不送校正指令，不碰 0x4700／0x4720／0x4C00 等 Factory registers。

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
DUT 設定使用 FC10 寫 `0x1007 + CH*0x10`：Sensor、reserved=0、Apply=0xA5A5。
設定後讀回 Sensor；此讀回確認事件已排入，並非 HAL 套用完成證明。
逐點新成功樣本確認用來排除沒有持續採樣的狀態；通道硬體對映仍需實際接線驗證。

CSV 保存時間、通道、Sensor、Gain、輸出設定值及單位、PV、input error、RAW、
ADC 微伏值、成功樣本數、儀器身分、冷端／供電設定、等待時間與流程狀態。
`recorded` 只表示讀回已完成，**不代表 OK**；不計算公差、不做 OK／NG。
電壓／電流／mV 的 PV 是韌體工程量，不能直接將其與 V／mA／mV 設定值相減。
各 Modbus 區塊分次讀取，並非同一瞬間的原子快照；RAW／微伏與 PV 可能差一個採樣週期。
結果檔預設 `calys_results/`，每個測試點立即 flush。
Ctrl+C 或通訊錯誤停止流程，保存已完成點及失敗點，方便重測。

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
模擬 DUT 設定及讀回、CSV 中斷保存、新樣本逾時、校正入口無寫入。
尚未在實體 CALYS 1500／DUT 上驗證；首次以選單 0.1.0、0.2、0.3 打通，再跑 1.1.0。
