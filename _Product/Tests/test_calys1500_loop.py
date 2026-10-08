"""Run: python -m unittest discover -s _Product/Tests -p test_calys1500_loop.py"""
import csv
import struct
import sys
import tempfile
import unittest
from decimal import Decimal
from pathlib import Path
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "Tools"))
import calys1500_protocol as protocol
import calys1500_loop_test as loop


def helpers():
    def u32(r):
        return (r[0] << 16) | r[1]
    def i32(r):
        value = u32(r)
        return value - (1 << 32) if value & (1 << 31) else value
    def f32(r):
        return struct.unpack(">f", struct.pack(">HH", *r))[0]
    return None, f32, i32, u32


class Transport:
    def __init__(self, replies):
        self.replies = iter(replies)
        self.writes = []
        self.closed = False
    def write(self, data):
        self.writes.append(data)
    def flush(self):
        pass
    def readline(self):
        return next(self.replies)
    def reset_input_buffer(self):
        pass
    def close(self):
        self.closed = True


class CalysFake:
    identity = "AOIP SAS,CALYS1500,test,A11"
    def __init__(self, interrupt=None):
        self.outputs = []
        self.interrupt = interrupt
    def output(self, sensor, value):
        if len(self.outputs) == self.interrupt:
            raise KeyboardInterrupt()
        self.outputs.append(value)


class DutFake:
    def __init__(self):
        self.count = 0
        self.writes = []
    def write_multiple_registers(self, address, values):
        self.writes.append((address, values))
    def read_holding_registers(self, address, quantity):
        if address == 0x1017:
            return [48]
        if address == 0x4871:
            self.count += 1
            return [0, self.count]
        if address == 0x1010:
            return [0x3F80, 0, 0]  # 1.0 PV, no input error
        if address == 0x488F:
            return [0x80, 1, 0, 255, 0xFFFF, 0xFFFF]  # -1 uV
        raise AssertionError((hex(address), quantity))


class Tests(unittest.TestCase):
    def test_points(self):
        for sensor in protocol.SENSORS.values():
            if sensor.token:
                points = protocol.ten_points(sensor)
                self.assertEqual(len(points), 10)
                self.assertEqual(points[0], sensor.low)
                self.assertEqual(points[-1], sensor.high)
                self.assertTrue(all(a < b for a, b in zip(points, points[1:])))
        self.assertEqual(protocol.ten_points(protocol.SENSORS[48])[1], Decimal("-33.333"))

    def test_units_and_validation(self):
        self.assertEqual(protocol.value_command(protocol.SENSORS[122], "12.5"), "SOUR:VOLT 0.0125")
        self.assertEqual(protocol.value_command(protocol.SENSORS[124], "4.25"), "SOUR:CURR 0.00425")
        self.assertEqual(protocol.value_command(protocol.SENSORS[48], "-12.5"), "SOUR:TC -12.5")
        for value in ("nan", "inf", "-201", "1301"):
            with self.assertRaises(ValueError):
                protocol.value_command(protocol.SENSORS[48], value)
        for code in (23, 62, 102, 115):
            with self.assertRaises(ValueError):
                protocol.setup_commands(protocol.SENSORS[code])
        commands = protocol.setup_commands(protocol.SENSORS[48], cjc="FIX", fixed_cjc="23.4")
        self.assertIn("SOUR:TC:RJUN 23.4", commands)
        self.assertEqual(commands[-1], "SOUR:FUNC TC")

    def test_protocol_and_errors(self):
        transport = Transport([b'AOIP SAS,CALYS1500,SN,A11\r\n', b'0,"No error"\r\n',
                               b'5,"range error"\r\n'])
        calys = protocol.Calys1500("fake", transport=transport)
        with self.assertRaises(protocol.CalysError):
            calys.output(protocol.SENSORS[120], "1")
        calys.close()
        self.assertEqual(transport.writes, [b'REM\n', b'*CLS\n', b'*IDN?\n',
                                          b'ERR?\n', b'SOUR:VOLT 1\n', b'ERR?\n', b'LOC\n'])
        self.assertTrue(transport.closed)
        transport = Transport([b'OTHER DEVICE\r\n'])
        with self.assertRaises(protocol.CalysError):
            protocol.Calys1500("fake", transport=transport)
        self.assertTrue(transport.closed)

    @patch.object(loop, "modbus_helpers", helpers)
    @patch.object(loop.time, "sleep", lambda seconds: None)
    def test_loop_and_partial_csv(self):
        dut, calys = DutFake(), CalysFake()
        loop.configure_dut(dut, 1, protocol.SENSORS[48], 0)
        self.assertEqual(dut.writes, [(0x1017, [48, 0, 0xA5A5])])
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "results.csv"
            loop.run_sweep(calys, dut, 1, protocol.SENSORS[48], path, dwell=0)
            with path.open(encoding="utf-8-sig") as stream:
                rows = list(csv.DictReader(stream))
            self.assertEqual(len(rows), 10)
            self.assertEqual(rows[-1]["setpoint"], "1300.000")
            self.assertEqual(rows[0]["pv"], "1.0")
            self.assertEqual(rows[0]["microvolts"], "-1")
            self.assertEqual(rows[0]["status"], "recorded")  # no pass/fail
            with self.assertRaises(KeyboardInterrupt):
                loop.run_sweep(CalysFake(interrupt=2), dut, 1, protocol.SENSORS[48], path, dwell=0)
            with path.open(encoding="utf-8-sig") as stream:
                rows = list(csv.DictReader(stream))
            self.assertEqual(len(rows), 3)
            self.assertEqual(rows[-1]["status"], "interrupted")
            self.assertEqual(dut.writes, [(0x1017, [48, 0, 0xA5A5])])

    @patch.object(loop, "modbus_helpers", helpers)
    @patch.object(loop.time, "sleep", lambda seconds: None)
    def test_no_fresh_sample(self):
        dut = DutFake()
        original = dut.read_holding_registers
        dut.read_holding_registers = lambda address, quantity: (
            [0, 5] if address == 0x4871 else original(address, quantity))
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "results.csv"
            with self.assertRaises(TimeoutError):
                loop.run_sweep(CalysFake(), dut, 1, protocol.SENSORS[48], path,
                               dwell=0, fresh_timeout=0)
            with path.open(encoding="utf-8-sig") as stream:
                self.assertEqual(list(csv.DictReader(stream))[0]["status"], "error")

    def test_calibration_placeholders(self):
        with patch("builtins.input", side_effect=[*loop.CALIBRATION_MENU, "q"]), \
             patch("builtins.print"), patch.object(loop, "Calys1500") as calys, \
             patch.object(loop, "modbus_helpers") as dut:
            with tempfile.TemporaryDirectory() as directory:
                loop.main(["--config", str(Path(directory) / "ports.json")])
            calys.assert_not_called()
            dut.assert_not_called()


if __name__ == "__main__":
    unittest.main()
