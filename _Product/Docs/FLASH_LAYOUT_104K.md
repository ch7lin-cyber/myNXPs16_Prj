# APP／OTA 暫存區擴大為 104 KiB

2026-10-23：以主專案 `d8e828a` 為基準，只調整 Flash 分區與相應容量檢查。編譯最佳化設定、PID／CKLIB、USB／RS485 功能與 PlatformCore 子模組版本維持基準。

## 分區配置

下列終點皆不包含在區間內。

| 區域 | 起點 | 終點 | 容量 |
|---|---|---|---|
| Bootloader | `0x00000` | `0x08000` | 32 KiB |
| APP | `0x08000` | `0x22000` | 104 KiB |
| OTA 暫存區 | `0x22000` | `0x3C000` | 104 KiB |
| Metadata 0 | `0x3C000` | `0x3C200` | 512 bytes |
| Metadata 1 | `0x3C200` | `0x3C400` | 512 bytes |

APP 與暫存區原為各 96 KiB；現在使用原暫存區後方的 16 KiB 空間，平均增加各 8 KiB。Bootloader、metadata、FFR 與 RAM 配置不變。現有 ROM adapter 以 512-byte page 擦除，不能將回報的 32-KiB sector 大小當成最小擦除單位。

OTA1 的 48-byte header 存於暫存區開頭，因此最大 APP BIN 為 `106496 - 48 = 106448 bytes`。APP linker 保留這個限制；不是只移除 ASSERT。使用者提供的 99460-byte 連結報表在新 payload 限制內尚有 6988 bytes；實際結果仍以重新連結的 MAP／BIN 為準。

## 必須搭配的版本

- Bootloader：`myNXP_bootLoader/005_NXP_55S16_IAP_example`，版本 00.01.11。
- Windows 工具：`myNXP_bootLoader/004_WindowsForms_CSharp` 的新版原始碼。最大 payload 改為 106448，較大映像須先確認 Bootloader >=00.01.11。
- APP：Debug／Release 的 managed memory 設定均採 `0x8000 / 0x1A000`。不要手改自動產生的 Debug/*.ld；Refresh、重新開啟專案，再 Clean／Build。
- Python 更新工具同步支援新容量；超出舊容量的映像在寫入前檢查 Bootloader 版本。

現有流程是 active APP 加下載暫存／複製安裝，不是雙 APP 自動回滾；此次不增加 A/B rollback。

## 實機遷移順序

1. 使用舊 Bootloader 完成既有 PENDING 安裝，確認沒有 Receiving／Staged／Busy。舊 PENDING 的暫存位址是 `0x20000`，不可跨版本直接續裝。
2. 保留舊版 Bootloader、APP BIN 與必要 Flash 備份。新版維持 ACTIVE metadata 格式，已安裝的小型 APP 可繼續驗證；不自動抹除 metadata。
3. 在 MCUXpresso 重新編譯 Bootloader 00.01.11，先透過 SWD 更新 Bootloader。留意燒錄設定是否整片擦除；若整片擦除，需重新下載／安裝 APP。
4. 重新編譯 Windows 更新工具，讀回 Bootloader 版本必須為 00.01.11。
5. APP Clean／Build，確認起點 `0x8000`、PROGRAM_FLASH 容量 104 KiB、BIN <=106448 bytes，再透過 USB 或 RS485 下載、驗證、安裝。
6. 執行啟動 APP、重新上電、更新中斷／續裝、CRC 拒絕及安全輸出實測，之後再進行原先的系統安全驗證。

新版 Bootloader 遇到舊 PENDING 不會自動搬移暫存資料；無法在新位址驗證時會拒絕安裝，不能將其當成已恢復成功。需要退版時亦先完成目前更新，且不能讓舊 96 KiB Bootloader 安裝或驗證超出其限制的 APP。

## 驗證紀錄

- APP adapter／共用更新庫 Host regression、managed memory 與 linker 容量檢查：通過。
- Bootloader core、ROM adapter、RTU、updater、Python→C 更新測試：通過。只有 SDK 無關的 Host runtime 測試已執行；完整 SDK／MEX 專案靜態檢查未執行。
- 新增最大 106448-byte 映像、超限拒絕、metadata／Bootloader 邊界及舊 PENDING 拒絕測試。
- Windows 容量邊界與舊 Bootloader 拒絕測試已加入；本環境無 .NET SDK，尚未執行。
- MCU／MCUXpresso 編譯連結及 USB／RS485 上板更新尚未驗證。
