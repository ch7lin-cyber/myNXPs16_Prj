# 5 點中位數濾波

每通道固定啟用 Window = 5，位置在
`_Product/Source/L03_System/product_sensor_measurement_service.c`。

```text
ADC µV → Factory Calibration → Median(5) → Sensor Conversion → IIR → Filtered PV
```

- 四通道各自保存 5 筆校正後 µV，排序後取第 3 筆；使用 int32_t 比較，不做減法或平均。
- 首筆樣本填滿視窗，立即輸出；初始化、Sensor Type 或 CJC 設定變更會重置視窗。
- 只有不同 ADC sequence 才推進視窗，同筆資料反覆 Process 不會重複加入。
- 穩定背景下可排除視窗中最多兩筆異常值；持續階躍在第 3 筆新值時通過。
- AD7124 若為 50 Hz，階躍約增加 40 ms 延遲；100 Hz 時約增加 20 ms。
- IIR 時間常數與 Modbus / NVM 欄位維持原設計；IIR 設為 0 秒仍保留中位數濾波。
- Unfiltered PV 表示未經 IIR，但已經過 Median；raw_code、uncalibrated_uv、calibrated_uv 診斷仍保留當筆原值。
- Sensor Conversion 的範圍旗標以中位數後的 µV 計算，瞬間越界可能被抑制；ADC 取樣失敗與 CJC 錯誤仍直接回報。
- 校正服務失敗不加入視窗；取樣中斷不會自動清除歷史。

## 驗證

執行 `python _Product/Tests/run_factory_calibration_tests.py`。

Measurement 測試涵蓋正負 spike、三筆階躍、重複 sequence、通道隔離、OFF/ON 重置，
以及原有 TC、RTD、CJC、工程量程與錯誤碼測試（等待視窗穩定後檢查）。
