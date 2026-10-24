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
        self.sample_valid = 0
        self.actual_temperature = 0
        self.entries = []
    def log(self, **entry):
        self.entries.append(entry)
    def read(self, address, count):
        if address == 0x4968:
            self.sample_valid = int(bool(self.enabled and self.valid))
            if self.enabled and self.valid:
                self.sequence += 1
                self.actual_temperature = self.temperature
                self.count = min(3, self.count+1) if self.temperature >= 8500 else 0
            self.trip = int(self.count >= 3)
            if self.trip and not self.fail:
                self.fault = self.latch = 1
                self.events = 1
            return [2, self.enabled, self.valid, self.temperature, self.count, self.trip,
                    self.sequence >> 16, self.sequence & 0xFFFF]
        if address == 0x4964:
            return [self.sample_valid, self.actual_temperature,
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
    def __init__(self, unsafe_release=False, cleanup_failure=None, accept_bad_key=False,
                 invalid_clears=False):
        self.temperature = Board()
        self.voltage = LowVoltageBoard()
        self.index = self.result = self.successful_resets = 0
        self.unsafe_release, self.cleanup_failure = unsafe_release, cleanup_failure
        self.accept_bad_key = accept_bad_key
        self.invalid_clears = invalid_clears
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
            if self.invalid_clears and self.temperature.enabled and not self.temperature.valid:
                self.temperature.count = self.temperature.trip = t[4] = t[5] = 0
            return t
        if address == 0x4964:
            return self.temperature.read(address, count)
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
        assert address == 0x4809
        code = values[0]
        if values[1] != 0xC1EA:
            if self.accept_bad_key:
                self.result = 1
                return
            self.result = 6
            raise tool.ModbusError(3)
        if code not in (0x0201, 0x0202):
            self.result = 4
            raise tool.ModbusError(3)
        if code not in self.faults():
            self.result = 2
            raise tool.ModbusError(3)
        child = self.temperature if code == 0x0201 else self.voltage
        active = self.temperature.trip if code == 0x0201 else self.voltage.confirmed
        if active:
            self.result = 3
            raise tool.ModbusError(3)
        assert code in self.faults()
        self.result = 1
        self.successful_resets += 1
        child.fault = child.latch = 0


class InjectionGuardBoard(MultiFaultBoard):
    def __init__(self, mutate_on_reject=False, accept_invalid=False, fail_cleanup=False):
        super().__init__()
        self.mutate_on_reject = mutate_on_reject
        self.accept_invalid = accept_invalid
        self.fail_cleanup = fail_cleanup
        self.disable_calls = {0x4960: 0, 0x4970: 0}
    def write_single(self, address, value):
        if 0x4960 <= address <= 0x497F:
            raise tool.ModbusError(2)
        super().write_single(address, value)
    def write_multiple(self, address, values):
        if address in (0x4960, 0x4970) and len(values) == 4:
            if values[3] != 0x5AFE or values[0] > 1 or values[1] > 1:
                if self.accept_invalid:
                    return
                if self.mutate_on_reject:
                    self.temperature.enabled = self.temperature.valid = 1
                    self.temperature.temperature = 8500
                raise tool.ModbusError(3)
            if values[0] == 0:
                self.disable_calls[address] += 1
                if self.fail_cleanup and self.disable_calls[address] > 1:
                    raise TimeoutError('cleanup ACK lost')
            super().write_multiple(address, values)
            return
        if 0x4960 <= address <= 0x497F:
            raise tool.ModbusError(2)
        super().write_multiple(address, values)


class HistoryBoard:
    def __init__(self, corrupt=False):
        self.event_sequence = self.snapshot_sequence = 0
        self.indices = {0x4920: 0, 0x4940: 0}
        self.entries = []
        self.corrupt = corrupt
    def log(self, **entry):
        self.entries.append(entry)
    def read(self, address, count):
        if address == 0x4800:
            return [0]
        if address == 0x4968:
            return [2, 0, 0, 0, 0, 0, 0, 0]
        if address == 0x4974:
            return [2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 100]
        if address == 0x4810:
            return [0, 0, 0, 0, 0, 0, 0, 0, min(32, self.event_sequence),
                    min(16, self.snapshot_sequence), self.event_sequence >> 16,
                    self.event_sequence & 0xFFFF, self.snapshot_sequence >> 16,
                    self.snapshot_sequence & 0xFFFF, 0, 0]
        assert address in self.indices
        index = self.indices[address]
        latest = self.event_sequence if address == 0x4920 else self.snapshot_sequence
        retained = min(32 if address == 0x4920 else 16, latest)
        sequence = latest - retained + 1 + index
        if self.corrupt and index == 1:
            sequence -= 1  # Duplicate record after ring wrap.
        record = [0] * count
        record[0], record[1], record[2] = index, sequence >> 16, sequence & 0xFFFF
        record[5], record[6] = 2, 1
        return record
    def write_single(self, address, value):
        assert address in self.indices
        self.indices[address] = value


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

    def test_reset_guard_both_sources_and_cleanup(self):
        board = MultiFaultBoard()
        with patch.object(tool.time, 'sleep', return_value=None):
            tool.reset_guard_cycle(board)
        self.assertEqual([e['code'] for e in board.entries if e['event'] == 'reset_guard_source_pass'],
                         ['0x0201', '0x0202'])
        self.assertEqual(len([e for e in board.entries if e['event'] == 'reset_guard_rejection_pass']), 22)
        self.assertEqual(board.successful_resets, 2)
        self.assertEqual(board.faults(), {})
        self.assertEqual(board.cleanup_attempts, ['temperature', 'low_voltage'])
        self.assertTrue(any(e['event'] == 'reset_guard_software_cycle_pass' for e in board.entries))

    def test_reset_guard_detects_bad_key_acceptance(self):
        board = MultiFaultBoard(accept_bad_key=True)
        with patch.object(tool.time, 'sleep', return_value=None), \
             self.assertRaisesRegex(AssertionError, '卻接受 Reset'):
            tool.reset_guard_cycle(board)
        self.assertEqual(board.cleanup_attempts, ['temperature', 'low_voltage'])
        self.assertTrue(board.temperature.fault)
        self.assertFalse(any(e['event'] == 'reset_guard_software_cycle_pass' for e in board.entries))

    def test_reset_guard_cleanup_failure_no_pass(self):
        board = MultiFaultBoard(cleanup_failure='temperature')
        with patch.object(tool.time, 'sleep', return_value=None), \
             self.assertRaisesRegex(RuntimeError, '無法確認'):
            tool.reset_guard_cycle(board)
        self.assertEqual(board.cleanup_attempts, ['temperature', 'low_voltage'])
        self.assertFalse(any(e['event'] == 'reset_guard_software_cycle_pass' for e in board.entries))

    def test_reset_guard_existing_fault_never_writes(self):
        board = MultiFaultBoard()
        board.voltage.fault = board.voltage.latch = 1
        with patch.object(board, 'write_multiple') as write, self.assertRaisesRegex(AssertionError, '已有 Fault'):
            tool.reset_guard_cycle(board)
        write.assert_not_called()

    def test_repeat_all_cases_and_ring_overwrite(self):
        board = HistoryBoard()
        def run(client):
            client.event_sequence += 9
            client.snapshot_sequence += 3
        with patch.multiple(tool, temperature_cycle=run, low_voltage_cycle=run,
                            multi_fault_cycle=run, reset_guard_cycle=run):
            tool.repeat_cycle(board, 2)
        self.assertEqual(len([e for e in board.entries if e['event'] == 'repeat_case_pass']), 8)
        self.assertEqual(len([e for e in board.entries if e['event'] == 'repeat_round_pass']), 2)
        self.assertTrue(any(e['event'] == 'repeat_software_cycle_pass' for e in board.entries))
        self.assertTrue(any(e['event'] == 'retained_record' for e in board.entries))

    def test_repeat_detects_duplicate_history(self):
        board = HistoryBoard(corrupt=True)
        def run(client):
            client.event_sequence += 9
            client.snapshot_sequence += 3
        with patch.multiple(tool, temperature_cycle=run, low_voltage_cycle=run,
                            multi_fault_cycle=run, reset_guard_cycle=run), \
             self.assertRaisesRegex(AssertionError, '環形紀錄'):
            tool.repeat_cycle(board, 2)
        self.assertFalse(any(e['event'] == 'repeat_software_cycle_pass' for e in board.entries))

    def test_repeat_detects_sequence_reset(self):
        board = HistoryBoard()
        calls = []
        def run(client):
            calls.append(1)
            client.event_sequence += 9
            client.snapshot_sequence += 3
            if len(calls) == 8:
                client.event_sequence = 1
        with patch.multiple(tool, temperature_cycle=run, low_voltage_cycle=run,
                            multi_fault_cycle=run, reset_guard_cycle=run), \
             self.assertRaisesRegex(AssertionError, '序號未前進'):
            tool.repeat_cycle(board, 2)

    def test_repeat_stops_at_first_case_failure(self):
        board = HistoryBoard()
        calls = []
        def run(client):
            calls.append(1)
            if len(calls) == 2:
                raise RuntimeError('case failed after cleanup')
        with patch.multiple(tool, temperature_cycle=run, low_voltage_cycle=run,
                            multi_fault_cycle=run, reset_guard_cycle=run), \
             self.assertRaisesRegex(RuntimeError, 'case failed'):
            tool.repeat_cycle(board, 2)
        self.assertEqual(len(calls), 2)

    def test_repeat_invalid_rounds_no_io(self):
        for cycles in (0, 1, 101):
            with self.assertRaisesRegex(AssertionError, '輪數'):
                tool.repeat_cycle(None, cycles)

    def test_sequence_wrap_skips_zero(self):
        self.assertEqual(tool.sequence_distance(0xFFFFFFFE, 1), 2)
        self.assertEqual(tool.sequence_distance(0, 10), 10)

    def test_invalid_sample_both_phases(self):
        board = MultiFaultBoard()
        clock = itertools.count(0, 0.05)
        with patch.object(tool.time, 'sleep', return_value=None), \
             patch.object(tool.time, 'monotonic', side_effect=lambda: next(clock)):
            tool.invalid_sample_cycle(board)
        self.assertEqual([e['requested_temperature'] for e in board.entries
                          if e['event'] == 'invalid_sample_hold_pass'], [8500, 8499])
        self.assertEqual(board.temperature.enabled, 0)
        self.assertEqual(board.faults(), {})
        self.assertTrue(any(e['event'] == 'invalid_sample_software_cycle_pass' for e in board.entries))

    def test_invalid_sample_catches_false_recovery(self):
        board = MultiFaultBoard(invalid_clears=True)
        clock = itertools.count(0, 0.05)
        with patch.object(tool.time, 'sleep', return_value=None), \
             patch.object(tool.time, 'monotonic', side_effect=lambda: next(clock)), \
             self.assertRaisesRegex(AssertionError, '計數／成立狀態'):
            tool.invalid_sample_cycle(board)
        self.assertEqual(board.temperature.enabled, 0)
        self.assertTrue(board.temperature.fault)

    def test_invalid_sample_old_firmware_never_writes(self):
        board = MultiFaultBoard()
        with patch.object(board, 'read', side_effect=tool.ModbusError(2)), \
             patch.object(board, 'write_multiple') as write, self.assertRaises(tool.ModbusError):
            tool.invalid_sample_cycle(board)
        write.assert_not_called()

    def test_invalid_sample_cleanup_failure_no_pass(self):
        board = MultiFaultBoard(cleanup_failure='temperature')
        clock = itertools.count(0, 0.05)
        with patch.object(tool.time, 'sleep', return_value=None), \
             patch.object(tool.time, 'monotonic', side_effect=lambda: next(clock)), \
             self.assertRaisesRegex(RuntimeError, '無法確認'):
            tool.invalid_sample_cycle(board)
        self.assertFalse(any(e['event'] == 'invalid_sample_software_cycle_pass' for e in board.entries))

    def test_injection_guard_all_rejections(self):
        board = InjectionGuardBoard()
        with patch.object(tool.time, 'sleep', return_value=None):
            tool.injection_guard_cycle(board)
        rejected = [e for e in board.entries if e['event'] == 'injection_guard_rejection_pass']
        self.assertEqual(len(rejected), 26)
        self.assertTrue(any(e['function'] == 6 for e in rejected))
        self.assertTrue(any(e['event'] == 'injection_guard_software_cycle_pass' for e in board.entries))
        self.assertEqual((board.temperature.enabled, board.voltage.enabled), (0, 0))

    def test_injection_guard_catches_mutation_despite_exception(self):
        board = InjectionGuardBoard(mutate_on_reject=True)
        with patch.object(tool.time, 'sleep', return_value=None), \
             self.assertRaisesRegex(AssertionError, '雖拒絕卻改變'):
            tool.injection_guard_cycle(board)
        self.assertEqual((board.temperature.enabled, board.voltage.enabled), (0, 0))

    def test_injection_guard_catches_accepted_bad_key(self):
        board = InjectionGuardBoard(accept_invalid=True)
        with patch.object(tool.time, 'sleep', return_value=None), \
             self.assertRaisesRegex(AssertionError, '誤接受'):
            tool.injection_guard_cycle(board)
        self.assertFalse(any(e['event'] == 'injection_guard_software_cycle_pass' for e in board.entries))

    def test_injection_guard_cleanup_attempts_both(self):
        board = InjectionGuardBoard(fail_cleanup=True)
        with patch.object(tool.time, 'sleep', return_value=None), \
             self.assertRaisesRegex(RuntimeError, '無法確認'):
            tool.injection_guard_cycle(board)
        self.assertEqual(board.disable_calls, {0x4960: 2, 0x4970: 2})
        self.assertFalse(any(e['event'] == 'injection_guard_software_cycle_pass' for e in board.entries))

    def test_injection_guard_unsupported_never_writes(self):
        board = InjectionGuardBoard()
        with patch.object(board, 'read', side_effect=tool.ModbusError(2)), \
             patch.object(board, 'write_multiple') as write, self.assertRaises(tool.ModbusError):
            tool.injection_guard_cycle(board)
        write.assert_not_called()


if __name__ == '__main__':
    unittest.main()
