#!/usr/bin/env python3
"""Independent CALYS 1500 / DUT linearity sweep. Run from repository root."""
from __future__ import annotations

import argparse
import csv
import json
import math
import re
import struct
import time
from dataclasses import dataclass, asdict, fields
from datetime import datetime
from decimal import Decimal
from pathlib import Path

from calys1500_protocol import Calys1500, SENSORS, validate
from modbus_rtu_test_menu import ModbusRtuClient

def number(text):
    s = str(text).strip().lower()
    return int(s[:-1], 16) if s.endswith("h") else int(s, 0)

def ask(prompt, default, convert=str):
    raw = input(f"{prompt} [{default}]: ").strip()
    return convert(raw) if raw else default

def decode_pv(registers, kind, order, scale):
    if kind == "int16":
        v = registers[0]
        value = v - 65536 if v & 0x8000 else v
    else:
        if len(registers) != 2:
            raise ValueError("32-bit PV requires two registers")
        words = registers if order == "ABCD" else registers[::-1]
        data = struct.pack(">HH", *words)
        value = struct.unpack(">i" if kind == "int32" else ">f", data)[0]
    value = float(value) * float(scale)
    if not math.isfinite(value):
        raise ValueError("PV is NaN/Infinity")
    return value

@dataclass
class Config:
    dut_port: str = "COM17"
    calys_port: str = "COM20"
    slave: int = 2
    baud: int = 115200
    timeout: float = 3.0
    calys_timeout: float = 10.0
    calys_trace: bool = False
    pv_address: int = 0x1000
    sensor_address: int = 0x1007
    sensor_value: int = 1
    sensor_mode: str = "manual"
    write_sensor: bool = False
    apply_address: int | None = None
    apply_value: int = 0xA5A5
    pv_kind: str = "float32"
    word_order: str = "ABCD"
    pv_scale: float = 1.0
    sensor_code: int = 48
    cjc: str = "DIS"
    fixed_cjc: str = "0"
    start: Decimal = Decimal("-200")
    end: Decimal = Decimal("1300")
    step: Decimal = Decimal("0.1")
    interval: float = 0.5
    samples: int = 2
    settle: float = 1.0
    results: str = "calys_results"
    safe_output: str = "leave"

    @property
    def sensor(self):
        return SENSORS[self.sensor_code]

    def sensor_number(self):
        return self.sensor_code if self.sensor_mode == "linked" else self.sensor_value

    def points(self):
        if self.step <= 0 or self.start > self.end:
            raise ValueError("Invalid sweep range or step")
        validate(self.sensor, self.start)
        validate(self.sensor, self.end)
        count = int((self.end - self.start) // self.step) + 1
        if count > 200000:
            raise ValueError("More than 200000 steps; reduce range or increase step")
        points = [self.start + self.step * i for i in range(count)]
        if points[-1] != self.end:
            points.append(self.end)
        return points

CONFIG_VERSION = 1
DEFAULT_CONFIG_PATH = Path(__file__).with_name("calys1500_linearity_settings.json")
DECIMAL_FIELDS = {"start", "end", "step"}
CONFIG_FIELDS = {f.name for f in fields(Config)}

def config_dict(cfg):
    result = asdict(cfg)
    for key in DECIMAL_FIELDS:
        result[key] = str(result[key])
    return {"version": CONFIG_VERSION, "settings": result}

def validated_config(data):
    """Validate a complete JSON profile before replacing the live configuration."""
    if not isinstance(data, dict) or data.get("version") != CONFIG_VERSION:
        raise ValueError("Unsupported configuration version")
    values = data.get("settings")
    if not isinstance(values, dict) or set(values) != CONFIG_FIELDS:
        raise ValueError("Configuration must contain exactly the supported settings")
    for key in DECIMAL_FIELDS:
        if not isinstance(values[key], str):
            raise ValueError(f"{key} must be a decimal string")
        values[key] = Decimal(values[key])
    integer_fields = {"slave", "baud", "pv_address", "sensor_address", "sensor_value",
                      "apply_value", "sensor_code", "samples"}
    float_fields = {"timeout", "calys_timeout", "pv_scale", "interval", "settle"}
    string_fields = {"dut_port", "calys_port", "sensor_mode", "pv_kind", "word_order",
                     "cjc", "fixed_cjc", "results", "safe_output"}
    for key in integer_fields:
        if type(values[key]) is not int:
            raise ValueError(f"{key} must be an integer")
    for key in float_fields:
        if type(values[key]) not in (float, int):
            raise ValueError(f"{key} must be numeric")
    for key in string_fields:
        if not isinstance(values[key], str):
            raise ValueError(f"{key} must be a string")
    if type(values["calys_trace"]) is not bool:
        raise ValueError("calys_trace must be a boolean")
    if type(values["write_sensor"]) is not bool:
        raise ValueError("write_sensor must be a boolean")
    if values["apply_address"] is not None and type(values["apply_address"]) is not int:
        raise ValueError("apply_address must be an integer or null")
    cfg = Config(**values)
    if not cfg.calys_port.strip() or not cfg.dut_port.strip():
        raise ValueError("COM port names must not be empty")
    if cfg.calys_port.casefold() == cfg.dut_port.casefold():
        raise ValueError("CALYS and DUT ports must be different")
    if not 1 <= cfg.slave <= 247 or cfg.baud <= 0:
        raise ValueError("Invalid slave or baud")
    if not math.isfinite(cfg.calys_timeout) or cfg.calys_timeout <= 0:
        raise ValueError("Invalid CALYS timeout")
    if not math.isfinite(cfg.timeout) or cfg.timeout <= 0:
        raise ValueError("Invalid timeout")
    if cfg.pv_kind not in ("int16", "int32", "float32"):
        raise ValueError("Invalid PV type")
    if cfg.word_order not in ("ABCD", "CDAB") or cfg.sensor_mode not in ("manual", "linked"):
        raise ValueError("Invalid word order or sensor mode")
    if cfg.cjc not in ("DIS", "INT", "FIX") or cfg.safe_output not in ("leave", "zero"):
        raise ValueError("Invalid CJC or safe output mode")
    if not Decimal(cfg.fixed_cjc).is_finite():
        raise ValueError("Invalid fixed CJC")
    if cfg.sensor_code not in SENSORS or SENSORS[cfg.sensor_code].token is None:
        raise ValueError("Unknown/unverified CALYS sensor code")
    for key in ("pv_address", "sensor_address", "sensor_value", "apply_value"):
        if not 0 <= getattr(cfg, key) <= 65535:
            raise ValueError(f"Invalid {key}")
    if cfg.apply_address is not None and not 0 <= cfg.apply_address <= 65535:
        raise ValueError("Invalid apply_address")
    if cfg.pv_address + (cfg.pv_kind != "int16") > 65535:
        raise ValueError("PV registers exceed address space")
    if not math.isfinite(cfg.pv_scale) or cfg.pv_scale == 0:
        raise ValueError("Invalid PV scale")
    if not math.isfinite(cfg.interval) or cfg.interval <= 0 or cfg.samples < 1:
        raise ValueError("Invalid sampling settings")
    if not math.isfinite(cfg.settle) or cfg.settle < 0:
        raise ValueError("Invalid settle time")
    if not cfg.results.strip():
        raise ValueError("Results path is empty")
    cfg.points()  # Also validates finite range, positive step, sensor limits, point count.
    return cfg

def profile_action(cfg, action):
    if action == "0.4":
        print(json.dumps(config_dict(cfg), ensure_ascii=False, indent=2))
    elif action == "0.2":
        if input("恢復預設值？輸入 RESET 確認: ").strip() == "RESET":
            cfg = Config()
            print("已恢復程式內預設值；未連線或寫入任何硬體")
    elif action == "0.1":
        path = Path(input(f"JSON 路徑 [{DEFAULT_CONFIG_PATH}]: ").strip() or DEFAULT_CONFIG_PATH)
        try:
            candidate = validated_config(json.loads(path.read_text(encoding="utf-8")))
            cfg = candidate
            print(f"已載入並驗證: {path}；未連線或寫入任何硬體")
        except (OSError, ValueError, TypeError, ArithmeticError) as exc:
            print(f"載入失敗，原設定保持不變: {exc}")
    elif action == "0.3":
        path = Path(input(f"儲存 JSON 路徑 [{DEFAULT_CONFIG_PATH}]: ").strip() or DEFAULT_CONFIG_PATH)
        try:
            validated_config(config_dict(cfg))
            if path.exists() and input("檔案已存在，輸入 OVERWRITE 覆寫: ").strip() != "OVERWRITE":
                print("取消儲存")
                return cfg
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(json.dumps(config_dict(cfg), ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
            print(f"已儲存: {path}")
        except (OSError, ValueError, TypeError, ArithmeticError) as exc:
            print(f"儲存失敗: {exc}")
    return cfg

def profile_menu(cfg):
    while True:
        print(f"""
[0] 測試設定檔管理
  [0.1] 載入設定檔 (JSON)
  [0.2] 恢復系統預設值
  [0.3] 儲存目前設定 (JSON)
  [0.4] 顯示目前設定
  [q] 返回主選單
預設檔案: {DEFAULT_CONFIG_PATH}""")
        action = input("Select: ").strip().lower()
        if action == "q":
            return cfg
        if action in ("0.1", "0.2", "0.3", "0.4"):
            cfg = profile_action(cfg, action)
        else:
            print("無效選項")


def calys_connect(cfg):
    return Calys1500(cfg.calys_port, timeout=cfg.calys_timeout, trace=cfg.calys_trace)

def dut_connect(cfg):
    if cfg.dut_port.casefold() == cfg.calys_port.casefold():
        raise ValueError("CALYS and DUT cannot share the same COM port")
    return ModbusRtuClient(cfg.dut_port, cfg.slave, cfg.baud, cfg.timeout, show_frames=False)

def show_calys(cfg):
    device = calys_connect(cfg)
    try:
        print("CALYS ID:", device.identity)
        return device.read_status(cfg.sensor)
    finally:
        device.close()

def choose_sensor(cfg):
    """Select and immediately configure CALYS; commit selection only after readback."""
    device = calys_connect(cfg)
    try:
        # Read the current function without requiring it to match the future selection.
        print("CALYS current status:", device.show_status())
        print("1. TC  2. RTD")
        kind = {"1": "TC", "2": "RTD"}.get(input("Class: ").strip())
        if not kind:
            return
        choices = [sensor for sensor in SENSORS.values()
                   if sensor.kind == kind and sensor.token is not None]
        for sensor in choices:
            print(f"{sensor.code:3d} {sensor.label:10} "
                  f"{sensor.low}..{sensor.high} {sensor.unit}")
        default_code = cfg.sensor_code if cfg.sensor.kind == kind else choices[0].code
        code = ask("CALYS sensor code", default_code, number)
        if code not in [sensor.code for sensor in choices]:
            raise ValueError("Invalid/unverified CALYS sensor")
        selected = SENSORS[code]
        print(f"Configuring CALYS: {selected.kind}-{selected.name} ({selected.token})")
        device.configure(selected, cjc=cfg.cjc, fixed_cjc=cfg.fixed_cjc)
        state = device.verify_configuration(selected, {"cjc": cfg.cjc})
        cfg.sensor_code = code
        cfg.start, cfg.end = Decimal(selected.low), Decimal(selected.high)
        print("CALYS configuration verified:", state)
        print(f"Sensor and range updated: {selected.name} {cfg.start}..{cfg.end}")
    finally:
        device.close()

def configure_calys(cfg):
    try:
        print("Before:", show_calys(cfg))
    except Exception as exc:
        print("Readback unavailable; cannot verify instrument:", exc)
    choice = input("CJC DIS / INT / FIX [DIS]: ").strip().upper() or "DIS"
    if choice not in ("DIS", "INT", "FIX"):
        raise ValueError("Invalid CJC")
    if choice == "FIX":
        cfg.fixed_cjc = str(ask("Fixed CJC degC", cfg.fixed_cjc, Decimal))
    cfg.cjc = choice
    device = calys_connect(cfg)
    try:
        device.configure(cfg.sensor, cjc=cfg.cjc, fixed_cjc=cfg.fixed_cjc)
        print("Verified:", device.verify_configuration(cfg.sensor, {"cjc": cfg.cjc}))
    finally:
        device.close()

def calys_output_test(cfg):
    """Set a manual CALYS output with SCPI verification and no DUT changes."""
    device = calys_connect(cfg)
    try:
        print("CALYS:", device.identity)
        print("Current CALYS function:", device.show_status())
        value = Decimal(str(ask(f"Output {cfg.sensor.unit}", cfg.start, Decimal)))
        validate(cfg.sensor, value)
        print(f"即將設定 CALYS 輸出：{value} {cfg.sensor.unit}")
        print("[1] 確認輸出")
        print("[0] 取消")
        if input("Select [0]: ").strip() != "1":
            print("Output cancelled")
            return
        device.configure(cfg.sensor, cjc=cfg.cjc, fixed_cjc=cfg.fixed_cjc)
        device.verify_configuration(cfg.sensor, {"cjc": cfg.cjc})
        device.output(cfg.sensor, value)
        print("Output command status:", device.verify_output(cfg.sensor, value))
        print("注意：僅確認 SCPI ERR? 無錯誤，非實際輸出量測或設定值回讀")
        print("CALYS may retain this output after LOC")
    finally:
        device.close()

def calys_communication_test(cfg):
    """Identity, function, and error readback without changing the output."""
    device = calys_connect(cfg)
    try:
        print("CALYS communication OK:", device.identity)
        print("Instrument state:", device.show_status())
        device.check_error("自動通訊測試")
        print("CALYS ERR? = 0")
    finally:
        device.close()


def pv_test(cfg):
    dut = dut_connect(cfg)
    try:
        qty = 1 if cfg.pv_kind == "int16" else 2
        registers = dut.read_holding_registers(cfg.pv_address, qty)
        print("Registers:", [f"{x:04X}h" for x in registers],
              "PV:", decode_pv(registers, cfg.pv_kind, cfg.word_order, cfg.pv_scale))
    finally:
        dut.close()

def configure_dut(cfg, x):
    """Handle one DUT menu action directly from the main menu."""
    if x == "1.1.1":
        cfg.pv_address = ask("PV address", f"0x{cfg.pv_address:04X}", number)
    elif x == "1.1.2":
        cfg.sensor_address = ask("Sensor address", f"0x{cfg.sensor_address:04X}", number)
    elif x in ("1.1.3", "1.1.4"):
        # Both choices write immediately. WITH APPLY preserves the CJC register
        # between Sensor Type and Apply (same layout as the ten-point tool).
        choice = input("1=CALYS linked / 2=manual [current]: ").strip()
        if choice not in ("", "1", "2"):
            raise ValueError("Select 1 or 2")
        mode = ("linked" if choice == "1" else "manual" if choice == "2" else cfg.sensor_mode)
        value = (cfg.sensor_code if mode == "linked" else
                 ask("DUT Sensor code", cfg.sensor_value, number))
        if not 0 <= value <= 65535:
            raise ValueError("Sensor Type must be 0..65535")
        if not 0 <= cfg.sensor_address <= 65533 and x == "1.1.4":
            raise ValueError("WITH APPLY requires Sensor, CJC, Apply consecutive addresses")
        if not 0 <= cfg.sensor_address <= 65535:
            raise ValueError("Invalid Sensor address")
        apply_value = cfg.apply_value
        if x == "1.1.4":
            apply_value = ask("Apply value", f"0x{cfg.apply_value:04X}", number)
            if not 0 <= apply_value <= 65535:
                raise ValueError("Apply value must be 0..65535")
            print(f"DUT FC10: {cfg.sensor_address:04X}h Sensor={value:04X}h, "
                  f"{cfg.sensor_address+1:04X}h CJC=保留原值, "
                  f"{cfg.sensor_address+2:04X}h Apply={apply_value:04X}h")
            if input("[1] 確認寫入 / [0] 取消: ").strip() != "1":
                print("DUT write cancelled")
                return
        else:
            print(f"DUT FC06: {cfg.sensor_address:04X}h Sensor={value:04X}h")
        dut = dut_connect(cfg)
        try:
            if x == "1.1.4":
                cjc = dut.read_holding_registers(cfg.sensor_address + 1, 1)[0]
                if not 0 <= cjc <= 3:
                    raise ValueError(f"Unexpected DUT CJC source: {cjc}; write cancelled")
                dut.write_multiple_registers(cfg.sensor_address, [value, cjc, apply_value])
            else:
                dut.write_single_register(cfg.sensor_address, value)
            actual = dut.read_holding_registers(cfg.sensor_address, 1)[0]
            if actual != value:
                raise ValueError(f"DUT Sensor readback mismatch: {actual:04X}h != {value:04X}h")
            cfg.sensor_mode = mode
            if mode == "manual":
                cfg.sensor_value = value
            if x == "1.1.4":
                cfg.apply_value = apply_value
            print(f"DUT Sensor Type written and read back: {actual:04X}h")
        finally:
            dut.close()
    elif x == "1.1.5":
        kind = input("int16 / int32 / float32: ").strip().lower()
        if kind not in ("int16", "int32", "float32"):
            raise ValueError("Invalid PV type")
        cfg.pv_kind = kind
    elif x == "1.1.6":
        order = input("ABCD / CDAB: ").strip().upper()
        if order not in ("ABCD", "CDAB"):
            raise ValueError("Invalid word order")
        cfg.word_order = order
    elif x == "1.1.7":
        scale = ask("PV scale", cfg.pv_scale, float)
        if not math.isfinite(scale) or scale == 0:
            raise ValueError("PV scale must be finite and nonzero")
        cfg.pv_scale = scale
    elif x == "1.1.8":
        pv_test(cfg)
    elif x == "1.1.9":
        cfg.dut_port = ask("DUT port", cfg.dut_port)
        cfg.slave = ask("DUT slave", cfg.slave, int)
        if not 1 <= cfg.slave <= 247:
            raise ValueError("DUT slave must be 1..247")
    else:
        raise ValueError("Unknown DUT menu selection")

def configure_test(cfg, selection):
    """Change one sampling setting directly from the main menu."""
    if selection == "1.3.1":
        value = ask("Step degC", cfg.step, Decimal)
        if not value.is_finite() or value <= 0:
            raise ValueError("Step must be positive and finite")
        cfg.step = value
    elif selection == "1.3.2":
        value = ask("Sample interval seconds", cfg.interval, float)
        if not math.isfinite(value) or value <= 0:
            raise ValueError("Sample interval must be positive and finite")
        cfg.interval = value
    elif selection == "1.3.3":
        value = ask("Samples per step", cfg.samples, int)
        if value < 1:
            raise ValueError("Samples per step must be >= 1")
        cfg.samples = value
    elif selection == "1.3.4":
        value = ask("Settle seconds", cfg.settle, float)
        if not math.isfinite(value) or value < 0:
            raise ValueError("Settle time must be nonnegative and finite")
        cfg.settle = value
    else:
        raise ValueError("Unknown test setting")


def preflight(cfg):
    points = cfg.points()
    if cfg.interval <= 0 or cfg.samples < 1 or cfg.settle < 0:
        raise ValueError("Invalid sampling settings")
    if not 0 <= cfg.pv_address <= 65535 or not 0 <= cfg.sensor_address <= 65535:
        raise ValueError("Invalid register address")
    if cfg.pv_address + (cfg.pv_kind != "int16") > 65535:
        raise ValueError("PV registers exceed Modbus address space")
    if not 0 <= cfg.sensor_number() <= 65535:
        raise ValueError("Invalid DUT sensor value")
    if not math.isfinite(cfg.pv_scale) or cfg.pv_scale == 0:
        raise ValueError("Invalid PV scale")
    device = calys_connect(cfg)
    dut = None
    try:
        state = device.read_status(cfg.sensor)
        print("CALYS readback:", state)
        actual_cjc = {"DISABLED": "DIS", "INTERNAL": "INT", "FIXED": "FIX",
                      "OFF": "DIS", "ON": "INT"}.get(state.get("cjc"), state.get("cjc"))
        if cfg.sensor.kind == "TC" and actual_cjc != cfg.cjc:
            raise ValueError("CALYS CJC mismatch: configure 1.2.2 first")
        if state.get("sensor_type") != cfg.sensor.token.upper():
            raise ValueError("CALYS sensor mismatch: configure 1.2.1 first")
        dut = dut_connect(cfg)
        qty = 1 if cfg.pv_kind == "int16" else 2
        registers = dut.read_holding_registers(cfg.pv_address, qty)
        print("DUT PV:", decode_pv(registers, cfg.pv_kind, cfg.word_order, cfg.pv_scale))
        # [1.4] is read-only: do not change Sensor Type or Apply here.
        current_sensor = dut.read_holding_registers(cfg.sensor_address, 1)[0]
        if current_sensor != cfg.sensor_number():
            raise ValueError(f"DUT Sensor Type mismatch: {current_sensor:04X}h "
                             f"!= expected {cfg.sensor_number():04X}h; configure [1.1.3]/[1.1.4]")
        print(f"DUT Sensor Type verified: {current_sensor:04X}h")
        estimate = len(points) * (cfg.settle + cfg.samples * cfg.interval)
        print(f"Points={len(points)}, samples={len(points)*cfg.samples}, minimum estimate={estimate:.1f}s")
        print("CALYS setpoint is not independently measured actual output.")
        if input("Start 60-point pretest (START): ").strip() != "START":
            raise ValueError("Test cancelled")
        return device, dut, points, estimate
    except BaseException:
        if dut: dut.close()
        device.close()
        raise

FIELDS = ["timestamp","step_index","sample_index","calys_setpoint","calys_actual",
          "dut_pv","delta","sensor_type","dut_sensor_type","read_status","error"]

def result_path(cfg, phase="full"):
    folder = Path(cfg.results)
    folder.mkdir(parents=True, exist_ok=True)
    stamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    label = re.sub(r"[^A-Za-z0-9_-]", "_", cfg.sensor.name)
    for index in range(1,10000):
        path = folder / f"{stamp}_{index:03d}_{cfg.sensor.kind}-{label}_{phase}.csv"
        if not path.exists(): return path
    raise RuntimeError("No free filename sequence")

def summarize(path, rows):
    groups = {}
    for row in rows:
        if row["read_status"] == "OK":
            groups.setdefault(row["step_index"], []).append(row)
    summary = path.with_name(path.stem + "_summary.csv")
    with summary.open("w", newline="", encoding="utf-8-sig") as f:
        w = csv.writer(f)
        w.writerow(["step_index","calys_setpoint","valid_samples","mean_dut_pv",
                    "mean_delta","max_positive_delta","max_negative_delta","max_absolute_delta"])
        for step, data in sorted(groups.items()):
            ds = [float(r["delta"]) for r in data]
            w.writerow([step,data[0]["calys_setpoint"],len(ds),
                        sum(float(r["dut_pv"]) for r in data)/len(data),
                        sum(ds)/len(ds),max([x for x in ds if x>0],default=""),
                        min([x for x in ds if x<0],default=""),max(map(abs,ds))])
    print("Summary:", summary)


def run_pretest(cfg, device, dut):
    """60 evenly spaced inclusive setpoints; 60 seconds is a target, not a hard deadline."""
    points = [cfg.start + (cfg.end - cfg.start) * Decimal(i) / Decimal(59)
              for i in range(60)]
    path = result_path(cfg, "pretest")
    rows = []
    started = time.monotonic()
    print(f"Pretest: 60 points, target 60 seconds; output/serial latency may extend it. CSV: {path}")
    with path.open("w", newline="", encoding="utf-8-sig") as stream:
        writer = csv.DictWriter(stream, fieldnames=FIELDS)
        writer.writeheader()
        stream.flush()
        try:
            for index, point in enumerate(points, 1):
                row = dict(timestamp=datetime.now().astimezone().isoformat(),
                           step_index=index, sample_index=1, calys_setpoint=str(point),
                           calys_actual="N/A", dut_pv="", delta="",
                           sensor_type=f"{cfg.sensor.kind}-{cfg.sensor.name}",
                           dut_sensor_type=f"0x{cfg.sensor_number():04X}",
                           read_status="OK", error="")
                try:
                    device.output(cfg.sensor, point)
                    device.verify_output(cfg.sensor, point)
                    # Do not skip steps if the instrument is slower than the 1-second target.
                    time.sleep(max(0.0, started + index - time.monotonic()))
                    qty = 1 if cfg.pv_kind == "int16" else 2
                    pv = decode_pv(dut.read_holding_registers(cfg.pv_address, qty),
                                   cfg.pv_kind, cfg.word_order, cfg.pv_scale)
                    row["dut_pv"] = pv
                    row["delta"] = float(Decimal(str(pv)) - point)
                except Exception as exc:
                    row["read_status"] = "ERROR"
                    row["error"] = str(exc)
                finally:
                    rows.append(row)
                    writer.writerow(row)
                    stream.flush()
                elapsed = time.monotonic() - started
                print(f"[PRE {index}/60] set={point} PV={row['dut_pv']} "
                      f"status={row['read_status']} elapsed={elapsed:.1f}s")
                if row["read_status"] != "OK":
                    print("Pretest stopped on error; full sweep is blocked.")
                    break
        finally:
            summarize(path, rows)
    errors = sum(r["read_status"] != "OK" for r in rows)
    valid = len(rows) == 60 and errors == 0
    print(f"Pretest complete: {len(rows)}/60, errors={errors}, "
          f"duration={time.monotonic()-started:.1f}s")
    print("Pretest CSV:", path.resolve())
    if valid:
        print("All 60 Modbus reads succeeded; this does not certify measurement accuracy.")
    return valid

def sweep(cfg):
    device,dut,points,estimate = preflight(cfg)
    rows = []
    path = None
    try:
        if not run_pretest(cfg, device, dut):
            print("Full sweep cancelled: pretest did not pass.")
            return
        print("Review pretest CSV and per-step summary before continuing.")
        if input("Confirm data and start full sweep (START): ").strip() != "START":
            print("Full sweep cancelled by operator; pretest data preserved.")
            return
        path = result_path(cfg, "full")
        started = time.monotonic()
        print("Full sweep CSV:", path)

        with path.open("w", newline="", encoding="utf-8-sig") as f:
            writer = csv.DictWriter(f, fieldnames=FIELDS)
            writer.writeheader(); f.flush()
            for step_index, setpoint in enumerate(points,1):
                step_started = time.monotonic()
                device.output(cfg.sensor, setpoint)
                device.verify_output(cfg.sensor, setpoint)
                time.sleep(cfg.settle)
                for sample_index in range(1,cfg.samples+1):
                    target = step_started + cfg.settle + sample_index*cfg.interval
                    time.sleep(max(0,target-time.monotonic()))
                    row = dict(timestamp=datetime.now().astimezone().isoformat(),
                               step_index=step_index,sample_index=sample_index,
                               calys_setpoint=str(setpoint),calys_actual="N/A",
                               dut_pv="",delta="",sensor_type=f"{cfg.sensor.kind}-{cfg.sensor.name}",
                               dut_sensor_type=f"0x{cfg.sensor_number():04X}",read_status="OK",error="")
                    try:
                        qty = 1 if cfg.pv_kind=="int16" else 2
                        regs = dut.read_holding_registers(cfg.pv_address,qty)
                        pv = decode_pv(regs,cfg.pv_kind,cfg.word_order,cfg.pv_scale)
                        row["dut_pv"] = pv
                        row["delta"] = float(Decimal(str(pv))-setpoint)
                    except Exception as exc:
                        row["read_status"]="ERROR"
                        row["error"]=str(exc)
                    rows.append(row); writer.writerow(row); f.flush()
                elapsed = time.monotonic()-started
                mean = elapsed/step_index
                remaining = max(0,(len(points)-step_index)*mean)
                print(f"[{step_index}/{len(points)}] CALYS={setpoint} elapsed={elapsed:.1f}s remaining≈{remaining:.1f}s")
    except KeyboardInterrupt:
        print("Interrupted; completed CSV rows preserved")
    finally:
        if path is not None:
            summarize(path,rows)
        print("CALYS retains last setpoint unless safe output explicitly selected.")
        if cfg.safe_output=="zero":
            try:
                device.output(cfg.sensor,0)
                print("CALYS set to 0")
            except Exception as exc:
                print("Cannot set safe output:",exc)
        dut.close();device.close()

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--calys-port",default="COM20")
    parser.add_argument("--dut-port",default="COM17")
    parser.add_argument("--results",default="calys_results")
    parser.add_argument("--calys-timeout",type=float,default=10.0,
                        help="CALYS SCPI query timeout in seconds (default: 10)")
    parser.add_argument("--calys-trace",action="store_true",help="Show CALYS TX/RX")
    args=parser.parse_args()
    if not math.isfinite(args.calys_timeout) or args.calys_timeout <= 0:
        parser.error("--calys-timeout must be finite and positive")
    cfg=Config(calys_port=args.calys_port,dut_port=args.dut_port,results=args.results,
               calys_timeout=args.calys_timeout,calys_trace=args.calys_trace)
    while True:
        print(f"""
========== CALYS 1500 / DUT 線性度測試 ==========
[0] 測試設定檔管理
  [0.1] 載入設定檔 (JSON)
  [0.2] 恢復系統預設值
  [0.3] 儲存目前設定 (JSON)
  [0.4] 顯示目前設定
[1] 線性度測試
  [1.1] DUT設定
    [1.1.1] 設定 DUT PV 通道 Address [{cfg.pv_address:04X}h]
    [1.1.2] 設定 DUT Sensor Address [{cfg.sensor_address:04X}h]
    [1.1.3] 設定 DUT Sensor type [0x{cfg.sensor_number():04X}]
    [1.1.4] 設定 DUT Sensor type WITH APPLY [0x{cfg.sensor_number():04X}]
    [1.1.5] PV type [{cfg.pv_kind}]
    [1.1.6] Word order [{cfg.word_order}]
    [1.1.7] PV scale [{cfg.pv_scale}]
    [1.1.8] 讀 PV 測試
    [1.1.9] DUT port/slave [{cfg.dut_port}/{cfg.slave}]
  [1.2] CALYS 1500 設定
    [1.2.1] 設定 CALYS SENSOR TYPE [{cfg.sensor.kind}-{cfg.sensor.name}]
    [1.2.2] 設定 CALYS CJC [{cfg.cjc}]
    [1.2.3] 起點／終點 [{cfg.start}..{cfg.end}]
    [1.2.4] 設定輸出值
    [1.2.5] 自動通訊測試
  [1.3] 測試設定
    [1.3.1] 溫度步階 [{cfg.step}]
    [1.3.2] PV 取樣間隔 [{cfg.interval}s]
    [1.3.3] 每階取樣次數 [{cfg.samples}]
    [1.3.4] Settle before sampling [{cfg.settle}s]
  [1.4] 開始測試
[q] 結束
=============================================""")
        x=input("Select: ").strip().lower()
        try:
            if x=="q":break
            elif x in ("0","0.1","0.2","0.3","0.4"):
                if x == "0":
                    cfg = profile_menu(cfg)
                else:
                    # Direct choices also work without entering a nested menu.
                    cfg = profile_action(cfg, x)
            elif x in ("1.1.1","1.1.2","1.1.3","1.1.4","1.1.5","1.1.6","1.1.7","1.1.8","1.1.9"):
                configure_dut(cfg, x)
            elif x=="1.2.1":choose_sensor(cfg)
            elif x=="1.2.2":configure_calys(cfg)
            elif x=="1.2.4":calys_output_test(cfg)
            elif x=="1.2.5":calys_communication_test(cfg)
            elif x=="1.2.3":
                cfg.start=ask("Start degC",cfg.start,Decimal)
                cfg.end=ask("End degC",cfg.end,Decimal)
                cfg.points()
            elif x in ("1.3.1","1.3.2","1.3.3","1.3.4"):
                configure_test(cfg, x)
            elif x=="1.4":sweep(cfg)
            elif x in ("1","1.2"):continue
            else:print("Unknown selection")
        except KeyboardInterrupt:
            print("Cancelled")
        except Exception as exc:
            print("ERROR:",exc)

if __name__=="__main__":
    main()
