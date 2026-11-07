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
from dataclasses import dataclass
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
    pv_address: int = 0x1000
    sensor_address: int = 0x1004
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

def calys_connect(cfg):
    return Calys1500(cfg.calys_port, timeout=cfg.timeout)

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
    try:
        print("CALYS readback:", show_calys(cfg))
    except Exception as exc:
        print("Readback unavailable:", exc)
    print("1. TC  2. RTD")
    kind = {"1": "TC", "2": "RTD"}.get(input("Class: ").strip())
    if not kind:
        return
    choices = [s for s in SENSORS.values() if s.kind == kind and s.token is not None]
    for sensor in choices:
        print(f"{sensor.code:3d} {sensor.label:10} {sensor.low}..{sensor.high} {sensor.unit}")
    code = ask("CALYS sensor code", cfg.sensor_code, number)
    if code not in [s.code for s in choices]:
        raise ValueError("Invalid/unverified CALYS sensor")
    cfg.sensor_code = code
    cfg.start, cfg.end = Decimal(cfg.sensor.low), Decimal(cfg.sensor.high)
    print("Sensor selected; range reset to sensor limits")

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
    elif x == "1.1.3":
        choice = input("1=CALYS linked / 2=manual [current]: ").strip()
        if choice not in ("", "1", "2"):
            raise ValueError("Select 1 or 2")
        if choice:
            cfg.sensor_mode = "linked" if choice == "1" else "manual"
        if cfg.sensor_mode == "manual":
            cfg.sensor_value = ask("DUT Sensor code", cfg.sensor_value, number)
    elif x == "1.1.4":
        pv_test(cfg)
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
        cfg.write_sensor = input("Enable sensor write? YES/no: ").strip() == "YES"
        if cfg.write_sensor:
            a = input("Apply register address (blank = none): ").strip()
            cfg.apply_address = number(a) if a else None
            if a:
                cfg.apply_value = ask("Apply value", "0xA5A5", number)
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
        if cfg.sensor.kind == "TC" and state.get("cjc") != cfg.cjc:
            raise ValueError("CALYS CJC mismatch: configure 1.2.2 first")
        if state.get("sensor_type") != cfg.sensor.token.upper():
            raise ValueError("CALYS sensor mismatch: configure 1.2.1 first")
        dut = dut_connect(cfg)
        qty = 1 if cfg.pv_kind == "int16" else 2
        registers = dut.read_holding_registers(cfg.pv_address, qty)
        print("DUT PV:", decode_pv(registers, cfg.pv_kind, cfg.word_order, cfg.pv_scale))
        if cfg.write_sensor:
            print(f"DUT sensor write: {cfg.sensor_address:04X}h = {cfg.sensor_number():04X}h")
            if input("Confirm DUT sensor write (WRITE): ").strip() != "WRITE":
                raise ValueError("DUT write cancelled")
            dut.write_single_register(cfg.sensor_address, cfg.sensor_number())
            if dut.read_holding_registers(cfg.sensor_address, 1)[0] != cfg.sensor_number():
                raise ValueError("DUT sensor readback mismatch")
            if cfg.apply_address is not None:
                if input(f"Apply {cfg.apply_address:04X}h={cfg.apply_value:04X}h (APPLY): ").strip() != "APPLY":
                    raise ValueError("Apply cancelled")
                dut.write_single_register(cfg.apply_address, cfg.apply_value)
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
    args=parser.parse_args()
    cfg=Config(calys_port=args.calys_port,dut_port=args.dut_port,results=args.results)
    while True:
        print(f"""
========== CALYS 1500 / DUT 線性度測試 ==========
[1] 線性度測試
  [1.1] DUT設定
    [1.1.1] 設定 DUT PV 通道 Address [{cfg.pv_address:04X}h]
    [1.1.2] 設定 DUT Sensor Address [{cfg.sensor_address:04X}h]
    [1.1.3] 設定 DUT Sensor type [0x{cfg.sensor_number():04X}]
    [1.1.4] 讀 PV 測試
    [1.1.5] PV type [{cfg.pv_kind}]
    [1.1.6] Word order [{cfg.word_order}]
    [1.1.7] PV scale [{cfg.pv_scale}]
    [1.1.8] Sensor write [{cfg.write_sensor}], Apply [{f'{cfg.apply_address:04X}h' if cfg.apply_address is not None else 'None'}]
    [1.1.9] DUT port/slave [{cfg.dut_port}/{cfg.slave}]
  [1.2] CALYS 1500 設定
    [1.2.1] 設定 CALYS SENSOR TYPE [{cfg.sensor.kind}-{cfg.sensor.name}]
    [1.2.2] 設定 CALYS CJC [{cfg.cjc}]
    [1.2.3] 起點／終點 [{cfg.start}..{cfg.end}]
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
            elif x in ("1.1.1","1.1.2","1.1.3","1.1.4","1.1.5","1.1.6","1.1.7","1.1.8","1.1.9"):
                configure_dut(cfg, x)
            elif x=="1.2.1":choose_sensor(cfg)
            elif x=="1.2.2":configure_calys(cfg)
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
