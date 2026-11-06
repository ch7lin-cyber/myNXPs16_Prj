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

def configure_dut(cfg):
    while True:
        print(f"""
[1.1] DUT settings
[1.1.1] PV address: {cfg.pv_address:04X}h
[1.1.2] Sensor address: {cfg.sensor_address:04X}h
[1.1.3] Sensor mode/value: {cfg.sensor_mode} / 0x{cfg.sensor_number():04X}
[1.1.4] Read PV test
[1.1.5] PV type: {cfg.pv_kind}
[1.1.6] Word order: {cfg.word_order}
[1.1.7] PV scale: {cfg.pv_scale}
[1.1.8] Sensor write: {cfg.write_sensor}, Apply: {cfg.apply_address}
[1.1.9] DUT port/slave: {cfg.dut_port}/{cfg.slave}
[q] Back""")
        x = input("Select: ").strip().lower()
        if x == "q": return
        if x == "1.1.1": cfg.pv_address = ask("PV address", f"0x{cfg.pv_address:04X}", number)
        elif x == "1.1.2": cfg.sensor_address = ask("Sensor address", f"0x{cfg.sensor_address:04X}", number)
        elif x == "1.1.3":
            cfg.sensor_mode = "linked" if input("1=CALYS linked / 2=manual [2]: ").strip() == "1" else "manual"
            if cfg.sensor_mode == "manual": cfg.sensor_value = ask("DUT Sensor code", cfg.sensor_value, number)
        elif x == "1.1.4": pv_test(cfg)
        elif x == "1.1.5":
            kind = input("int16 / int32 / float32: ").strip().lower()
            if kind not in ("int16", "int32", "float32"): raise ValueError("Invalid PV type")
            cfg.pv_kind = kind
        elif x == "1.1.6":
            order = input("ABCD / CDAB: ").strip().upper()
            if order not in ("ABCD", "CDAB"): raise ValueError("Invalid order")
            cfg.word_order = order
        elif x == "1.1.7": cfg.pv_scale = ask("PV scale", cfg.pv_scale, float)
        elif x == "1.1.8":
            cfg.write_sensor = input("Enable sensor write? YES/no: ").strip() == "YES"
            if cfg.write_sensor:
                a = input("Apply register address (blank = none): ").strip()
                cfg.apply_address = number(a) if a else None
                if a: cfg.apply_value = ask("Apply value", "0xA5A5", number)
        elif x == "1.1.9":
            cfg.dut_port = ask("DUT port", cfg.dut_port)
            cfg.slave = ask("DUT slave", cfg.slave, int)
        else: print("Unknown selection")

def configure_test(cfg):
    while True:
        print(f"""
[1.3] Test settings
[1.3.1] Step: {cfg.step} degC
[1.3.2] PV sample interval: {cfg.interval}s
[1.3.3] Samples per step: {cfg.samples}
[1.3.4] Settle before sampling: {cfg.settle}s
[q] Back""")
        x = input("Select: ").strip().lower()
        if x == "q": return
        if x == "1.3.1": cfg.step = ask("Step degC", cfg.step, Decimal)
        elif x == "1.3.2": cfg.interval = ask("Sample interval seconds", cfg.interval, float)
        elif x == "1.3.3": cfg.samples = ask("Samples per step", cfg.samples, int)
        elif x == "1.3.4": cfg.settle = ask("Settle seconds", cfg.settle, float)

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
        if input("Start sweep (START): ").strip() != "START":
            raise ValueError("Test cancelled")
        return device, dut, points, estimate
    except BaseException:
        if dut: dut.close()
        device.close()
        raise

FIELDS = ["timestamp","step_index","sample_index","calys_setpoint","calys_actual",
          "dut_pv","delta","sensor_type","dut_sensor_type","read_status","error"]

def result_path(cfg):
    folder = Path(cfg.results)
    folder.mkdir(parents=True, exist_ok=True)
    stamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    label = re.sub(r"[^A-Za-z0-9_-]", "_", cfg.sensor.name)
    for index in range(1,10000):
        path = folder / f"{stamp}_{index:03d}_{cfg.sensor.kind}-{label}.csv"
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

def sweep(cfg):
    device,dut,points,estimate = preflight(cfg)
    path = result_path(cfg)
    rows = []
    started = time.monotonic()
    previous_elapsed = None
    print("CSV:", path)
    try:
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
        print("Interrupted; partial CSV preserved")
    finally:
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
  [1.2] CALYS 1500 設定
    [1.2.1] 設定 CALYS SENSOR TYPE [{cfg.sensor.kind}-{cfg.sensor.name}]
    [1.2.2] 設定 CALYS CJC [{cfg.cjc}]
    [1.2.3] 起點／終點 [{cfg.start}..{cfg.end}]
  [1.3] 測試設定
    [1.3.1] 溫度步階 [{cfg.step}]
    [1.3.2] PV 取樣間隔 [{cfg.interval}s]
    [1.3.3] 每階取樣次數 [{cfg.samples}]
  [1.4] 開始測試
[q] 結束
=============================================""")
        x=input("Select: ").strip().lower()
        try:
            if x=="q":break
            elif x=="1.1":configure_dut(cfg)
            elif x in ("1.1.1","1.1.2","1.1.3","1.1.4","1.1.5","1.1.6","1.1.7","1.1.8","1.1.9"):
                print("Open [1.1] for DUT settings and PV format options")
                configure_dut(cfg)
            elif x=="1.2.1":choose_sensor(cfg)
            elif x=="1.2.2":configure_calys(cfg)
            elif x=="1.2.3":
                cfg.start=ask("Start degC",cfg.start,Decimal)
                cfg.end=ask("End degC",cfg.end,Decimal)
                cfg.points()
            elif x=="1.3":configure_test(cfg)
            elif x in ("1.3.1","1.3.2","1.3.3"):configure_test(cfg)
            elif x=="1.4":sweep(cfg)
            elif x in ("1","1.2"):continue
            else:print("Unknown selection")
        except KeyboardInterrupt:
            print("Cancelled")
        except Exception as exc:
            print("ERROR:",exc)

if __name__=="__main__":
    main()
