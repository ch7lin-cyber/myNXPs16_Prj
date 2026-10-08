#!/usr/bin/env python3
"""Interactive CALYS1500 / S16 Modbus RTU loop test. No calibration writes."""
import argparse
import csv
import json
import math
import time
from datetime import datetime, timezone
from pathlib import Path

from calys1500_protocol import Calys1500, SENSORS, numeric, ten_points, validate

# No serial dependency is required to import the catalog or run --list-sensors.
def modbus_helpers():
    from modbus_rtu_test_menu import (ModbusRtuClient, float32_from_registers,
                                      int32_from_registers, uint32_from_registers)
    return ModbusRtuClient, float32_from_registers, int32_from_registers, uint32_from_registers


CALIBRATION_MENU = {
    "2.1.0": "TC／mV Gain=16", "2.1.1": "TC／mV Gain=32",
    "2.1.2": "TC／mV Gain=64", "2.1.3": "TC／mV Gain=128",
    "2.2.0": "0～5V／0～10V 共用 Gain=32",
    "2.3.0": "0～20mA／4～20mA 共用 Gain=64（依現有韌體）",
    "2.4.0": "RTD Gain=1", "2.4.1": "RTD Gain=8",
    "2.4.2": "RTD Gain=16", "2.4.3": "RTD Gain=32",
}
FIELDS = ["utc", "point", "channel", "sensor_code", "sensor", "gain", "setpoint",
          "unit", "pv", "input_error", "raw_code", "microvolts", "successful_samples",
          "calys_idn", "cjc", "fixed_cjc", "current_supply", "dwell_seconds", "status", "error"]


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
    # A single FC10 stages type/reserved/apply, preserving the existing filter.
    dut.write_multiple_registers(base + 7, [sensor.code, 0, 0xA5A5])
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
    with path.open("w", newline="", encoding="utf-8-sig") as stream:
        writer = csv.DictWriter(stream, fieldnames=FIELDS)
        writer.writeheader()
        stream.flush()
        for index, point in enumerate(ten_points(sensor), 1):
            row = dict(utc=datetime.now(timezone.utc).isoformat(), point=index,
                       channel=channel, sensor_code=sensor.code, sensor=sensor.name,
                       gain=sensor.gain, setpoint=str(point), unit=sensor.unit,
                       calys_idn=calys.identity, cjc=settings.get("cjc", "INT"),
                       fixed_cjc=settings.get("fixed_cjc", "0"),
                       current_supply=settings.get("current_supply", "ON"),
                       dwell_seconds=dwell, status="recorded", error="")
            try:
                calys.output(sensor, point)
                time.sleep(dwell)
                before = sample_count(dut, channel)
                deadline = time.monotonic() + fresh_timeout
                while sample_count(dut, channel) == before:
                    if time.monotonic() >= deadline:
                        raise TimeoutError("等待新成功樣本逾時")
                    time.sleep(0.1)
                pv = dut.read_holding_registers(0x1000 + channel * 0x10, 3)
                raw = dut.read_holding_registers(0x484F + channel * 0x40, 6)
                row.update(pv=f32(pv[:2]), input_error=pv[2],
                           raw_code=f"0x{u32(raw[:2]):08X}", microvolts=i32(raw[4:6]),
                           successful_samples=sample_count(dut, channel))
                print(f"[{index}/10] 輸出 {point} {sensor.unit} → PV={row['pv']}, "
                      f"error={row['input_error']}, ADC={row['microvolts']} uV")
            except BaseException as exc:
                row.update(status="interrupted" if isinstance(exc, KeyboardInterrupt) else "error",
                           error=str(exc) or type(exc).__name__)
                writer.writerow(row)
                stream.flush()
                raise
            writer.writerow(row)
            stream.flush()
    return path


def positive(text):
    value = float(text)
    if not math.isfinite(value) or value <= 0:
        raise argparse.ArgumentTypeError("必須是有限的正數")
    return value


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--calys-port")
    parser.add_argument("--dut-port")
    parser.add_argument("--dut-baud", type=int, default=115200)
    parser.add_argument("--slave", type=int, choices=range(1, 248), default=2,
                        metavar="1..247")
    parser.add_argument("--dwell", type=positive, default=2.0)
    parser.add_argument("--fresh-timeout", type=positive, default=10.0)
    parser.add_argument("--calys-timeout", type=positive, default=5.0)
    parser.add_argument("--cjc", choices=["INT", "DIS", "FIX"], default="INT")
    parser.add_argument("--fixed-cjc", default="0")
    parser.add_argument("--current-supply", choices=["ON", "OFF"], default="ON")
    parser.add_argument("--results", type=Path, default=Path("calys_results"))
    parser.add_argument("--config", type=Path, default=Path("calys1500_ports.json"))
    parser.add_argument("--list-sensors", action="store_true")
    args = parser.parse_args(argv)
    numeric(args.fixed_cjc)
    if args.list_sensors:
        list_sensors()
        return
    saved = json.loads(args.config.read_text(encoding="utf-8")) if args.config.exists() else {}
    calys_port = args.calys_port or saved.get("calys_port", "")
    dut_port = args.dut_port or saved.get("dut_port", "")
    selected, channel, calys = None, None, None
    settings = dict(cjc=args.cjc, fixed_cjc=args.fixed_cjc, current_supply=args.current_supply)

    def save_ports():
        args.config.parent.mkdir(parents=True, exist_ok=True)
        args.config.write_text(json.dumps(dict(calys_port=calys_port, dut_port=dut_port),
                                          indent=2), encoding="utf-8")

    def connect():
        nonlocal calys, calys_port
        if calys is None:
            if not calys_port:
                calys_port = input("CALYS 1500 port：").strip()
            calys = Calys1500(calys_port, args.calys_timeout)
            print(calys.identity)
        return calys

    try:
        while True:
            print("\n0 CALYS 1500 相關　1 選通道／Sensor　2 自動校正（預留）　q 結束")
            print("0.1.0 自動通訊測試　0.1.1 改 CALYS port　0.1.2 改 DUT port")
            print("0.2 改 CALYS Sensor　0.3 設輸出值　1.1.0 十點自動測試")
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
                elif action == "0.1.2":
                    dut_port = input("DUT port：").strip()
                    save_ports()
                elif action == "0.2":
                    sensor = select_sensor()
                    connect().configure(sensor, **settings)
                    selected = sensor
                elif action == "0.3":
                    if selected is None:
                        raise ValueError("請先選擇 Sensor")
                    value = validate(selected, input(f"輸出值 ({selected.unit})："))
                    connect().configure(selected, **settings)
                    connect().output(selected, value)
                elif action == "1":
                    candidate = int(input("通道 0／1／2／3："))
                    if candidate not in range(4):
                        raise ValueError("通道必須是 0～3")
                    sensor = select_sensor()
                    channel, selected = candidate, sensor
                elif action == "1.1.0":
                    if selected is None or channel is None:
                        raise ValueError("請先用選單 1 選通道及 Sensor")
                    if not dut_port:
                        dut_port = input("DUT port：").strip()
                    if dut_port.casefold() == calys_port.casefold() and calys_port:
                        raise ValueError("CALYS 與 DUT 必須使用不同 port")
                    device = connect()
                    device.configure(selected, **settings)
                    cls, *_ = modbus_helpers()
                    dut = cls(dut_port, args.slave, args.dut_baud)
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
                    for key, label in CALIBRATION_MENU.items():
                        print(key, label, "（預留，尚未實作）")
                elif action in CALIBRATION_MENU:
                    print(CALIBRATION_MENU[action], "：預留入口，尚未實作；未寫入校正資料。")
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
