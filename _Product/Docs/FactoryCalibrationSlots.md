# 工廠校正資料槽與 Sensor type 對映

本階段建立四通道 × 十種 Profile，共 40 組獨立的零點／跨度資料槽，
並讓正常量測選用相同 Profile。資料為 ADC 端 µV，尚未還原分壓。
S16 Debug / Release 編譯符號 `FACTORY_CALIBRATION_INPUT_COUNT=4`，
只配置四通道；平台預設仍支援 16 通道。

## 固定讀取位址

FC03，每組八個 holding registers；所有位址均為十六進位。
位址公式：`0x4C00 + channel * 0x80 + profile * 8`。

| Profile ID | 輸入路徑與 Gain | Sensor type | CH0 | CH1 | CH2 | CH3 |
|---:|---|---|---|---|---|---|
| 0 | TC / mV，Gain 32 | K、D、E、N、L、U、TXK、0～50mV | 4C00 | 4C80 | 4D00 | 4D80 |
| 1 | TC，Gain 64 | T、R、C | 4C08 | 4C88 | 4D08 | 4D88 |
| 2 | RTD，Gain 8、500µA | Pt100、Ni120 | 4C10 | 4C90 | 4D10 | 4D90 |
| 3 | TC，Gain 16 | J | 4C18 | 4C98 | 4D18 | 4D98 |
| 4 | 電壓，Gain 32 | 0～5V、0～10V | 4C20 | 4CA0 | 4D20 | 4DA0 |
| 5 | RTD，Gain 16、500µA | JPt100 | 4C28 | 4CA8 | 4D28 | 4DA8 |
| 6 | 電流，Gain 64 | 0～20mA、4～20mA | 4C30 | 4CB0 | 4D30 | 4DB0 |
| 7 | TC，Gain 128 | B、S | 4C38 | 4CB8 | 4D38 | 4DB8 |
| 8 | RTD，Gain 1、250µA | Pt1000 | 4C40 | 4CC0 | 4D40 | 4DC0 |
| 9 | RTD，Gain 32、500µA | Cu50 | 4C48 | 4CC8 | 4D48 | 4DC8 |

| 相對組別起點 | 內容 | 型別 |
|---:|---|---|
| +0、+1 | 已套用的實測零點 | signed int32 µV，高字在前 |
| +2、+3 | 已套用的實測跨度點 | signed int32 µV，高字在前 |
| +4 | 換算資料可用 | 0 / 1 |
| +5 | 已經 Factory Apply 校正 | 0 / 1 |
| +6、+7 | 保留 | 0 |

例如 CH0 電壓組：零點 `0x4C20～0x4C21`，跨度點 `0x4C22～0x4C23`。
每通道最後 48 個寄存器保留並回傳 0。所有資料槽禁止 FC06 / FC10 直接寫入。
這些正式校正資料位址不受 `PRODUCT_ADC_DEBUG_ENABLE` 開關影響。

## Factory mode 選擇介面

- `0x4703`：通道 0～3。
- `0x4704`：上表 Profile ID 0～9。
- `0x4702`：1 選擇、2 擷取零點、3 擷取跨度、4 Apply、5 Abort。
- `0x4709～0x470C` 仍是目前操作的 pending 點；新資料槽是各組 Apply 後的值。

舊 Profile ID 3 原為獨立 mV 組，現在為 TC Gain 16；mV 改用 ID 0。
舊 ID 5 原為獨立 10V 組，現在為 RTD Gain 16；10V 改用 ID 4。
外部校正工具需依新表更新，不能沿用舊的 3 / 5 定義。

## 本階段完成範圍

預設值 0 / 30000µV 為原有等比例直通資料，`可用=1、已校正=0`，
避免尚未工廠校正時停止量測。Factory Apply 成功後才將對應組的已校正位設為 1。
新資料槽目前在 RAM；重啟會恢復預設值。

目前校正換算仍沿用原有 30000µV 目標。各 Profile 的理論零點／跨度目標、
實際校正步驟、穩定偵測、NVM 序列化／保存／開機還原留待下一階段實作。
因此本階段不能直接把外部 5V 擷取為跨度，就認定電壓精度校正已完成。
RTD 校正點也需另依激勵電流與參考電阻定義；CJC 不包含在這 40 組內。
