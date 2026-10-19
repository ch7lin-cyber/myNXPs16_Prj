# FwUpdateApp：APP 線上更新入口公用庫

此庫從 `myNXPs16_Prj/branch2_ver1` 已實測的更新入口抽出，保持 Windows 004 與 LPC55S16 Bootloader 005 的既有協議。
這一版只重構 APP，沒有更動 UI、Bootloader 或實際燒錄演算法，尚未加入簽章／防降版。

## 檔案與責任

| 檔案 | 用途 |
|---|---|
| `include/fw_update_app.h`、`src/fw_update_app.c` | 固定記憶體狀態機、身分讀取、命令／鮑率檢查、等待安全停機與 TX 完成 |
| `include/boot_request.h` | 與現有 Bootloader 相容的 SRAM mailbox ABI、CRC、編碼／檢查 |
| `tests/test_fw_update_app.c` | 不需要 SDK 的 Host 測試，不要加入 MCU build |

庫不依賴 NXP SDK、產品 GPIO、USB driver、作業系統或動態記憶體。
同一裝置的 USB／RS485 共用一個 `FwUpdateApp_t`；所有 API 在前景依序呼叫，ISR 僅記錄 TX 完成事件，再由前景通知。
config/platform 必須在實例使用期間持續有效，且不可更改。

## 新 project 整合

1. 複製整個 `FwUpdateApp` 目錄。將 `src/fw_update_app.c` 加入 Debug／Release 的編譯來源，加入 `include` 搜尋路徑。
2. 準備一個固定 `FwUpdateApp_t`、`FwUpdateAppConfig_t` 與 `FwUpdateAppPlatform_t`，初始化時呼叫 `FwUpdateApp_Init()`，並檢查回傳值。設定錯誤會保持 FAILED，不允許更新。
3. 實作 `quiesce(user)`：同步停止控制器、排程與所有必要輸出；成功回 true，失敗回 false。失敗後狀態鎖定，不能重啟進入 updater。
4. 實作 `reset(user, route, unit, baud)`：依現有板級 adapter 提交 mailbox、需要時 detach USB，最後執行系統 soft reset。不能正常返回；意外返回會鎖定 FAILED。
5. 將現有 Modbus 03/04 身分／狀態讀取及 FC06 更新控制寫入轉交本庫。處理每一請求前設定 route/station，之後清為 0；廣播 route 必須為 0。FC10 不得寫更新控制區。
6. 每次前景循環呼叫 `FwUpdateApp_Process(now_ms)`；傳入可回繞的單調 uint32 毫秒計時。收到更新請求後，透過 `FwUpdateApp_IsPending()` 停止正常 APP 控制排程，並讓一般控制 register 寫入回 busy。通訊與 watchdog 仍繼續。
7. 命令回覆真正傳完後才呼叫 `FwUpdateApp_ReplySent(route)`：USB 要等 IN completion；RS485 要等 TXIDLE 且 DE 已轉回 RX。排入 TX queue 不等於完成。
8. 保留 APP Linker／向量設定及 mailbox，依 Bootloader 分區限制產生 BIN，使用既有 UI 燒錄後重測。

`reply_delay_ms` 從首次處理請求的 Process 計時；目前產品為 200 ms。即使時間已到，只要沒有 TX completion 也不會 reset。
`quiesce` 不可長時間阻塞通訊與 watchdog；若產品需要非同步停機，必須另行設計完成握手，不能把「停機尚未完成」當成成功。

### 初始化範例

```c
static FwUpdateApp_t updater;
static const FwUpdateAppConfig_t config = {
    .identity = {0xA016, 1, 1, 1, 0xC200, 8, 0, 1, 0, 1},
    .allowed_routes = FW_UPDATE_APP_ROUTE_USB | FW_UPDATE_APP_ROUTE_RS485,
    .reply_delay_ms = 200u
};
static const FwUpdateAppPlatform_t platform = {
    .user = &product_context,
    .quiesce = ProductStopForUpdate,
    .reset = ProductCommitMailboxAndReset
};
/* 初始化：若失敗，不啟動正常控制，回報產品啟動故障。 */
bool ok = FwUpdateApp_Init(&updater, &config, &platform);
```

`product_context`、兩個 callback 由產品提供。USB-only／RS485-only 產品可僅啟用對應 route bit。
此身分範例屬於既有 LPC55S16 協議；其他 MCU 不能只改一個識別值就使用同一 Bootloader。

### 協議相容範圍

| 功能 | 位址／值 |
|---|---|
| APP 身分 | 0x7000 起 10 registers，配置值須與 Bootloader／UI 協議一致 |
| APP 版本 | **產品繼續提供** 0x2000～0x2002 三個 U16；本庫不攔截產品版本 registers |
| 進 USB updater | FC06：0x7050 = 0xB007，僅 USB request context |
| 進 RS485 updater | FC06：0x7050 = 0xB008，僅 RS485 request context |
| 更新鮑率 | FC06：0x7052 = 0..5；38400／57600／115200／230400／460800／921600 |
| 切換狀態 | 0x7051 起 3 registers：state／route／station |

route：0=禁止／廣播、1=USB、2=RS485；station=1..247。
state：0=Idle、1=待停機、2=待回覆、3=reset、4=失敗／reset 返回。
USB 與每次初始化的預設更新速率為 115200；RS485 的所選速率只傳給下一次 Bootloader，APP 通訊配置不在本庫變更。
版本、產品 ID 與映像 target 仍屬產品／Bootloader 的配置責任，未增加新的 wire fields。

### LPC55S16 的平台前提

現有分區：APP FLASH `0x8000`／`0x18000`，SRAMX 保留 `0x04003FC0..0x04003FFF` 共 64 bytes。
APP vector/linker 不可侵入 Bootloader、staging 或 metadata；使用產品的 linker assertions 檢查。
本庫的 `boot_request.h` 與 Bootloader 的 ABI v1 相同；mailbox 需 magic 最後提交、DSB、soft reset，並保留 SRAMX。
mailbox CRC 只防資料意外損壞。安全輸出腳位、DAC 安全值、USB detach、時脈和 reset 仍由板級程式實作。

目前產品整合請看 `_Product/Config/ProductFwUpdateConfig.h`、`_Product/Source/L03_System/product_fw_update.c`、`_Product/Source/L01_ProductDriver/product_fw_update_board.c`。
產品 wrapper 保留既有 `ProductFwUpdate_*` 呼叫，因此原來的 Modbus、scheduler 與板級程式不用改 API。

## 驗證

執行 `python3 _Product/Tests/run_fw_update_tests.py`：原產品回歸、獨立庫測試、兩種 IDE configuration 的 source entry 與分區設定檢查。
其他既有 USB／factory calibration 測試已補入本庫來源；測試需要原專案及固定版本 PlatformCore 子模組。
Host 通過不等於完整 MCU build：MCUXpresso Clean/Rebuild 後必須確認 map 容量、USB／RS485 實機更新及安全輸出。
