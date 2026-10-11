# USB0 CDC 虛擬 COM / 共用 Modbus Register Map

## 已建立

- [x] USB0 FS CDC ACM device stack、descriptor 與 USB0 IRQ。
- [x] 開機初始化並由主迴圈處理 USB CDC。
- [x] Modbus RTU 03／04／06／10 接到 FC0 相同 ProductModbusRegisterAdapter。
- [x] USB 分包／合包、CRC、廣播、站號變更、部分封包逾時、session reset、TX retry 主機測試。
- [x] Debug／Release SDK 原始碼主機語法與實際 CDC 描述器檢查。
- [ ] MCUXpresso ARM 編譯、連結、Flash／RAM 容量確認。
- [ ] Windows COM 枚舉、插拔、最大長度傳輸與 FC0／USB 雙介面上板驗證。

## 使用

韌體跑起來後接 **FS_USB / USB0** 資料線（DP pin 97、DM pin 98、VBUS pin 78），
不是板上的 Debug Link USB，也不是 USB1 HS。Windows 裝置名稱為 S16 Modbus USB。
使用原 Python Modbus 測試程式，改成新枚舉的 COM port，協定選 RTU。
USB COM 固定 **115200／8N1、Modbus RTU**，Python／主機端也使用此設定。
主機 SET_LINE_CODING 控制傳輸會被接受以維持 Windows 開啟相容性，
但裝置不套用其他格式，GET_LINE_CODING 永遠回報 115200／8N1。
USB 本身沒有 UART 鮑率時鐘，此設定不會改動 FC0 實體 UART。

USB 使用 FC0 的 active slave ID（目前測試一般為 2，以實際設定為準）。
例如 02 03 10 00 00 02 C0 F8 可讀 CH0 PV；0x2000 起的版本區也共用。
USB 固定 RTU，即使 FC0 改 ASCII，USB 仍用 RTU。未增加任意 MCU RAM 存取功能。
所有位址、唯讀限制、Pending／Apply key、Factory Mode 與 NVM 行為
均由既有 Register Adapter 處理，不建立第二份 Register Map。

FC0 與 USB 的 register 操作都在同一主迴圈執行，USB IRQ 只處理傳輸完成及控制請求。
若兩個主機都寫同一 Pending 設定，依主迴圈處理順序生效，不提供交易隔離或自動鎖定。

## 傳輸與時鐘

USB bulk packet 邊界不等於 Modbus RTU frame；依功能碼及 byte count 累積請求，
不套用 UART T1.5／T3.5。部分請求 1 秒未收到新資料就清除。
CRC 錯誤不執行 register callback。支援功能碼以外的資料會丟棄。
端點有背壓：USB OUT 暫存未處理前不重新掛接；回應未送出前不再執行新請求。
USB reset／重新 configuration 清除部分請求與待送回應。
64-byte 整數倍回應補 zero-length packet。

USB0 使用與已測試 bootloader 相同的 16 MHz 晶體／PLL1 產生 48 MHz USB 時鐘；
不修改 CPU PLL0 時鐘或另建 SysTick。初始化失敗時 debugger 的
`g_productBootError=9`；無 USB 主機時仍可初始化，FC0 正常運作。

## SDK／IDE

NXP USB device sources、CDC class、descriptor 與 bare-metal OSA header
沿用本地 myNXP_bootLoader_005 的 SDK 檔案，保留授權標示。
描述器目前沿用 NXP 開發範例 VID/PID；正式產品需使用可授權的產品識別。
USB 裝置本輪採用與 bootloader 相同 VID/PID，Windows 可能沿用相同 COM 編號。
Debug／Release .cproject 已加入 USB device source 與 include paths。
MEX 尚未同步 CDC class 選項，Config Tools 重生 usb_device_config.h 時需保留
LPCIP3511FS=1、LPCIP3511HS=0、CDC_ACM=1。

## 驗證範圍

主機測試使用真實 ModbusSlave／CRC／功能碼處理程式，USB transport 及 Register Adapter
使用 fake。因此已驗證 USB service 封包流程，但不代表實際 Register Map、Apply／NVM
或 USB 硬體已驗證。SDK 主機語法檢查不能替代 ARM 連結及實際 COM 枚舉。

執行：`python3 _Product/Tests/run_usb_modbus_tests.py`。
