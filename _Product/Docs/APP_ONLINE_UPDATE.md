# APP 線上更新與 RS485 入口（Bootloader 00.01.08）

## 範圍與前提

本次串接 `myNXPs16_Prj/branch2_ver1`、Bootloader 005、Windows 004。
USB 與 RS485 均使用 Modbus RTU，115200 / 8N1。APP station 預設 2，004 可選 1..247；Bootloader 沿用切換請求的 station。
先以 SWD 更新 005，才部署這版 APP。舊版 005 不支援 RS485 request 與 RUN。
本次未修改 DTX、branch001 核心、原始 MEX、簽章或多產品封裝。

## 操作流程

1. USB 供電／一般 reset 仍固定進 USB updater，不做 USB／24 V 電源辨識。
2. 004 選 COM、站號及通道，連線。Bootloader 模式可直接下載。
3. 第一次將連結於 `0x8000` 的 APP BIN 下載、FINISH、INSTALL，產生 ACTIVE metadata。
4. 按「啟動 APP」。Bootloader 確認沒有 Receiving/PENDING/Busy、驗證 ACTIVE CRC、target 與向量，送完回覆後 soft reset，再從乾淨 reset 狀態跳入 APP。
5. APP 模式顯示 APP 版本，可按「進入更新模式」。USB CDC 通道選 USB，外接 USB-RS485 adapter 選 RS485。
6. APP 接受命令後停止 APP 的 Tick/Process 及 FC1 master 排程，先把 4 PWM／4 DO 轉 GPIO low、停 timer，並將 DAC code 清為 0。通訊與 watchdog 繼續工作。
7. USB 等待 IN 完成，RS485 等待 TXIDLE／DE 回接收。至少保留 200 ms，才提交 mailbox 並 reset。沒有完成回覆就保持安全停機，不用計時器強制 reset。
8. Bootloader 消耗有效 mailbox，選 USB 或 FC0 RS485。下載期間不執行 APP，INSTALL 完成仍留在 updater。由使用者再按「啟動 APP」。

004 切換後釋放 COM，最多約 15 秒重試原來選定的 COM，不向其他 COM 發送切換／燒錄指令。
USB 若改列舉為新 COM，請選新號碼再連線。RS485 adapter 本身不會因 MCU reset 消失。
APP 目前改成其他 baud／ASCII／站號時，須先用既有診斷工具切回 RTU 115200/8N1，並在004填入正確站號。

## 共用協議

| 模式／功能 | FC | Address | 內容 |
|---|---|---|---|
| 身分 | 03 | 0x7000 / 10 regs | Boot=0xB016、mode=2；APP=0xA016、mode=1、schema=1 |
| APP 版本 | 03 | 0x2000 / 3 regs | APP 既有 major/sub1/sub2 |
| APP 進 USB updater | 06 | 0x7050 | 0xB007，僅 USB callback 可接受 |
| APP 進 RS485 updater | 06 | 0x7050 | 0xB008，僅 FC0 callback 可接受 |
| APP 切換狀態 | 03 | 0x7051 / 3 regs | state、route、station |
| Boot 啟動 APP | 06 | 0x7020 | command=5，排程後驗證及等待 TX drain |

APP state：0=無請求、1=待安全停機、2=等待回覆完成、3=重置、4=停機失敗或 reset 意外返回。
廣播不能觸發更新，錯誤 route／key 回 03，重複請求回 06。APP 停機後其他控制 register 寫入回 busy。
0x7050 只接受 FC06，避免把 bulk write 當作可觸發 reset 的一般參數寫入。
Boot RUN 的 ACK 只表示接受排程，004 必須重新讀到 APP 身分才確認成功。

## Mailbox 與 linker

兩份相同的 `boot_request.h` 定義 ABI v1。固定 SRAMX `0x04003FC0..0x04003FFF`，共64 bytes。
包含 magic、ABI、mode、station、固定baud、保留欄位與 CRC32。先寫資料、DSB，再寫 magic、DSB、reset。
005 只在 software-reset flag 成立且 mailbox 完整有效時使用，讀取後立即清 magic。
此 CRC 只防意外損壞，不是認證／授權。reset 期間 SRAMX retention 仍需板上實測。

| 區域 | Location | Size |
|---|---|---|
| Boot FLASH | 0x0 | 0x8000 |
| APP FLASH | 0x8000 | 0x18000 |
| SRAM（雙方） | 0x20000000 | 0x10000 |
| USB_RAM（雙方） | 0x20010000 | 0x4000 |
| SRAMX（雙方） | 0x04000000 | **0x3FC0** |

APP Debug/Release 使用 managed linker 與附加 `app_update_assert.ld`，檢查向量位址、payload <=98,256 bytes、mailbox 不重疊。
005 使用原固定 boot.ld。不要修改自動產生的 Debug/*.ld 或放大區域掩蓋 overflow。
APP Debug/Release 加入 -Os/-flto，保留 debug symbols，但原始碼單步／區域變數可能受最佳化影響。
Post-build 自動產生 BIN，不再保留會把 objcopy 註解掉的 # 命令。
MEX 沒改，若工具重新產生 memory 設定，仍需維持上述邊界。

## RS485 硬體

FC0 RX=P0_29/FUNC1，TX=P0_30/FUNC1，DE/RE=P1_0，高為TX，低為RX，與 branch2 接線一致。
Bootloader 以 USART FIFO polling 收集 frame，至少3 ms idle後處理，錯誤／overflow丟棄該frame。
回覆前2 ms讓主機轉接收，DE assert/deassert各10 us，等待真正TXIDLE才放開DE。
EVK 驗證需外接 RS485 transceiver，LINK2 的 UART COM 不是此 RS485 電路。
USB boot clock 仍依目前005已驗證的EVK 16 MHz晶振/PLL1。無晶振的其他產品需另行配置 USB clock，不可直接當作已通用。

## 驗證與限制

Host：APP request CRC/route/broadcast/ACK/timeout/wrap/safe failure；實際USB parser延後IN完成；RUN拒絕PENDING/壞映像、TX drain、watchdog window；原有CRC/IAP/torn metadata/續裝回歸。
C#：APP/Boot辨識、站號、切換命令、RUN、舊Boot拒絕，以及實際C RTU updater下載/CRC/續裝整合。
ARM：以 GCC14.2 Cortex-M33 hard-float編譯來源，使用 Newlib Nano 做分區連結檢查。這不是 MCUXpresso11.7/GCC10.3/Redlib 的完整 build，實際AXF/BIN與map仍需在IDE Clean/Rebuild核對。
目前APP接近96KiB及64KiB SRAM上限，後續增加功能需重新看map，不可自動侵入staging。

尚需板上驗證：兩條實體通道切換、SRAMX跨reset、watchdog跨reset、USB COM重列舉、APP初始化、DE/RE波形、斷電恢復及安全輸出。
上電/reset到firmware初始化前的低電位仍需硬體下拉／gate保證。DAC code=0是否對應產品的安全工程量，需依輸出電路確認。
簽章、安全啟動、防降版、A/B回滾、外部電源自動啟動策略不屬於本次實作。

### 本輪容量檢查（GCC14.2／Newlib Nano，-Os）

Boot FLASH 23,836 bytes／SRAM 18,560 bytes。APP 加LTO後 FLASH 96,300 bytes／SRAM 63,940 bytes／USB_RAM 5,504 bytes。
這些是替代工具鏈的檢查數字，不能當成MCUXpresso/Redlib最終報表；APP容量餘裕有限。
