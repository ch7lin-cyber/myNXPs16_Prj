# DTX 相容暫存器 0x4908～0x491F

## 摘要

本區支援 FC03／04 讀取與 FC06／10 寫入。寫入直接更新指定的 Active 欄位，
不需 Apply Key，也不套用其他欄位原有的 Pending 設定。既有標準位址的
Pending／Apply 規則保持不變。資料不額外寫入 NVM。

PV／DI 寫入只暫時改變 Modbus 監測值，下一次成功量測會覆蓋；DI 不改變實體輸入
或 DigitalInputService 內的取樣狀態。PWM／DAC／DO 則透過現有輸出 service 操作硬體，
原有安全抑制仍有效，PWM 週期切換時機依原 updateMode。

## 位址與格式

| 相容位址 | 用途 | 原位址 | 格式／範圍 |
|---|---|---|---|
| 0x4908 | CH0 PV | 0x1005～1006 | int16，0.1 單位 |
| 0x4909 | CH1 PV | 0x1015～1016 | int16，0.1 單位 |
| 0x490A | CH2 PV | 0x1025～1026 | int16，0.1 單位 |
| 0x490B | CH3 PV | 0x1035～1036 | int16，0.1 單位 |
| 0x490C | DI 狀態 | 0x1500 | bit0～3 |
| 0x490D～490F | 保留 RAM | 無 | uint16，可讀寫，重開機歸零 |
| 0x4910 | CH0 MV（PWM＋DAC） | 0x1301／0x1400 | 0～1000，即 0.0～100.0% |
| 0x4911 | CH1 MV（PWM＋DAC） | 0x1304／0x1401 | 0～1000 |
| 0x4912 | CH2 MV（PWM＋DAC） | 0x1307／0x1402 | 0～1000 |
| 0x4913 | CH3 MV（PWM＋DAC） | 0x130A／0x1403 | 0～1000 |
| 0x4914 | DO 狀態 | 0x1510 | bit0～3 |
| 0x4915～4917 | 保留 RAM | 無 | uint16，可讀寫，重開機歸零 |
| 0x4918 | CH0 PWM period | 0x1300 | 10～10000 ms |
| 0x4919 | CH1 PWM period | 0x1303 | 10～10000 ms |
| 0x491A | CH2 PWM period | 0x1306 | 10～10000 ms |
| 0x491B | CH3 PWM period | 0x1309 | 10～10000 ms |
| 0x491C～491F | 保留 RAM | 無 | uint16，可讀寫，重開機歸零 |

例如寫入 PV=1234，標準 float32 PV 讀回 123.4；寫入 0xFB2E（int16=-1234），
float32 PV 讀回 -123.4。讀取時四捨五入至 0.1，半值遠離零；超過 int16 範圍
飽和至 -32768／32767（即 -3276.8～3276.7），NaN 讀回 0。原 float32 PV 不被裁切。

## MV 對映 DAC

0x4910～0x4913 寫入同時驅動對應的 PWM 與 DAC0～3，不需 Apply。
DAC 使用下式（整數運算，四捨五入）：

`DAC code = 最小 code + round((最大 code - 最小 code) × MV / 1000)`

端點設定在 `_Product/Config/ProductDacConfig.h` 的 `PRODUCT_DAC_MV_MIN_CODES`
及 `PRODUCT_DAC_MV_MAX_CODES`，每組依 DAC0～3 排列；目前預設四路皆為 0～65535。
例如 MV=0 → 0、MV=250 → 16384、MV=500 → 32768、MV=750 → 49151、MV=1000 → 65535。
若最小 code=1000、最大 code=11000，MV=500 則為 6000，包含最小值偏移。
這些是 DAC 原始 code 端點；實際電流／電壓仍取決於 MAO_SEL、類比電路與校正。

成功寫入會更新 0x1400～0x1403 的 Active／對應 Pending code，並保留其他通道
尚未 Apply 的 raw code。原 0x1400 區的 raw code Pending／Apply 操作保持不變。
直接寫 raw code 不反向改變 MV／PWM。只寫 PDO_period 不會改變 DAC。
DAC 寫入失敗時回覆 0x04，嘗試還原同次命令已更新的 DAC 與 PWM；DO 失敗時
也會嘗試還原同次更新的 DAC。連續硬體故障仍可能導致回復失敗。

## 錯誤與批次寫入

- PWM period=0～9 或大於 10000、duty 大於 1000、DI／DO 使用 bit4～15：回覆 0x03。
- 超出相容區範圍的批次存取：回覆 0x02。
- FC10 在操作任何硬體前驗證全部數值；數值非法時整批不生效。
- PWM 或 DO 硬體失敗：回覆 0x04；保留原 register context，並嘗試還原已操作的 PWM。
  DO 的回復由 DigitalOutputService 處理。硬體連續故障時無法保證回復成功。
- 相容區直接寫入會同步該欄位的 Pending 值；同通道其他尚未 Apply 的欄位仍保留。

## ADC 診斷搬移

原 ADC 診斷區為 0x4820～0x491F，與相容區重疊；現搬到 **0x4E00～0x4EFF**。
每顆仍有 64 個 register，欄位偏移不變：CH0=4E00、CH1=4E40、CH2=4E80、CH3=4EC0。
原診斷位址不再提供服務；PC 診斷工具基底已同步更新。
0x4C00～0x4DFF 工廠校正資料位址不變。

## 驗證細則

`python3 _Product/Tests/run_factory_calibration_tests.py` 中的 Modbus 測項包含：

1. 四通道正負 PV、int16 邊界、讀取飽和與四捨五入、標準 float32 讀回及取樣覆蓋。
2. DI 暫時寫入、標準位址讀回與成功取樣後恢復。
3. 四通道 duty／period 及 DO 不需 Apply 即更新硬體與標準 register。
4. 24 個 register 的完整 FC10、保留 RAM 與批次越界。
5. duty／period／bit mask 範圍檢查、尾端數值錯誤時整批不變。
6. 既有 Pending 隔離、標準 Apply 寫入後相容區反向讀回。
7. PWM／DO 故障回覆與 PWM 回復。
8. 四路 MV=0／250／500／750／1000 的實際 mock DAC code 與 raw register 讀回，
   另以四路不同非零端點重新編譯驗證公式中的最小值偏移；DAC Pending 隔離與跨通道故障回復。
9. 搬移後四顆 ADC 診斷及既有工廠校正測項；ADC DEBUG=1／0 皆執行。

Host 測試不取代 ARM 連結、RS-485 RTU／ASCII 封包及實際輸出上板驗證。
