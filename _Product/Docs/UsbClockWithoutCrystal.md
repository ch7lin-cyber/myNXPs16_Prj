# 產品板 USB FS 時鐘（無外部晶振）

產品板未裝外部晶振，USB CDC 使用內部 FRO 96 MHz，經 USB divider 除以 2 得到 48 MHz。

`product_usb_cdc.c` 呼叫 `CLOCK_EnableUsbfs0DeviceClock(kCLOCK_UsbfsSrcFro, 96000000U)`。第二個參數是來源頻率；SDK 負責分頻並啟用 `USBCLKADJ`。此路徑不啟動外部 XTAL32M，也不等待 PLL1 鎖定。

CPU 時鐘維持內部 FRO 12 MHz → PLL0 → 150 MHz；`board/clock_config.c` 已啟用 FRO HF 96 MHz。

原設定使用外部 16 MHz 晶振 → PLL1 → USB 48 MHz，在無晶振的產品板會卡於 `CLOCK_IsPLL1Locked()` 等待。

驗證：已檢查本專案 SDK 的 FRO 分支、96 MHz 分頻及 USBCLKADJ 設定。尚未執行目標板編譯與實測。請 Clean/Rebuild、重新燒錄 APP，確認 USB 初始化通過、Windows COM port 列舉及 Modbus 通訊；再測拔插和冷開機。若另有 bootloader 使用外部晶振設定，需另行調整該專案。
