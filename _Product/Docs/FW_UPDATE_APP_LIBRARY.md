# APP 更新入口公用庫整合與重測

本次將已定版功能抽成 `_Modules/FwUpdateApp`，在 `branch2_ver1` 透過原 `ProductFwUpdate_*` API 接回。
詳細新 project 接法見 [FwUpdateApp README](../../_Modules/FwUpdateApp/README.md)。

## 本產品變更

- `ProductFwUpdateConfig.h`：集中身分、USB／RS485 route 及 200 ms 切換等待配置。
- `product_fw_update.c`：保留產品 API，改為轉交公用庫；板級停機及 reset 函式繼續由既有 board adapter 提供。
- `_Product/Shared/boot_request.h`：相容轉接 header，canonical ABI 移入公用庫，內容／CRC 與既有 Bootloader 一致。
- `.cproject`：Debug／Release 都編譯 `FwUpdateApp/src`，不編譯公用庫 tests。
- 更新既有 Host 測試的連結來源，避免抽出後出現 undefined reference。

UI 與 Bootloader 沿用這次已定版版本；三個 U16 APP 版本、更新 registers、鮑率 code、Linker／Flash 邊界均維持既有配置。
這次沒有加入簽章或新的安全協議；安全更新另行實作。

## 已完成檢查

1. 原產品 request CRC／route／廣播拒絕／重複請求／ACK／200 ms／uint32 回繞／停機失敗回歸：PASS。
2. SDK-free 庫測試：六種 RS485 鮑率、USB 115200、配置與多實例隔離、非法參數、缺少 callback、reset 意外返回：PASS。
3. 原 USB Host RTU 測試連結新的庫：分包／合包、讀寫、CRC、廣播、session、TX retry 與 IN completion 後 reset：PASS。
4. Debug／Release source entry 與既有 Flash／SRAMX 分區設定檢查：PASS。

本次環境沒有 MCUXpresso／ARM 工具鏈，未產生新的 APP BIN，也未執行完整 SDK syntax／factory calibration 全套測試。
未更動的通訊及板級程式仍需與新 APP 一起做以下實機回歸。

## 使用者重測

1. 更新 `branch2_ver1`，保留目前 Bootloader 與 UI。MCUXpresso Clean/Rebuild，確認 `.map` 沒有 overflow，APP BIN 位於既有分區內。
2. 透過目前定版 UI，使用 USB CDC 燒錄新的 APP BIN，啟動 APP、讀版本、進 updater、再次啟動 APP；重複兩次。
3. 使用 RS485 FC0，至少各測 115200 與 921600 更新，再啟動 APP／重新進 updater，確認 Bootloader 回報選定速率。
4. 更新命令觸發時，確認控制／PWM／DO／DAC 依原產品安全停機規則停止；重啟後確認 APP 正常工作。
5. APP 安裝完成後重新上電，確認既有 Bootloader 啟動政策及 APP 行為與定版一致。

PENDING 續裝／安裝途中斷電恢復仍依之前的獨立驗證計畫執行；本次公用庫重構不代表已完成那些實測。
