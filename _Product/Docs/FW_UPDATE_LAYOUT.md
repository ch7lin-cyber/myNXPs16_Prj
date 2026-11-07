# APP 更新配套：DirectUpdate

APP 分支：**branch2_ver2_directUpdate**。搭配 Bootloader：`ch7lin-cyber/myNXP_bootLoader` 的 **006_NXP_55S16_IAP_DirectUpdate**。
Bootloader 版本：00.02.01。C#：`004_WindowsForms_CSharp` **0.4.6 / FileVersion 0.4.6.0**，同一支 EXE 支援兩模式。

| 項目 | 設定 |
|---|---|
| Bootloader | 0x00000..0x08000（終點不包含） |
| APP 起點 | 0x8000 |
| APP 分區大小 | 0x34000 / 208 KiB |
| APP 終點（不包含） | 0x3C000 |
| APP BIN 最大大小 | **212992 bytes** |
| Metadata 保留 | 0x3C000..0x3C400 |
| 其他 Flash | 本次不擴用 |
| SRAMX | 0x04001000..0x04003FC0；前4KiB MCAN，尾64B mailbox |

## 建置與下載

1. `git clone --branch branch2_ver2_directUpdate --recurse-submodules <APP-repository-url>`，或已 clone 後 `git submodule update --init --recursive`。必須使用儲存庫固定的 submodule commit。
2. 匯入 MCUXpresso，確認 `.cproject` 的 MCU Settings / PROGRAM_FLASH 起點0x8000、大小0x34000；Debug/Release 共用該記憶體配置。
3. `_Product/linker/app_update_assert.ld` 同時檢查 APP 起點、容量、payload 終點、MCAN 及 mailbox；不要只在 IDE 改大小而忽略這個 ASSERT，也不要手改 Debug 下自動生成的 linker script。
4. Clean/Rebuild Debug 或 Release，檢查 MAP，再用上述配套 C# 選 APP `.bin` 下載。Bootloader 請依其分支 `_Bootloader/docs/update_mode.md` 先用 SWD 建置/燒錄。
5. 若透過 IDE 直接下載 APP，只會寫入程式，不代表 Bootloader 的 ACTIVE metadata 已建立。正式驗證以 C# 完成更新，再按啟動 APP、斷電重啟確認。

Direct：邊接收邊覆寫 APP；FINISH 驗證通過才標記 ACTIVE。沒有 staging/PENDING 續裝，中斷後重啟至 RS485 recovery 再完整下載。OTA1 header 由 C# 解析，不佔 APP Flash。

## 本次修改範圍

同步修改 `.cproject` PROGRAM_FLASH size 與 `_Product/linker/app_update_assert.ld`；PID、量測與既有 FwUpdateApp 通訊介面不變。
APP USB/RS485 切換流程沿用共用庫；本文件不會把已停用的 USB 重新啟用，是否含 USB 依實際建置配置。

已檢查 `.cproject` 與 linker 邊界一致；APP 的完整 ARM 編譯/連結及板上更新待實測。其他既有建置相依（例如 CKLIB）不由此容量修改解決。


## 專案清理

使用者已確認基準可編譯／燒錄且APP啟動成功。本次只清除根目錄Debug/Release的已追蹤輸出，新增忽略規則並更新配套分支名稱；APP .cproject、linker邊界、runtime及PlatformCore版本不變。請在IDE Clean/Rebuild重新產生本機輸出。

## 簽章／信任根預留（Bootloader 00.02.01）

兩份未啟用的簽章驗證紀錄：副本0 `0x3C400..0x3CA00`、副本1 `0x3CA00..0x3D000`，各1536 bytes。APP仍從0x8000到0x3C000，208 KiB不變；metadata兩頁不變。補充linker以絕對符號顯示保留區，不產生額外APP資料段。

公鑰信任根雜湊依NXP PFR/CMPA保護機制另作製造佈署；韌體簽署私鑰僅在離線簽署端/HSM，不放APP/Bootloader/C#。這次不寫PFR、不存金鑰、不生成簽章，也沒有啟用驗簽或防降版；既有C# 0.4.6.0與BIN仍可用。保留區目前只由Bootloader的一般更新寫入白名單排除，不是硬體讀寫保護。

完整格式、憑證容量限制與後續工作請看Bootloader的 `_Bootloader/docs/security_reservation.md`。
