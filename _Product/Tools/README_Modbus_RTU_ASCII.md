# Modbus RTU/ASCII 診斷工具

適用分支：`branch2_ver1`。在專案根目錄執行：

```powershell
python -m pip install pyserial
python _Product\Tools\modbus_rtu_test_menu.py --port COM17 --slave 2 --baud 115200 --timeout 3
```

| 參數 | 作用 |
|---|---|
| `--port COM17` | DUT port |
| `--slave 2` | Modbus 站號 |
| `--mode rtu` / `--mode ascii` | 通訊模式；省略時啟動選擇，Enter 預設 RTU |
| `--bytesize 7` / `8` | 資料位元；RTU 預設 8，ASCII 預設 7 |
| `--parity N` / `E` / `O` | 同位檢查；RTU 預設 N，ASCII 預設 E |
| `--stopbits 1` / `2` | 停止位元；預設 1 |
| `--baud 115200` | Baud rate |
| `--timeout 3` | 回覆逾時 3 秒；未指定時預設 1 秒 |

ASCII 範例（7E1）：

```powershell
python _Product\Tools\modbus_rtu_test_menu.py --port COM17 --slave 2 --baud 115200 --mode ascii --timeout 3
```

RTU 預設 8N1，ASCII 預設 7E1；可用上述參數覆寫，必須與 DUT 的協定及 UART 格式一致。選擇工具模式不會自動切換 MCU 協定。ASCII 使用冒號、HEX 字元、LRC 與 CRLF；RTU 使用 CRC。兩種模式共用診斷與 FC03/06/10 讀寫功能。為相容現有 CALYS 工具匯入，保留檔名 `modbus_rtu_test_menu.py`。

此工具每筆請求送出前固定等待 **50 ms**。目前主選單如下：

```text
========== Modbus RTU/ASCII 診斷工具 ==========
03. Read arbitrary holding registers
06. Write holding register(s)
c1. Read channel PV and ADC diagnostics
c2. Continue read register
q. Exit
==========================================
```

- `03`：FC03 讀取任意 Holding Registers，可指定起始位址與數量。
- `06`：寫入一筆 FC06 或多筆 FC10 Registers，送出前需確認。
- `c1`：讀取 CH0～CH3 PV、Sensor/CJC 設定與 ADC 診斷。
- `c2`：設定連續讀取間隔及最多 10 個位址、檢視設定並啟動讀取；子選單沿用 `r1.1`～`r1.4`，也可輸入 `1`～`4`，按 `q` 返回。
- `q`：離開工具。

輸入位址支援 `0x1000`、`1000h` 等十六進位格式。

**這支工具不支援 `--cjc DIS`**，不會設定 CALYS，也不會變更 DUT 的 CJC。加入該參數會出現不認得參數的錯誤。

