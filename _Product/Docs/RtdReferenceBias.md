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
CHANNEL0=0x80C5、CONFIG0=0x0863 時 REF_EN 仍開啟，
以及未要求偏壓的外部參考配置可關閉 REF_EN。
Product driver 的 DEBUG=0／1 測試確認此需求會傳給底層。

實機重測先確認 CALYS「Current too low」是否消失、成功樣本數是否持續增加、
故障丟棄數是否停止增加。這次不調整零點／跨度或 NVM。
RTD 的外部參考微伏／電阻比例換算仍需獨立驗證；
REF_EN 修正不代表 PV 精度已完成驗證。
