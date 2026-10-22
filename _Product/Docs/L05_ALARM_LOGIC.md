# L05 Alarm／Logic 串接

## 摘要

**尚未完成：正式 PID callback、Register、實體輸出與 MCU 實測仍待接入。** 本版完成的是 Alarm／Logic 資料鏈及 Host 驗證，不代表完整產品控制功能完成。

2026-10-22：已建立 L03 溫度 Snapshot／DI → L05 adapter → L04 Alarm／Logic → L05 結果查詢的功能鏈，接入 ProductApplication 原有的主迴圈排程。預設容量為 4 通道 × 每通道 3 組 Alarm，共 12 組；Logic 共 16 組。週期 20 ms，全部實例預設停用，須由產品設定 API 啟用。

已完成嚴格 C99／O2、ASan／UBSan Host 測試；另以 2 通道 × 2 組 Alarm、4 組 Logic 編譯並通過，確認容量可由產品調整，不修改 L04。原 CH0 MovingAverage 整合測試仍通過，並修正其未初始化 ENABLE 引腳的問題。

本次完成記憶體中的運算與查詢，不直接驅動 DO／PWM／DAC、不發布 SYSTEM 事件、不新增 Modbus／NVM 欄位。PID 呼叫位置已建立，但正式 PID callback 尚未註冊；不能宣稱已完成 PID 閉迴路整合。尚待 MCU 實測。

## 資料與執行順序

同一個 Scheduler callback 依序完成：

1. 每通道讀一次溫度 Snapshot，更新該通道的三組 Alarm。
2. Logic 前半組依編號由小到大執行，預設為人員編號 1～8。
3. 若已註冊 PID callback，呼叫一次並傳入實際 elapsed_ms。
4. Logic 後半組依編號由小到大執行，預設為人員編號 9～16。

這段鏈放在產品排程表最前面；原 MovingAverage 仍使用原有排程項目。此順序只保證本鏈內的 PID callback 位置，不能將其他位置執行的控制運算也視為位於此位置。日後接入 ProductPidControllerAdapter_Process 時，須註冊此 callback，避免另一個排程項目重複呼叫 PID。L05 不直接引用 CKLIB。

所有 C API 編號皆從 0 起算：CH0～3、Alarm0～2、Logic0～15；文件／UI 的 Alarm1～3、Logic1～16 須由外層換算。

## 容量與時間設定

修改 [_Product/Config/ProductAlarmLogicConfig.h](../Config/ProductAlarmLogicConfig.h)，或在所有相關編譯單元一致提供巨集：

| 參數 | 預設值 | 意義 |
|---|---:|---|
| PRODUCT_AL_CHANNEL_COUNT | 4 | Alarm 輸入通道數。 |
| PRODUCT_AL_ALARMS_PER_CHANNEL | 3 | 每通道獨立 Alarm 實例數。 |
| PRODUCT_AL_LOGIC_COUNT | 16 | Logic 實例總數，不是每通道組數。 |
| PRODUCT_AL_PRE_LOGIC_COUNT | 8 | 放在 PID callback 前的 Logic 組數。 |
| PRODUCT_AL_PERIOD_MS | 20 | 整條 Alarm／Logic 鏈的週期。 |
| PRODUCT_AL_MAX_SAMPLE_AGE_MS | 200 | 同一 sample_sequence 允許的最長觀察時間；0 關閉過期判斷。 |

通道及組數限 1～255，前半組數可為 0～Logic 總數。容量改變須重新編譯。其他 Project 仍需依實際 Sensor 數量實作 provider；增加陣列容量不會自動增加硬體通道。配置與命令只在主迴圈呼叫，不可從 ISR 或 callback 執行中修改。

## 溫度來源與有效性

正式 provider 呼叫 ProductSensorMeasurementService_GetSnapshot()，直接傳 engineering_value（0.001°C）；只接受已列舉的 TC／RTD sensor_type。電壓、電流、OFF 及未知 Sensor 類型視為無效，避免不同單位混入溫度門檻。

Sample sequence 改變時觀察年齡歸零；相同 sequence 仍每個週期執行 FB，以推進延遲。觀察年齡超過上限、Snapshot 取得失敗或 valid=false 時，Alarm 依 L04 無效輸入契約立即 OFF／清除執行狀態，峰值保留。新樣本恢復後重新待機及計時。這是從 L05 觀察到的年齡，非 Sensor 擷取時間戳；初始化第一次讀取視為新樣本。

SV 由 SetSetpoint() 提供，單位同為 0.001°C，預設 0。啟用相對或遲滯模式前，產品須明確設定 SV；不自動採用 PID SV。

## 設定與結果 API

由 ProductFbScheduleIntegration_GetAlarmLogicAdapter() 取得產品實例，再使用 [product_alarm_logic_adapter.h](../Source/L05_Application/product_alarm_logic_adapter.h) 的 API。回傳 false 表示設定／索引不合法或命令佇列已滿，呼叫端必須檢查。

| API | 用途 |
|---|---|
| SetAlarm(adapter, channel, alarm, config, enabled) | 驗證後複製單組 Alarm 設定；新設定在下一個 callback 套用。 |
| SetSetpoint(adapter, channel, sv) | 更新該通道所有 Alarm 使用的 SV。 |
| Command(adapter, channel, alarm, reset, clear_peak) | 提出獨立 Reset／Peak Clear 命令；不會直接改寫結果。 |
| SetLogic(adapter, logic, config, sources, enabled) | 驗證四個來源，複製設定並清除舊結果／計時；下一個 callback 執行。 |
| SetPidStep(adapter, callback, context) | 註冊中間的 PID 執行位置；NULL 取消。 |
| GetAlarm(adapter, channel, alarm, output) | 取得該 Alarm 最近一次完整掃描結果，包含鎖存、峰值及 status。 |
| GetLogic(adapter, logic, output, valid) | 取得 Logic 結果及來源有效性；valid=false 時 value 強制 0。 |

Alarm 設定先用獨立暫存 L04 實例驗證，不會動到實際實例；非法設定不覆蓋舊設定。L04 實際設定變更時依契約清除狀態與峰值。Logic 每次 SetLogic 都會重新初始化該組，避免來源改變卻延用原計時；不應每週期重複設定。

## Logic 來源

| kind | index／channel | 來源 |
|---|---|---|
| ZERO／ONE | 不使用 | 固定 0／1，供未使用輸入或常數使用。 |
| ALARM | channel＋index | 該組 Alarm 的最終 alarm_active，包含反向；不是單純比較結果。 |
| LOGIC | index | 指定 Logic 的最終 value。 |
| EXTERNAL | index | 正式產品綁定 L03 DigitalInput Snapshot 的 bit；valid_mask 必須有效。 |

預設四個輸入皆為 ZERO。AND 未使用輸入須設 ONE（並考慮輸入反向）。讀取較低編號 Logic 時可使用本週期結果；較高編號則讀前週期結果及有效旗標。來源尚未執行或被重新配置後，可能至少等待一週期才有效；不要建立沒有有效初始來源的循環相依。

任一必要來源無效，整組 Logic 以 enable=false 呼叫 L04，輸出 0 並回報 valid=false，連帶讓下游停用。停用組或 OFF 的輸出為有效常數 0，不要求來源有效。Alarm 的 INVALID_INPUT／INVALID_CONFIGURATION 等錯誤結果不當作正常布林值；RESET_REJECTED 則仍是有效警報狀態。

## Reset／Peak Clear 命令

每組各有一個 pending Reset、一個 pending Peak Clear；同一待處理命令重複提出會回 false，不會無聲合併。兩個不同命令可同時提出，亦可同次執行。

Adapter 保證兩次相同命令之間插入一次低電位掃描，讓 L04 看到新上升緣。若前一週期剛送出命令，下一個新命令可能等待一個掃描，再於下次執行。每組互不影響。取得命令佇列成功不等於重置成功：實際結果看 GetAlarm().status；異常仍存在會回 RESET_REJECTED。

停用或來源失效期間的命令也會送入 L04 並被消費，結果依當次 DISABLED／INVALID_INPUT 回報，不會在恢復時補做 Reset。清峰值在停用期間仍依 L04 契約有效。未建立 Modbus bit 解碼；未來每通道 Alarm Reset／Peak Clear 各 bit 應轉成對應單組 API。

## 使用範例

在 ProductFbScheduleIntegration_Init() 成功後、主迴圈設定階段執行：

```c
ProductAlarmLogicAdapter_t *adapter =
    ProductFbScheduleIntegration_GetAlarmLogicAdapter();
FbAlarmConfig_t alarm;
FbLogicConfig_t logic;
ProductAlSource_t source[FB_LOGIC_INPUT_COUNT] = {{0}};

FbAlarm_DefaultConfig(&alarm);
alarm.mode = FB_ALARM_MODE_ABSOLUTE_HIGH;
alarm.alarm_high = 100000; /* 100.000 C */
alarm.reset_mode = FB_ALARM_RESET_MANUAL;
alarm.options = FB_ALARM_OPTION_PEAK;
alarm.delay_ms = 2000U;
if (!ProductAlarmLogicAdapter_SetAlarm(adapter, 0U, 0U, &alarm, true))
    return; /* 由產品回報設定錯誤 */

FbLogic_DefaultConfig(&logic);
logic.operator_type = FB_LOGIC_OPERATOR_OR;
source[0].kind = PRODUCT_AL_SOURCE_ALARM;
source[0].channel = 0U;
source[0].index = 0U;
if (!ProductAlarmLogicAdapter_SetLogic(adapter, 0U, &logic, source, true))
    return;
```

## Host 驗證與限制

在 repository 根目錄執行：

```sh
python3 _Product/Tests/run_alarm_logic_tests.py
python3 _Product/Tests/run_alarm_logic_tests.py --sanitize
```

| 測項 | 結果 |
|---|---|
| 全部 12 Alarm／16 Logic 可配置與獨立運作，排程未到期不執行 | PASS |
| Alarm → 前半 Logic → PID 測試 callback → 後半 Logic 順序 | PASS |
| 手動 Reset 拒絕／接受、獨立清峰值、重複命令回報及低掃描間隔 | PASS |
| 通道隔離、DI／Logic 來源、無效 DI 與下游傳播及恢復 | PASS |
| 非溫度單位拒絕、樣本過期／取得失敗及恢復 | PASS |
| ON／OFF 實際時間邊界、較高編號來源延後一週期 | PASS |
| 非法設定／索引／NULL 拒絕 | PASS |
| 原 CH0 MovingAverage 回歸，ENABLE 正確初始化 | PASS |
| 2 通道 × 2 Alarm／4 Logic 替代容量 | PASS |

測試以 mock 替代 Sensor／DI，以測試 callback 替代 PID，未使用 CKLIB 實體庫。`.cproject` 的 Debug／Release 已納入 FbAlarm.c／FbLogic.c 與新的 L05 檔案所在來源路徑；MCUXpresso 完整建置仍需要使用者提供實體 CKLIB archive（目前為空檔），本次不宣稱完整 MCU link 成功。
