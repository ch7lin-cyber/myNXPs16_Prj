"""離線測試 Python 的 RTU 收發、測試判定及失敗清理，不宣稱上板通過。"""
import importlib.util
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


if __name__ == '__main__':
    unittest.main()
