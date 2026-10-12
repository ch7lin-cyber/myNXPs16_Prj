# ADC 除錯資訊編譯開關

在 `_Product/Config/ProductFeatureConfig.h` 設定：

```c
#define PRODUCT_ADC_DEBUG_ENABLE (1U) /* 除錯：預設啟用 */
/* 改成 (0U) 可關閉 */
```

也可在整個專案的編譯器符號設定加入 `PRODUCT_ADC_DEBUG_ENABLE=0`。
變更後須 Clean / Rebuild，讓所有引用診斷結構的檔案使用一致設定。
這是編譯選項，不能透過 Modbus 動態切換。

| 功能 | 設為 0 時 |
|---|---|
| Configure 來源、階段、次數與事件 ACK 除錯計數 | 不編譯、不配置紀錄 RAM |
| Read 階段、失敗快照 | 不編譯、不配置紀錄 RAM |
| RAW → µV 分子、分母、商與整數上下限明細 | 不編譯呼叫、不配置紀錄 RAM |
| IO_CONTROL1／CHANNEL0／CONFIG0／FILTER0 診斷回讀 | 不執行額外 SPI 診斷回讀 |
| `0x4A00～0x4AFF`、`0x4B00～0x4BFF` | FC03 回覆 Illegal Data Address（0x02） |
| 各 ADC 基本診斷區塊的 offset 53～63 | 保留位址，回傳 0；不代表讀回有效 |

量測、首筆樣本丟棄、Sensor type 切換、ACK 重試保護、ADC 錯誤分類、
CRC、ERROR_ENABLE 檢核、故障恢復、工廠校正與 PV 換算仍然啟用。
成功／丟棄計數、目前故障與 RAW／µV 基本診斷也保留。

HAL 的一般換算入口不再引用除錯明細入口。本專案 Debug / Release 已使用
`-ffunction-sections -fdata-sections` 與 `--gc-sections`，因此關閉後未引用的
換算明細函式會由連結器移除。移植至其他專案時也需保留這些選項。

單元測試已驗證開啟／關閉兩種設定下的 ADC 讀取、錯誤處理、Sensor type
事件與 Modbus 存取。MCU 的實際 Flash／RAM 節省量以重新編譯的 map 與
MCUXpresso memory usage 為準。

## Python 診斷工具與連續請求

`modbus_rtu_test_menu.py` 每筆請求送出前固定等待 50 ms，以便 RS-485 slave 完成回覆與恢復接收。預設 timeout=1 秒，可用 `--timeout 3` 做對照。加入間隔不代表 MCU 通訊問題已完成定位。

```powershell
python D:\Python\pyTestCode\NXP\myNXPs16_Prj\_Product\Tools\modbus_rtu_test_menu.py --port COM8 --slave 2 --baud 115200 --timeout 3
```

CALYS 工具的 DUT wrapper 另預設等待 50 ms，搭配共用 client 合計約 100 ms，並對可重試的讀取錯誤重試。請避免兩支工具同時開啟 COM8。

`STATUS=0x80` 表示當下尚未有新 ADC 資料；需搭配成功計數、目前故障及最近讀取結果判斷，不能單獨當成故障。
`Latched ERROR OR` 是歷史累積值；目前故障以 Active fault／Last ERROR 與持續取樣狀態核對。PV 與 microvolts 在故障時可能保留先前有效值。

完整啟動方法見 [useMethod.md](../Tools/useMethod.md)，十點差值與結果摘要見 [Calys1500LoopTest.md](Calys1500LoopTest.md)。
