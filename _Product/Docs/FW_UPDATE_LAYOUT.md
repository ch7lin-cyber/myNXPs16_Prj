# APP 更新配套：Staging

APP 分支：**branch2_ver1_staging**。搭配 Bootloader：`ch7lin-cyber/myNXP_bootLoader` 的 **005_NXP_55S16_IAP_Staging**。
Bootloader 版本：00.01.12。C#：`004_WindowsForms_CSharp` **0.4.6 / FileVersion 0.4.6.0**，同一支 EXE 支援兩模式。

| 項目 | 設定 |
|---|---|
| Bootloader | 0x00000..0x08000（終點不包含） |
| APP 起點 | 0x8000 |
| APP 分區大小 | 0x1A000 / 104 KiB |
| APP 終點（不包含） | 0x22000 |
| APP BIN 最大大小 | **106448 bytes** |
| Metadata 保留 | 0x3C000..0x3C400 |
| 其他 Flash | 本次不擴用 |
| SRAMX | 0x04001000..0x04003FC0；前4KiB MCAN，尾64B mailbox |

## 建置與下載

1. `git clone --branch branch2_ver1_staging --recurse-submodules <APP-repository-url>`，或已 clone 後 `git submodule update --init --recursive`。必須使用儲存庫固定的 submodule commit。
2. 匯入 MCUXpresso，確認 `.cproject` 的 MCU Settings / PROGRAM_FLASH 起點0x8000、大小0x1A000；Debug/Release 共用該記憶體配置。
3. `_Product/linker/app_update_assert.ld` 同時檢查 APP 起點、容量、payload 終點、MCAN 及 mailbox；不要只在 IDE 改大小而忽略這個 ASSERT，也不要手改 Debug 下自動生成的 linker script。
4. Clean/Rebuild Debug 或 Release，檢查 MAP，再用上述配套 C# 選 APP `.bin` 下載。Bootloader 請依其分支 `_Bootloader/docs/update_mode.md` 先用 SWD 建置/燒錄。
5. 若透過 IDE 直接下載 APP，只會寫入程式，不代表 Bootloader 的 ACTIVE metadata 已建立。正式驗證以 C# 完成更新，再按啟動 APP、斷電重啟確認。

Staging：先收至暫存區再安裝，PENDING 可續裝；OTA1 header 佔48 bytes。

## 本次修改範圍

保留原 `.cproject` 與 linker 限制，應用功能未變更。
APP USB/RS485 切換流程沿用共用庫；本文件不會把已停用的 USB 重新啟用，是否含 USB 依實際建置配置。

已檢查 `.cproject` 與 linker 邊界一致；APP 的完整 ARM 編譯/連結及板上更新待實測。其他既有建置相依（例如 CKLIB）不由此容量修改解決。
