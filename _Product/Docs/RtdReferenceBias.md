# RTD 外部參考與 REFOUT 板級偏壓

Sensor type 切換至 RTD 時，ADC 使用 REFIN1，不代表可以關閉內部參考輸出。
此板的 REFOUT 經 REF0_C／REF0_D／REF0_E／REF0_F 與運算放大器，
提供 CHx_VS 偏壓。舊初始化程式始終設定 CTRL_REF_ENABLE；
新 driver 原先只在某個 setup 使用內部參考時設定 REF_EN，
因此只配置外部參考的 RTD 時，會關閉 REFOUT 並影響類比回路。

產品設定 PRODUCT_ADC_REFERENCE_OUTPUT_REQUIRED=1，
由 product_adc_driver 透過 referenceOutputRequired 告知 AD7124 driver。
driver 在 ADC 使用外部參考時仍保持 REF_EN；不改變 REF_SEL、
AIN6−AIN5、Gain 或兩路激勵電流。
其他沒有 REFOUT 負載的板子可以將此需求設為 0。

回歸測試涵蓋外部 REFIN1、Gain=8、IO_CONTROL1=0x24F0、
CHANNEL0=0x80C5、CONFIG0=0x09E3 時 REF_EN 仍開啟，
以及未要求偏壓的外部參考配置可關閉 REF_EN。
Product driver 的 DEBUG=0／1 測試確認此需求會傳給底層。

實機重測先確認 CALYS「Current too low」是否消失、成功樣本數是否持續增加、
故障丟棄數是否停止增加。這次不調整零點／跨度或 NVM。
RTD 的外部參考微伏／電阻比例換算仍需獨立驗證；
REF_EN 修正不代表 PV 精度已完成驗證。

## 舊版 RTD 比例換算與設定對齊

RTD Sensor Apply 開啟正負參考 Buffer；Pt100 gain=8 時 CONFIG0=0x09E3。
RTD 使用 Sinc4 single-cycle，FS 按舊版 614400/101/32/4 整數計算為 47。
目前產品 REJ60 設定仍為 1，預期 FILTER0=0x11002F；TC/V/I 不變。

舊版 RAW 換算等效 R_mOhm=(RAW-8388608)*4300000/(gain*8388608)。
4300 ohm 是舊程式等效係數，不宣稱是 R82 阻值。
產品 driver 以 4300*I_uA 作為換算係數，輸出 R*I 的等效微伏，
下游再除以激勵電流得到電阻。使用既有 signed 64-bit 轉換，
不先截斷為 mOhm，保持 HAL、FactoryCalibration 與 FRAM 的微伏單位。
Pt100 使用係數 2150000，Pt1000 使用 1075000；診斷 reference 欄位
此時代表等效換算係數，不是實測 REFIN 差動電壓。固定外部 2.5V 常數
只供其他外部參考路徑使用，RTD 不使用它，也不直接改成 Excel 的 1.92V。

既有 RTD 校正紀錄若以先前固定 2.5V 換算所取得，需重新工廠校正；
此提交不清除 NVM、不修改目標跨度與儲存格式。
Host 測試涵蓋 Pt100 100 ohm -> 約 50000 uV、Pt1000 1000 ohm ->
約 250000 uV，DEBUG=0/1，以及實際配置寄存器。
實機仍須用 CALYS CONT/1MA 驗證有效樣本與 PV，先測 0/100/250°C。
