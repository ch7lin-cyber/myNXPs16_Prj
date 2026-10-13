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
from calys1500_expected import pv_comparison


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
        self.timeout = 5
    def write(self, data):
        self.writes.append(data)
    def flush(self):
        pass
    def readline(self):
        return next(self.replies, b"")
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
    def write_single_register(self, address, value):
        self.writes.append((address, value))
    def read_holding_registers(self, address, quantity):
        if address == 0x1018:
            return [1]
        if address == 0x1017:
            return [48]
        if address == 0x4871:
            self.count += 1
            return [0, self.count]
        if address == 0x1010:
            return [0x3F80, 0, 61]  # 1.0 PV, no input error
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

    def test_analog_pv_scaling_endpoints(self):
        sensor = protocol.SENSORS[124]  # 4-20 mA
        physical, expected = pv_comparison(sensor, sensor.low, -32000.0)
        self.assertEqual((physical, expected), (4.0, -32000.0))
        physical, expected = pv_comparison(sensor, sensor.high, 32000.0)
        self.assertEqual((physical, expected), (20.0, 32000.0))
        physical, expected = pv_comparison(sensor, 12.0, 0.0)
        self.assertEqual((physical, expected), (12.0, 0.0))

    @patch.object(protocol.time, "sleep")
    def test_protocol_and_errors(self, sleep):
        transport = Transport([b'AOIP SAS,CALYS1500,SN,A11\r\n', b'0,"No error"\r\n',
                               b'5,"range error"\r\n'])
        calys = protocol.Calys1500("fake", transport=transport)
        with self.assertRaises(protocol.CalysError):
            calys.output(protocol.SENSORS[120], "1")
        calys.close()
        self.assertEqual(transport.writes, [b'REM\n', b'*CLS\n', b'*IDN?\n',
                                          b'ERR?\n', b'SOUR:VOLT 1\n', b'ERR?\n', b'LOC\n'])
        self.assertTrue(transport.closed)
        self.assertIn(unittest.mock.call(0.15), sleep.call_args_list)
        transport = Transport([b'OTHER DEVICE\r\n'])
        with self.assertRaises(protocol.CalysError):
            protocol.Calys1500("fake", transport=transport)
        self.assertTrue(transport.closed)

    @patch.object(protocol.time, "sleep")
    def test_fragmented_idn_and_blank_line(self, sleep):
        transport = Transport([b'\r\n', b'AOIP SAS,CALYS', b'1500,SN,A11\r\n',
                               b'0,"No error"\r\n'])
        calys = protocol.Calys1500("COM12", transport=transport)
        self.assertEqual(calys.identity, "AOIP SAS,CALYS1500,SN,A11")
        self.assertEqual(transport.timeout, 5)
        calys.close()

    @patch.object(protocol.time, "sleep")
    def test_identification_retry_and_diagnostics(self, sleep):
        transport = Transport([b'AOIP SAS,CALYS1500,SN,A11\r\n', b'0,"No error"\r\n'])
        original = protocol.Calys1500.query
        attempts = []
        def query(client, command):
            if command == "*IDN?":
                attempts.append(command)
                if len(attempts) == 1:
                    raise protocol.CalysError("temporary timeout")
            return original(client, command)
        with patch.object(protocol.Calys1500, "query", query):
            calys = protocol.Calys1500("COM12", transport=transport)
        self.assertEqual(len(attempts), 2)
        self.assertEqual(transport.writes.count(b'REM\n'), 2)
        calys.close()
        transport = Transport([])
        with self.assertRaisesRegex(protocol.CalysError, "port=COM12.*RX=b''"):
            protocol.Calys1500("COM12", timeout=0.001, transport=transport)
        self.assertEqual(transport.writes.count(b'*IDN?\n'), 3)
        self.assertTrue(transport.closed)

    @patch.object(loop, "modbus_helpers", helpers)
    @patch.object(loop.time, "sleep", lambda seconds: None)
    def test_loop_and_partial_csv(self):
        dut, calys = DutFake(), CalysFake()
        loop.configure_dut(dut, 1, protocol.SENSORS[48], 0)
        self.assertEqual(dut.writes, [(0x1017, [48, 1, 0xA5A5])])
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
            self.assertEqual(dut.writes, [(0x1017, [48, 1, 0xA5A5])])

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

    def test_wiring_groups_and_unique_profiles(self):
        from calys1500_calibration import WIRING_GROUPS
        self.assertEqual(set(WIRING_GROUPS), {"1", "2", "3", "4"})
        self.assertIn(0, WIRING_GROUPS["1"].profiles)
        self.assertIn(0, WIRING_GROUPS["3"].profiles)  # shared firmware profile
        for group in WIRING_GROUPS.values():
            self.assertEqual(len(group.profiles), len(set(group.profiles)))
            for profile, codes in group.profiles.items():
                self.assertTrue(all(code in protocol.SENSORS for code in codes))

    def test_calibration_menu_selection_parsing(self):
        self.assertEqual(loop.parse_wiring_group_selection("2.1"), "1")
        self.assertEqual(loop.parse_wiring_group_selection(" 2.4 "), "4")
        self.assertEqual(loop.parse_wiring_group_selection("1"), "1")
        self.assertIsNone(loop.parse_wiring_group_selection("q"))
        with self.assertRaisesRegex(ValueError, "2.1"):
            loop.parse_wiring_group_selection("2.5")

        self.assertEqual(loop.parse_profile_selection("P0"), 0)
        self.assertEqual(loop.parse_profile_selection(" p7 "), 7)
        self.assertEqual(loop.parse_profile_selection("3"), 3)
        self.assertIsNone(loop.parse_profile_selection("Q"))
        with self.assertRaisesRegex(ValueError, "P0"):
            loop.parse_profile_selection("profile0")

    def test_calibration_preview_aborts_without_apply(self):
        from calys1500_calibration import WIRING_GROUPS, guided_profile_run

        def i32_regs(value):
            value &= 0xFFFFFFFF
            return [value >> 16, value & 0xFFFF]

        class Calys:
            def __init__(self, dut):
                self.dut = dut
                self.value = 0
            def configure(self, sensor, **settings):
                self.sensor = sensor
            def output(self, sensor, value):
                self.value = 1000 if value == 0 else 51000
                self.dut.microvolts = self.value

        class Dut:
            def __init__(self):
                self.writes = []
                self.count = 0
                self.microvolts = 0
                self.pending = [0, 0, 0, 0]
                self.record = [0, 0, 0, 60417, 1, 0, 0, 0]
            def read_holding_registers(self, address, quantity):
                if address == 0x4724:
                    return [1]
                if address == 0x4726:
                    return [0]
                if address == 0x4706:
                    return [0]
                if address == 0x4720:
                    return [0, 0, 0, 60417]
                if address == 0x4C20:
                    return self.record
                if address == 0x4709:
                    return self.pending
                if address == 0x4831:
                    self.count += 1
                    return [0, self.count]
                if address in (0x1000, 0x1010):
                    return [0x3F80, 0, 61]
                if address in (0x484F, 0x488F):
                    return [0x80, 1, 0, 255, *i32_regs(self.microvolts)]
                if address == 0x4823:
                    return [0]
                raise AssertionError((hex(address), quantity))
            def write_multiple_registers(self, address, values):
                self.writes.append((address, list(values)))
            def write_single_register(self, address, value):
                self.writes.append((address, value))
                if address == 0x4702 and value == 2:
                    self.pending[0:2] = i32_regs(self.microvolts)
                if address == 0x4702 and value == 3:
                    self.pending[2:4] = i32_regs(self.microvolts)
                if address == 0x4702 and value == 4:
                    self.record = [*self.pending, 1, 1, 0, 0]

        dut = Dut()
        calys = Calys(dut)
        inputs = iter(["0", "5"])
        report = guided_profile_run(
            calys, dut, 0, WIRING_GROUPS["3"], 4, protocol.SENSORS[120],
            allow_apply=False, dwell=0, stable_timeout=1, spread_limit_uv=20,
            input_fn=lambda prompt: next(inputs), sleep=lambda _: None)
        self.assertFalse(report["applied"])
        self.assertEqual((report["measured_zero_uv"], report["measured_span_uv"]),
                         (1000, 51000))
        self.assertIn((0x4702, 5), dut.writes)  # abort
        self.assertNotIn((0x4702, 4), dut.writes)  # no NVM Apply
        self.assertIn((0x4700, [0x1234, 0x1234]), dut.writes)

        dut = Dut()
        calys = Calys(dut)
        inputs = iter(["0", "5", "  apply_ch0_profile_4  "])
        report = guided_profile_run(
            calys, dut, 0, WIRING_GROUPS["3"], 4, protocol.SENSORS[120],
            allow_apply=True, dwell=0, stable_timeout=1, spread_limit_uv=20,
            input_fn=lambda prompt: next(inputs), sleep=lambda _: None)
        self.assertTrue(report["applied"])
        self.assertIn((0x4702, 4), dut.writes)

    @patch.object(loop.time, "sleep")
    def test_dut_read_retry_only(self, sleep):
        from unittest.mock import Mock
        client = Mock()
        client.read_holding_registers.side_effect = [
            RuntimeError("response timeout: expected 3-byte header, received 0"), [0, 50]]
        dut = loop.DutLink(client)
        self.assertEqual(dut.read_holding_registers(0x4831, 2), [0, 50])
        self.assertEqual(dut.read_retries, 1)
        self.assertEqual(client.read_holding_registers.call_count, 2)
        client.read_holding_registers.reset_mock()
        client.read_holding_registers.side_effect = RuntimeError("Modbus exception: code=0x02")
        with self.assertRaises(RuntimeError):
            dut.read_holding_registers(0x4831, 2)
        self.assertEqual(client.read_holding_registers.call_count, 1)
        client.write_multiple_registers.side_effect = RuntimeError("response timeout")
        with self.assertRaises(RuntimeError):
            dut.write_multiple_registers(0x1007, [113, 0, 0xA5A5])
        self.assertEqual(client.write_multiple_registers.call_count, 1)

    @patch.object(loop, "modbus_helpers", helpers)
    @patch.object(loop.time, "sleep", lambda seconds: None)
    def test_sweep_recovers_timeout_without_redundant_count(self):
        client = DutFake()
        original = client.read_holding_registers
        addresses = []
        def read(address, quantity):
            addresses.append(address)
            if address == 0x4871 and addresses.count(0x4871) == 1:
                raise RuntimeError("response timeout: expected 3-byte header, received 0")
            return original(address, quantity)
        client.read_holding_registers = read
        dut = loop.DutLink(client)
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "results.csv"
            loop.run_sweep(CalysFake(), dut, 1, protocol.SENSORS[48], path, dwell=0)
            with path.open(encoding="utf-8-sig") as stream:
                rows = list(csv.DictReader(stream))
            self.assertEqual(len(rows), 10)
            self.assertEqual(rows[0]["dut_read_retries"], "1")
            self.assertEqual(rows[-1]["status"], "recorded")
            self.assertEqual(addresses.count(0x4871), 21)  # 2/point + one retry

    @patch.object(loop.time, "sleep")
    def test_dut_read_retry_exhausted(self, sleep):
        from unittest.mock import Mock
        client = Mock()
        client.read_holding_registers.side_effect = RuntimeError("response CRC error")
        dut = loop.DutLink(client)
        with self.assertRaises(RuntimeError):
            dut.read_holding_registers(0x4831, 2)
        self.assertEqual(client.read_holding_registers.call_count, 3)
        self.assertEqual(dut.read_retries, 2)


if __name__ == "__main__":
    unittest.main()

