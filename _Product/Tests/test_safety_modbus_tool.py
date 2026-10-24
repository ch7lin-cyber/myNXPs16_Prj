"""離線測試 Python 的 RTU 收發、測試判定及失敗清理，不宣稱上板通過。"""
import importlib.util
import itertools
from pathlib import Path
import struct
import unittest
from unittest.mock import patch

spec = importlib.util.spec_from_file_location('safety_tool', Path(__file__).parents[1]/'Tools/safety_modbus_test.py')
tool = importlib.util.module_from_spec(spec)
spec.loader.exec_module(tool)


class FakePort:
    timeout = 0.05
    def __init__(self, corrupt=False):
        self.rx = b''
        self.corrupt = corrupt
    def reset_input_buffer(self):
        self.rx = b''
    def write(self, frame):
        self.request = frame
        if frame[1] == 3:
            body = bytes([frame[0], 3, 4])+struct.pack('>HH', 0x1234, 0x5678)
        else:
            body = frame[:6]
        self.rx = body+struct.pack('<H', tool.crc16(body))
        if self.corrupt:
            self.rx = self.rx[:-1]+bytes([self.rx[-1]^1])
        return len(frame)
    def read(self, count):
        # Exercise fragmented serial replies.
        result, self.rx = self.rx[:min(count, 2)], self.rx[min(count, 2):]
        return result


class Board:
    """Independent state machine for the cycle and cleanup assertions."""
    def __init__(self, fail=False):
        self.enabled = self.valid = self.count = self.trip = self.sequence = self.temperature = 0
        self.fault = self.latch = self.result = self.samples = 0
        self.fail = fail
        self.events = 0
        self.entries = []
    def log(self, **entry):
        self.entries.append(entry)
    def read(self, address, count):
        if address == 0x4968:
            self.sequence += 1
            if self.enabled and self.valid:
                self.count = min(3, self.count+1) if self.temperature >= 8500 else 0
            self.trip = int(self.count >= 3)
            if self.trip and not self.fail:
                self.fault = self.latch = 1
                self.events = 1
            return [2, self.enabled, self.valid, self.temperature, self.count, self.trip,
                    self.sequence >> 16, self.sequence & 0xFFFF]
        if address == 0x4800:
            return [self.fault]
        if address == 0x4802:
            return [0x0201, 8500]
        if address == 0x480B:
            return [self.result]
        if address == 0x4810:
            return [0, 0, 0, self.trip if not self.fail else 0, 0, self.latch,
                    0, self.latch, self.events, self.events, 0, self.events, 0, self.events,
                    self.latch, self.latch]
        raise AssertionError(hex(address))
    def write_single(self, address, value):
        assert address == 0x4801 and value == 0
    def write_multiple(self, address, values):
        if address == 0x4960:
            self.enabled, self.valid, self.temperature, key = values
            assert key == 0x5AFE
            if not self.enabled:
                self.count = self.trip = 0
        else:
            assert address == 0x4809 and values == [0x0201, 0xC1EA]
            if self.trip:
                self.result = 3
                raise tool.ModbusError(3)
            self.result = 1
            self.fault = self.latch = 0


class LowVoltageBoard:
    """Run independent input/debounce steps between Modbus observations."""
    def __init__(self, fail=False, cleanup_fail=False):
        self.enabled = self.active = self.limit = self.remaining = 0
        self.processed = self.peak_assert = self.peak_release = 0
        self.count = self.release = self.confirmed = self.ever = self.raw = 0
        self.sequence = self.events = self.fault = self.latch = self.result = 0
        self.fail, self.cleanup_fail = fail, cleanup_fail
        self.entries = []
    def log(self, **entry):
        self.entries.append(entry)
    def read(self, address, count):
        if address == 0x4974:
            for _ in range(100):
                self.sequence += 1
                self.raw = int(self.enabled and self.active and
                               (not self.limit or self.remaining > 0))
                if self.raw:
                    self.processed += 1
                    self.remaining = max(0, self.remaining - 1)
                    self.release = 0
                    self.count = min(3, self.count + 1)
                    if self.count == 3:
                        self.confirmed = 1
                else:
                    self.count = 0
                    if self.confirmed:
                        self.release += 1
                        if self.release == 100:
                            self.confirmed = 0
                    else:
                        self.release = 0
                self.peak_assert = max(self.peak_assert, self.count)
                self.peak_release = max(self.peak_release, self.release)
                self.ever |= self.confirmed
                if self.confirmed and not self.fail and not self.fault:
                    self.fault = self.latch = 1
                    self.events += 1
            flags = self.confirmed | (self.ever << 1) | (self.raw << 2)
            flags |= 8 if self.limit and not self.remaining else 0
            return [2, self.enabled, self.active, self.limit, self.processed,
                    self.peak_assert, self.peak_release, flags,
                    self.sequence >> 16, self.sequence & 0xFFFF, 3, 100]
        if address == 0x4800:
            return [self.fault]
        if address == 0x4802:
            return [0x0202, 1]
        if address == 0x480B:
            return [self.result]
        if address == 0x4810:
            live = 2 if self.confirmed and not self.fail else 0
            return [0, 0, 0, live, 0, self.latch * 2, 0, self.latch * 2,
                    self.events, self.events, 0, self.events, 0, self.events,
                    self.latch, self.latch]
        raise AssertionError(hex(address))
    def write_single(self, address, value):
        assert address == 0x4801 and value == 0
    def write_multiple(self, address, values):
        if address == 0x4970:
            if not values[0] and self.cleanup_fail:
                raise TimeoutError('lost cleanup ACK')
            self.enabled, self.active, self.limit, key = values
            assert key == 0x5AFE
            self.remaining = self.limit
            self.processed = self.peak_assert = self.peak_release = self.ever = 0
        else:
            assert address == 0x4809 and values == [0x0202, 0xC1EA]
            if self.confirmed:
                self.result = 3
                raise tool.ModbusError(3)
            self.result = 1
            self.fault = self.latch = 0


class MultiFaultBoard:
    """Compose independent temperature/voltage models with per-source resets."""
    def __init__(self, unsafe_release=False, cleanup_failure=None):
        self.temperature = Board()
        self.voltage = LowVoltageBoard()
        self.index = self.result = self.successful_resets = 0
        self.unsafe_release, self.cleanup_failure = unsafe_release, cleanup_failure
        self.entries, self.cleanup_attempts = [], []
    def log(self, **entry):
        self.entries.append(entry)
    def faults(self):
        result = {}
        if self.temperature.fault:
            result[0x0201] = 8500
        if self.voltage.fault:
            result[0x0202] = 1
        return result
    def read(self, address, count):
        t = self.temperature.read(0x4968, 8)
        v = self.voltage.read(0x4974, 12)
        if address == 0x4968:
            return t
        if address == 0x4974:
            return v
        if address == 0x4800:
            return [len(self.faults())]
        if address == 0x4802:
            return list(list(self.faults().items())[self.index])
        if address == 0x480B:
            return [self.result]
        if address == 0x4810:
            live = self.temperature.trip | (self.voltage.confirmed << 1)
            latch = self.temperature.latch | (self.voltage.latch << 1)
            inhibit = int(bool(live or latch))
            if self.unsafe_release and self.successful_resets and live:
                inhibit = 0  # Regression: resetting one source releases all outputs.
            events = self.temperature.events + self.voltage.events
            return [0, 0, 0, live, 0, latch, 0, latch, events, events,
                    0, events, 0, events, int(bool(live or latch)), inhibit]
        raise AssertionError(hex(address))
    def write_single(self, address, value):
        assert address == 0x4801
        self.index = value
    def write_multiple(self, address, values):
        if address in (0x4960, 0x4970):
            name = 'temperature' if address == 0x4960 else 'low_voltage'
            if not values[0]:
                self.cleanup_attempts.append(name)
                if self.cleanup_failure == name:
                    raise TimeoutError('lost cleanup ACK')
            child = self.temperature if address == 0x4960 else self.voltage
            child.write_multiple(address, values)
            return
        assert address == 0x4809 and values[1] == 0xC1EA
        code = values[0]
        child = self.temperature if code == 0x0201 else self.voltage
        active = self.temperature.trip if code == 0x0201 else self.voltage.confirmed
        if active:
            self.result = 3
            raise tool.ModbusError(3)
        assert code in self.faults()
        self.result = 1
        self.successful_resets += 1
        child.fault = child.latch = 0


class Tests(unittest.TestCase):
    def test_crc_fragmented_and_ack(self):
        self.assertEqual(tool.crc16(b'123456789'), 0x4B37)
        c = tool.Client(FakePort())
        self.assertEqual(c.read(0x4800, 2), [0x1234, 0x5678])
        c.write_single(0x4801, 0)
        self.assertEqual(c.port.request[1], 6)
        c.write_multiple(0x4960, [1, 1, 8499, 0x5AFE])
        self.assertEqual(c.port.request[1], 16)
        with self.assertRaises(ValueError):
            tool.Client(FakePort(), 0)
    def test_crc_rejected(self):
        with self.assertRaises(IOError):
            tool.Client(FakePort(True)).read(0x4800, 2)
    def test_cycle_pass_and_cleanup(self):
        board = Board()
        with patch.object(tool.time, 'sleep', return_value=None):
            tool.temperature_cycle(board)
        self.assertEqual(board.enabled, 0)
        self.assertEqual(board.fault, 0)
        self.assertTrue(any(e['event'] == 'software_cycle_pass' for e in board.entries))
    def test_cycle_failure_does_not_report_pass(self):
        board = Board(fail=True)
        with patch.object(tool.time, 'sleep', return_value=None), self.assertRaises(AssertionError):
            tool.temperature_cycle(board)
        self.assertEqual(board.enabled, 0)
        self.assertFalse(any(e['event'] == 'software_cycle_pass' for e in board.entries))

    def test_low_voltage_pass_and_cleanup(self):
        board = LowVoltageBoard()
        with patch.object(tool.time, 'sleep', return_value=None):
            tool.low_voltage_cycle(board)
        self.assertEqual(board.enabled, 0)
        self.assertEqual(board.fault, 0)
        self.assertEqual([e['samples'] for e in board.entries
                          if e['event'] == 'low_voltage_short_pulse_pass'], [1, 2])
        self.assertTrue(any(e['event'] == 'low_voltage_software_cycle_pass' for e in board.entries))

    def test_low_voltage_failure_cleanup(self):
        board = LowVoltageBoard(fail=True)
        with patch.object(tool.time, 'sleep', return_value=None), self.assertRaises(AssertionError):
            tool.low_voltage_cycle(board)
        self.assertEqual(board.enabled, 0)
        self.assertFalse(any(e['event'] == 'low_voltage_software_cycle_pass' for e in board.entries))

    def test_low_voltage_cleanup_failure_is_failure(self):
        board = LowVoltageBoard(cleanup_fail=True)
        with patch.object(tool.time, 'sleep', return_value=None), self.assertRaisesRegex(RuntimeError, '無法確認'):
            tool.low_voltage_cycle(board)
        self.assertTrue(any(e['event'] == 'cleanup_failed' for e in board.entries))

    def test_low_voltage_unsupported_stops_before_writes(self):
        board = LowVoltageBoard()
        with patch.object(board, 'read', side_effect=tool.ModbusError(2)), \
             patch.object(board, 'write_multiple') as write, self.assertRaises(tool.ModbusError):
            tool.low_voltage_cycle(board)
        write.assert_not_called()

    def test_multi_fault_both_orders(self):
        board = MultiFaultBoard()
        with patch.object(tool.time, 'sleep', return_value=None):
            tool.multi_fault_cycle(board)
        self.assertEqual([e['order'] for e in board.entries if e['event'] == 'multi_fault_order_pass'],
                         ['temperature-first', 'low-voltage-first'])
        self.assertEqual(board.successful_resets, 4)
        self.assertEqual(board.faults(), {})
        self.assertEqual((board.temperature.enabled, board.voltage.enabled), (0, 0))
        self.assertTrue(any(e['event'] == 'multi_fault_software_cycle_pass' for e in board.entries))

    def test_multi_fault_catches_unsafe_output_release(self):
        board = MultiFaultBoard(unsafe_release=True)
        clock = itertools.count(0, 0.1)
        with patch.object(tool.time, 'sleep', return_value=None), \
             patch.object(tool.time, 'monotonic', side_effect=lambda: next(clock)), \
             self.assertRaises(TimeoutError):
            tool.multi_fault_cycle(board)
        self.assertEqual(board.cleanup_attempts, ['temperature', 'low_voltage'])
        self.assertTrue(board.voltage.fault)  # Failure cleanup never resets the other fault.
        self.assertFalse(any(e['event'] == 'multi_fault_software_cycle_pass' for e in board.entries))

    def test_multi_fault_cleanup_attempts_both_after_one_failure(self):
        for name in ('temperature', 'low_voltage'):
            with self.subTest(name=name):
                board = MultiFaultBoard(cleanup_failure=name)
                with patch.object(tool.time, 'sleep', return_value=None), \
                     self.assertRaisesRegex(RuntimeError, '無法確認'):
                    tool.multi_fault_cycle(board)
                self.assertEqual(board.cleanup_attempts, ['temperature', 'low_voltage'])
                self.assertFalse(any(e['event'] == 'multi_fault_software_cycle_pass' for e in board.entries))

    def test_multi_fault_probe_failure_never_writes(self):
        board = MultiFaultBoard()
        original = board.read
        def read(address, count):
            if address == 0x4974:
                raise tool.ModbusError(2)
            return original(address, count)
        with patch.object(board, 'read', side_effect=read), \
             patch.object(board, 'write_multiple') as write, self.assertRaises(tool.ModbusError):
            tool.multi_fault_cycle(board)
        write.assert_not_called()

    def test_multi_fault_interrupt_cleans_both(self):
        board = MultiFaultBoard()
        original = board.write_multiple
        def write(address, values):
            if address == 0x4960 and values[2] == 8500:
                raise KeyboardInterrupt()
            original(address, values)
        with patch.object(tool.time, 'sleep', return_value=None), \
             patch.object(board, 'write_multiple', side_effect=write), \
             self.assertRaises(KeyboardInterrupt):
            tool.multi_fault_cycle(board)
        self.assertEqual(board.cleanup_attempts, ['temperature', 'low_voltage'])
        self.assertEqual((board.temperature.enabled, board.voltage.enabled), (0, 0))

    def test_multi_fault_existing_fault_never_writes(self):
        board = MultiFaultBoard()
        board.temperature.fault = board.temperature.latch = 1
        with patch.object(board, 'write_multiple') as write, self.assertRaisesRegex(AssertionError, '已有 Fault'):
            tool.multi_fault_cycle(board)
        write.assert_not_called()


if __name__ == '__main__':
    unittest.main()
