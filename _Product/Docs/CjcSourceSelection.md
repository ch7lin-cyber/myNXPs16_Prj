# CJC 補償來源選擇

每通道獨立設定，預設 **1：內部 TMP20**。

| 數值 | 模式 | 行為 |
|---:|---|---|
| 0 | 關閉 | 完全跳過 CJC 電勢相加，也不要求 TMP20 讀值有效 |
| 1 | 內部補償 | CH0／CH1 使用 TMP20 CJC_0；CH2／CH3 使用 TMP20 CJC_1 |
| 2 | 外部補償 1 | 來源入口預留，尚未接其他通道或外部寫入值 |
| 3 | 外部補償 2 | 來源入口預留，尚未接其他通道或外部寫入值 |

內部不是 AD7124／MCU 晶片溫度。每兩通道共用一顆 TMP20，端子與 TMP20 的溫差及通道間熱梯度仍需獨立驗證／校正，本次沒有加入各通道 CJC Offset。
TMP20 電壓→溫度使用既有公式 Vout=1.8605−0.01177×T (V)。TC 冷端溫度→電勢使用既有各型 CJC 表；K 型表與主機測試已存在，本次未抄舊版程式。

## Register Map

| 通道 | CJC source | Apply Key |
|---|---|---|
| CH0 | 0x1008 | 0x1009 |
| CH1 | 0x1018 | 0x1019 |
| CH2 | 0x1028 | 0x1029 |
| CH3 | 0x1038 | 0x1039 |

FC03 讀 Active。FC06／FC10 寫 CJC source 到 Pending，寫 0xA5A5 Apply 後生效。0～3 以外回 Illegal Data Value (03)。
CJC 僅影響 TC 換算，RTD／V／mV／mA 不使用它。來源變更會重置該通道的 PV 濾波狀態，避免混合舊補償結果。
2／3 可以保存與讀回，但 TC 測量會回 CJC 無效 (input_error=9)，不靜默回退到 TMP20。尚未提供外部 CJC 溫度 Register，也沒有使用其他通道的實作。

## 銅線 K 型十點驗證

1. 更新韌體與 PlatformCore submodule，重新編譯／燒錄。
2. RTU 選單 6 用 FC10：起點 0x1008，兩個值 `[0x0000, 0xA5A5]`。
3. FC03 讀 0x1008 確認為 0。
4. CALYS 工具加 `--cjc DIS`，執行選單 1／1.1.0。切 Sensor 時會保留 DUT 目前 CJC source，不再固定寫 0。
5. 恢復正常 K 型感測器使用時，FC10 起點 0x1008 寫 `[0x0001, 0xA5A5]`。

也可一次設定 Sensor＋來源＋Apply，例如 CH0 K／關閉：FC10 起點 0x1007，三個值 `[0x0030, 0x0000, 0xA5A5]`。
`--cjc DIS` 仍只控制 CALYS；DUT 的來源設定要透過上述 Register 切換。

## 保存與相容性

來源加入既有配置事件與 NVM 保存／ACK 流程。NVM 格式由 v3 的每通道 8 bytes 升為 v4 的 10 bytes。仍可讀 v3；v3 還原時自動設為內部 TMP20 (1)，保留原 Sensor、Filter 與 revision。新寫入使用 v4。
舊韌體不認得 v4；需回退舊韌體時，不可假設它能還原新格式設定。Factory 校正資料槽與係數格式不變。

主機已檢查 Pending／Apply、非法值、TMP20 分組、關閉不依賴 TMP20、外部來源無效、K 1300°C 關閉補償換算、v3 遷移及關閉／外部來源保存還原。仍待 MCUXpresso 目標編譯與實機十點重測。
