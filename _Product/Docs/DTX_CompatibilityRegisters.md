# DTX 相容暫存器 0x4908～0x491F

## 摘要

本區支援 FC03／04 讀取與 FC06／10 寫入。寫入直接更新指定的 Active 欄位，
不需 Apply Key，也不套用其他欄位原有的 Pending 設定。既有標準位址的
Pending／Apply 規則保持不變。資料不額外寫入 NVM。

PV／DI 寫入只暫時改變 Modbus 監測值，下一次成功量測會覆蓋；DI 不改變實體輸入
或 DigitalInputService 內的取樣狀態。PWM／DO 則透過現有輸出 service 操作硬體，
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
| 0x4910 | CH0 PWM duty | 0x1301 | 0～1000，即 0.0～100.0% |
| 0x4911 | CH1 PWM duty | 0x1304 | 0～1000 |
| 0x4912 | CH2 PWM duty | 0x1307 | 0～1000 |
| 0x4913 | CH3 PWM duty | 0x130A | 0～1000 |
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
8. 搬移後四顆 ADC 診斷及既有工廠校正測項；ADC DEBUG=1／0 皆執行。

Host 測試不取代 ARM 連結、RS-485 RTU／ASCII 封包及實際輸出上板驗證。
