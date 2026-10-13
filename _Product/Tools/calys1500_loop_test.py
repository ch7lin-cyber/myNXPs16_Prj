#!/usr/bin/env python3
"""Interactive CALYS1500 / S16 loop test and guided factory calibration."""
import argparse
import csv
import json
import math
import time
from datetime import datetime, timezone
from pathlib import Path

from calys1500_protocol import Calys1500, SENSORS, numeric, ten_points, validate
from calys1500_expected import add_comparison, format_summary
from calys1500_calibration import WIRING_GROUPS, PROFILE_LABELS, guided_profile_run

# No serial dependency is required to import the catalog or run --list-sensors.
def modbus_helpers():
    from modbus_rtu_test_menu import (ModbusRtuClient, float32_from_registers,
                                      int32_from_registers, uint32_from_registers)
    return ModbusRtuClient, float32_from_registers, int32_from_registers, uint32_from_registers


def parse_wiring_group_selection(text):
    """Return the internal wiring-group key from a displayed 2.x choice."""
    value = text.strip().lower()
    if value == "q":
        return None
    if value.startswith("2."):
        value = value[2:]
    if value not in WIRING_GROUPS:
        raise ValueError("接線類別必須是 2.1、2.2、2.3 或 2.4")
    return value


def parse_profile_selection(text):
    """Accept the displayed Pn profile ID as well as the legacy bare number."""
    value = text.strip().upper()
    if value == "Q":
        return None
    if value.startswith("P"):
        value = value[1:]
    if not value.isdigit():
        raise ValueError("Profile ID 格式必須為 P0、P1 等")
    return int(value)


FIELDS = ["utc", "point", "channel", "sensor_code", "sensor", "gain", "setpoint",
          "unit", "pv", "input_error", "raw_code", "microvolts", "successful_samples",
          "calys_idn", "cjc", "fixed_cjc", "current_supply", "dwell_seconds",
          "dut_read_retries", "status", "error", "pv_signal", "pv_signal_unit",
          "expected_pv", "pv_delta", "pv_native_delta", "expected_adc_uv",
          "adc_delta_uv", "adc_reference_basis", "calys_cjc_temperature", "dut_cjc_source"]


class DutLink:
    """Pace requests; retry only read transport errors, never writes/apply."""
    def __init__(self, client, retries=2, request_delay=0.05):
        self.client = client
        self.retries = retries
        self.request_delay = request_delay
        self.read_retries = 0

    def read_holding_registers(self, address, quantity):
        for attempt in range(self.retries + 1):
            time.sleep(self.request_delay if attempt == 0 else max(0.15, self.request_delay))
            try:
                return self.client.read_holding_registers(address, quantity)
            except RuntimeError as exc:
                # A valid Modbus exception is a DUT rejection, not a lost frame.
                retryable = any(message in str(exc) for message in (
                    "response timeout", "response is incomplete", "response CRC error"))
                if not retryable or attempt == self.retries:
                    raise
                self.read_retries += 1
                print(f"DUT 讀取 0x{address:04X} 重試 {attempt + 1}/{self.retries}：{exc}")

    def write_multiple_registers(self, address, values):
        time.sleep(self.request_delay)
        return self.client.write_multiple_registers(address, values)

    def write_single_register(self, address, value):
        time.sleep(self.request_delay)
        return self.client.write_single_register(address, value)

    def close(self):
        self.client.close()


def list_sensors():
    for s in SENSORS.values():
        print(f"{s.code:3}  {s.label:12} {s.low:5}～{s.high:<5} {s.unit:4} Gain={s.gain}")
    print("_X：尚無已驗證的直接指令／曲線對映，禁止自動輸出。")


def select_sensor():
    list_sensors()
    code = int(input("Sensor type 代碼："))
    if code not in SENSORS:
        raise ValueError("不存在的 Sensor type")
    sensor = SENSORS[code]
    validate(sensor, sensor.low)
    return sensor


def sample_count(dut, channel):
    _, _, _, u32 = modbus_helpers()
    return u32(dut.read_holding_registers(0x4831 + channel * 0x40, 2))


def configure_dut(dut, channel, sensor, wait_seconds):
    if channel not in range(4):
        raise ValueError("通道必須是 0～3")
    validate(sensor, sensor.low)
    base = 0x1000 + channel * 0x10
    # Preserve the DUT CJC source when switching Sensor type.
    cjc_source = dut.read_holding_registers(base + 8, 1)[0]
    if cjc_source not in range(4):
        raise RuntimeError("DUT CJC source 讀回無效")
    dut.write_multiple_registers(base + 7, [sensor.code, cjc_source, 0xA5A5])
    time.sleep(wait_seconds)
    if dut.read_holding_registers(base + 7, 1)[0] != sensor.code:
        raise RuntimeError("DUT Sensor type 讀回不符")
    # Active type readback acknowledges the event was queued, not HAL completion.
    # run_sweep additionally requires a new successful ADC sample at each point.


def run_sweep(calys, dut, channel, sensor, path, dwell=2.0,
              fresh_timeout=10.0, settings=None):
    settings = settings or {}
    _, f32, i32, u32 = modbus_helpers()
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    rows = []
    completed = False
    try:
        _run_sweep_points(calys, dut, channel, sensor, path, dwell, fresh_timeout, settings, rows)
        completed = True
    finally:
        summary = format_summary(rows, sensor, completed)
        summary_path = path.with_name(path.stem + "_summary.txt")
        summary_path.write_text(summary, encoding="utf-8-sig")
        print("\n" + summary)
        print("CSV：", path.resolve())
        print("結果摘要：", summary_path.resolve())
    return path


def _run_sweep_points(calys, dut, channel, sensor, path, dwell, fresh_timeout, settings, rows):
    _, f32, i32, u32 = modbus_helpers()
    dut_cjc_source = dut.read_holding_registers(0x1008 + channel * 0x10, 1)[0]
    with path.open("w", newline="", encoding="utf-8-sig") as stream:
        writer = csv.DictWriter(stream, fieldnames=FIELDS)
        writer.writeheader()
        stream.flush()
        for index, point in enumerate(ten_points(sensor), 1):
            retries_before = getattr(dut, "read_retries", 0)
            row = dict(utc=datetime.now(timezone.utc).isoformat(), point=index,
                       channel=channel, sensor_code=sensor.code, sensor=sensor.name,
                       gain=sensor.gain, setpoint=str(point), unit=sensor.unit,
                       calys_idn=calys.identity, cjc=settings.get("cjc", "INT"),
                       fixed_cjc=settings.get("fixed_cjc", "0"),
                       current_supply=settings.get("current_supply", "ON"),
                       calys_cjc_temperature=settings.get("calys_cjc_temperature", ""),
                       dut_cjc_source=dut_cjc_source,
                       dwell_seconds=dwell, status="recorded", error="")
            try:
                calys.output(sensor, point)
                time.sleep(dwell)
                before = sample_count(dut, channel)
                deadline = time.monotonic() + fresh_timeout
                while True:
                    current_count = sample_count(dut, channel)
                    if current_count != before:
                        break
                    if time.monotonic() >= deadline:
                        raise TimeoutError("等待新成功樣本逾時")
                    time.sleep(0.1)
                pv = dut.read_holding_registers(0x1000 + channel * 0x10, 3)
                row.update(pv=f32(pv[:2]), input_error=pv[2],
                           successful_samples=current_count)
                raw = dut.read_holding_registers(0x484F + channel * 0x40, 6)
                row.update(raw_code=f"0x{u32(raw[:2]):08X}", microvolts=i32(raw[4:6]))
                add_comparison(row, sensor, point, settings)
                if row["input_error"] != 61:
                    row.update(status="input_error", error=f"DUT input_error={row['input_error']}")
                print(f"[{index}/10] 輸出 {point} {sensor.unit} → PV={row['pv']}, "
                      f"error={row['input_error']}, ADC={row['microvolts']} uV")
            except BaseException as exc:
                row.update(status="interrupted" if isinstance(exc, KeyboardInterrupt) else "error",
                           error=str(exc) or type(exc).__name__,
                           dut_read_retries=getattr(dut, "read_retries", 0) - retries_before)
                rows.append(row)
                writer.writerow(row)
                stream.flush()
                raise
            row["dut_read_retries"] = getattr(dut, "read_retries", 0) - retries_before
            rows.append(row)
            writer.writerow(row)
            stream.flush()


def positive(text):
    value = float(text)
    if not math.isfinite(value) or value <= 0:
        raise argparse.ArgumentTypeError("必須是有限的正數")
    return value


def nonnegative(text):
    value = float(text)
    if not math.isfinite(value) or value < 0:
        raise argparse.ArgumentTypeError("必須是有限的非負數")
    return value


def retry_count(text):
    value = int(text)
    if not 0 <= value <= 10:
        raise argparse.ArgumentTypeError("重試次數必須介於 0～10")
    return value


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--calys-port")
    parser.add_argument("--dut-port")
    parser.add_argument("--dut-baud", type=int, default=115200)
    parser.add_argument("--dut-timeout", type=positive, default=2.0)
    parser.add_argument("--dut-read-retries", type=retry_count, default=2)
    parser.add_argument("--dut-request-delay", type=nonnegative, default=0.05)
    parser.add_argument("--slave", type=int, choices=range(1, 248), default=2,
                        metavar="1..247")
    parser.add_argument("--dwell", type=positive, default=2.0)
    parser.add_argument("--fresh-timeout", type=positive, default=10.0)
    parser.add_argument("--calibration-timeout", type=positive, default=30.0,
                        help="自動校正每一點等待穩定樣本的逾時")
    parser.add_argument("--calibration-stability-uv", type=positive, default=20.0,
                        help="穩定視窗最大 µV 範圍")
    parser.add_argument("--allow-calibration-apply", action="store_true",
                        help="允許進入 NVM Apply；仍需逐筆輸入 APPLY_CHn_PROFILE_p 確認")
    parser.add_argument("--calys-timeout", type=positive, default=5.0)
    parser.add_argument("--calys-write-delay", type=nonnegative, default=0.15,
                        help="CALYS 每筆指令後的等待秒數（預設 0.15）")
    parser.add_argument("--calys-trace", action="store_true",
                        help="顯示 CALYS TX/RX，協助檢查 port 與回應")
    parser.add_argument("--cjc", choices=["INT", "DIS", "FIX"], default="INT")
    parser.add_argument("--fixed-cjc", default="0")
    parser.add_argument("--calys-cjc-temperature", type=float,
                        help="INT模式時CALYS面板顯示的冷端溫度(degC)，供理論ADC比較；不改儀器設定")
    parser.add_argument("--current-supply", choices=["ON", "OFF"], default="ON")
    parser.add_argument("--results", type=Path, default=Path("calys_results"))
    parser.add_argument("--config", type=Path, default=Path("calys1500_ports.json"))
    parser.add_argument("--list-sensors", action="store_true")
    args = parser.parse_args(argv)
    numeric(args.fixed_cjc)
    if args.calys_cjc_temperature is not None and not math.isfinite(args.calys_cjc_temperature):
        parser.error("--calys-cjc-temperature 必須是有限數值")
    if args.list_sensors:
        list_sensors()
        return
    saved = json.loads(args.config.read_text(encoding="utf-8")) if args.config.exists() else {}
    calys_port = args.calys_port or saved.get("calys_port", "")
    dut_port = args.dut_port or saved.get("dut_port", "")
    selected, channel, calys = None, None, None
    processed_calibration_profiles = set()
    settings = dict(cjc=args.cjc, fixed_cjc=args.fixed_cjc, current_supply=args.current_supply,
                    calys_cjc_temperature=args.calys_cjc_temperature)

    def save_ports():
        args.config.parent.mkdir(parents=True, exist_ok=True)
        args.config.write_text(json.dumps(dict(calys_port=calys_port, dut_port=dut_port),
                                          indent=2), encoding="utf-8")

    def connect():
        nonlocal calys, calys_port
        if calys is None:
            if not calys_port:
                calys_port = input("CALYS 1500 port：").strip()
            print(f"正在連線 CALYS：{calys_port}，115200/8N1")
            calys = Calys1500(calys_port, args.calys_timeout,
                              write_delay=args.calys_write_delay,
                              trace=args.calys_trace)
            print(calys.identity)
        return calys

    try:
        while True:
            print("\n0 CALYS 1500 相關")
            print("    [0.1.0] 自動通訊測試")
            print("    [0.1.1] 改 CALYS port")
            print("  [0.2] 改 DUT port")
            print("  [0.3] 改 CALYS Sensor type")
            print("  [0.4] 設定輸出值")
            print("\n[1] 選通道／Sensor（直接寫 DUT）")
            print("    [1.1.0] 十點自動測試")
            print("\n[2] 自動校正（依接線類別；預設只預覽）／選通道")
            print("  [2.1] 電壓／mV")
            print("  [2.2] RTD 三線")
            print("  [2.3] 熱電偶 TC")
            print("  [2.4] 電流")
            print("q 結束")
            print(f"CALYS={calys_port or '未指定'} DUT={dut_port or '未指定'} "
                  f"CH={channel} Sensor={selected.label if selected else '未選'}")
            action = input("選項：").strip()
            try:
                if action == "q":
                    break
                if action == "0":
                    continue
                if action == "0.1.0":
                    # Only probe the designated calibrator port; do not send SCPI
                    # to arbitrary devices sharing the computer's serial ports.
                    device = connect()
                    print("通訊成功：", device.query("*IDN?"))
                    save_ports()
                elif action == "0.1.1":
                    if calys:
                        calys.close()
                        calys = None
                    from serial.tools import list_ports
                    print("可用 ports：", ", ".join(p.device for p in list_ports.comports()))
                    calys_port = input("CALYS port：").strip()
                    save_ports()
                elif action == "0.2":
                    dut_port = input("DUT port：").strip()
                    save_ports()
                elif action == "0.3":
                    sensor = select_sensor()
                    connect().configure(sensor, **{key: settings[key] for key in ("cjc", "fixed_cjc", "current_supply")})
                    selected = sensor
                elif action == "0.4":
                    if selected is None:
                        raise ValueError("請先選擇 Sensor")
                    value = validate(selected, input(f"輸出值 ({selected.unit})："))
                    connect().configure(selected, **{key: settings[key] for key in ("cjc", "fixed_cjc", "current_supply")})
                    connect().output(selected, value)
                elif action == "1":
                    candidate = int(input("通道 0／1／2／3："))
                    if candidate not in range(4):
                        raise ValueError("通道必須是 0～3")
                    sensor = select_sensor()
                    if not dut_port:
                        dut_port = input("DUT port：").strip()
                    if not dut_port:
                        raise ValueError("請指定 DUT port")
                    if dut_port.casefold() == calys_port.casefold() and calys_port:
                        raise ValueError("CALYS 與 DUT 必須使用不同 port")
                    cls, *_ = modbus_helpers()
                    dut = DutLink(cls(dut_port, args.slave, args.dut_baud, args.dut_timeout),
                                  args.dut_read_retries, args.dut_request_delay)
                    try:
                        configure_dut(dut, candidate, sensor, args.dwell)
                    finally:
                        dut.close()
                    # Commit the menu selection only after write/apply and readback.
                    channel, selected = candidate, sensor
                    save_ports()
                    print(f"DUT CH{channel} Sensor type 已寫入並讀回確認："
                          f"{selected.label} ({selected.code}, 0x{selected.code:04X})；"
                          "已送 Apply Key，ADC 套用／量測狀態請查看診斷。")
                elif action == "1.1.0":
                    if selected is None or channel is None:
                        raise ValueError("請先用選單 1 選通道及 Sensor")
                    if not dut_port:
                        dut_port = input("DUT port：").strip()
                    if dut_port.casefold() == calys_port.casefold() and calys_port:
                        raise ValueError("CALYS 與 DUT 必須使用不同 port")
                    device = connect()
                    device.configure(selected, **{key: settings[key] for key in ("cjc", "fixed_cjc", "current_supply")})
                    cls, *_ = modbus_helpers()
                    dut = DutLink(cls(dut_port, args.slave, args.dut_baud, args.dut_timeout),
                                  args.dut_read_retries, args.dut_request_delay)
                    path = args.results / (datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S_%fZ")
                                          + f"_CH{channel}_{selected.code}.csv")
                    print("十點：", ", ".join(str(p) for p in ten_points(selected)))
                    print("CSV：", path.resolve())
                    try:
                        configure_dut(dut, channel, selected, args.dwell)
                        run_sweep(device, dut, channel, selected, path, args.dwell,
                                  args.fresh_timeout, settings)
                        save_ports()
                    finally:
                        dut.close()
                elif action == "2":
                    candidate = int(input("校正通道 0／1／2／3："))
                    if candidate not in range(4):
                        raise ValueError("通道必須是 0～3")
                    for key, group in WIRING_GROUPS.items():
                        print(f"[2.{key}] {group.title}：{group.wiring}")
                    group_key = parse_wiring_group_selection(
                        input("接線類別 [2.1] / [2.2] / [2.3] / [2.4]；q 返回："))
                    if group_key is None:
                        continue
                    group = WIRING_GROUPS[group_key]
                    print(f"\n{group.title} 接線：{group.wiring}")
                    if input("確認已依上方說明接線 (YES/q)：").strip() != "YES":
                        print("未確認接線，取消校正。")
                        continue
                    if not dut_port:
                        dut_port = input("DUT port：").strip()
                    if not dut_port:
                        raise ValueError("請指定 DUT port")
                    if dut_port.casefold() == calys_port.casefold() and calys_port:
                        raise ValueError("CALYS 與 DUT 必須使用不同 port")
                    device = connect()
                    cls, *_ = modbus_helpers()
                    dut = DutLink(cls(dut_port, args.slave, args.dut_baud, args.dut_timeout),
                                  args.dut_read_retries, args.dut_request_delay)
                    try:
                        print("本類別的唯一 Firmware Profiles：")
                        for profile, sensor_codes in group.profiles.items():
                            labels = ", ".join(SENSORS[code].label for code in sensor_codes)
                            print(f"  [P{profile}] {PROFILE_LABELS[profile]}：{labels}")
                        while True:
                            profile = parse_profile_selection(
                                input("要校正的 Profile ID（例如 P0；q 返回）："))
                            if profile is None:
                                break
                            if profile not in group.profiles:
                                raise ValueError("此接線類別不包含該 Profile")
                            profile_key = (candidate, profile)
                            if profile_key in processed_calibration_profiles:
                                print(f"CH{candidate} Profile {profile} 本次已處理；"
                                      "依操作員選擇重新校正。")
                            choices = group.profiles[profile]
                            print("此 Profile 的可選 Sensor：", ", ".join(
                                f"{code}:{SENSORS[code].label}" for code in choices))
                            code = int(input("選一個代表此共用 Profile 的 Sensor code："))
                            if code not in choices:
                                raise ValueError("Sensor 不屬於所選 Profile／接線類別")
                            sensor = SENSORS[code]
                            if sensor.token is None:
                                raise ValueError(f"{sensor.label} 尚無已驗證 CALYS 輸出對映")
                            configure_dut(dut, candidate, sensor, args.dwell)
                            report = guided_profile_run(
                                device, dut, candidate, group, profile, sensor,
                                allow_apply=args.allow_calibration_apply,
                                dwell=args.dwell, stable_timeout=args.calibration_timeout,
                                spread_limit_uv=args.calibration_stability_uv,
                                settings={key: settings[key] for key in
                                          ("cjc", "fixed_cjc", "current_supply")})
                            processed_calibration_profiles.add(profile_key)
                            path = args.results / (
                                datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S_%fZ")
                                + f"_cal_CH{candidate}_P{profile}.json")
                            path.parent.mkdir(parents=True, exist_ok=True)
                            path.write_text(json.dumps(report, indent=2, ensure_ascii=False),
                                            encoding="utf-8")
                            print("校正預覽／結果：", path.resolve())
                    finally:
                        dut.close()
                    channel = candidate
                    save_ports()
                else:
                    print("無效選項")
            except KeyboardInterrupt:
                print("已中止；已完成的測試點保留於 CSV。CALYS 保留最後輸出值。")
                if calys:
                    calys.close()
                    calys = None
            except Exception as exc:
                print(f"操作失敗：{exc}；CALYS 可能保留最後輸出值。")
                if calys:
                    calys.close()
                    calys = None
    finally:
        if calys:
            calys.close()
        print("結束；LOC 解除遠端控制，輸出值不會自動歸零。")


if __name__ == "__main__":
    main()

