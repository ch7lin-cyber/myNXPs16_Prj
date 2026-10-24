#!/usr/bin/env python3
"""SafetyTest 上板工具：FC03／06／10、過溫／低電壓循環與 JSONL 紀錄。"""
import argparse
import hashlib
from contextlib import contextmanager
import datetime
import json
import pathlib
import struct
import time


def crc16(data):
    crc = 0xFFFF
    for value in data:
        crc ^= value
        for _ in range(8):
            crc = (crc >> 1) ^ (0xA001 if crc & 1 else 0)
    return crc


class ModbusError(Exception):
    def __init__(self, code):
        self.code = code
        super().__init__(f"Modbus exception 0x{code:02X}")


class Client:
    def __init__(self, port, station=2, log=lambda **kw: None):
        if not 1 <= station <= 247:
            raise ValueError("站號必須為 1..247，不允許廣播")
        self.port, self.station, self.log = port, station, log

    def exact(self, size):
        result = bytearray()
        deadline = time.monotonic() + self.port.timeout
        while len(result) < size and time.monotonic() < deadline:
            result.extend(self.port.read(size-len(result)))
        if len(result) != size:
            raise TimeoutError(f"回覆不完整：預期 {size} bytes，收到 {len(result)}")
        return bytes(result)

    def exchange(self, function, body):
        request = bytes([self.station, function]) + body
        request += struct.pack('<H', crc16(request))
        time.sleep(0.05)
        self.port.reset_input_buffer()
        self.log(event='TX', frame=request.hex().upper())
        if self.port.write(request) != len(request):
            raise IOError('傳送不完整')
        header = self.exact(3)
        count = 2 if header[1] & 0x80 else header[2]+2 if function == 3 else 5
        if count > 252:
            raise IOError('回覆長度超限')
        reply = header + self.exact(count)
        self.log(event='RX', frame=reply.hex().upper())
        if crc16(reply) or reply[0] != self.station or reply[1] not in (function, function | 0x80):
            raise IOError('回覆 CRC／站號／功能碼錯誤')
        if reply[1] & 0x80:
            raise ModbusError(reply[2])
        if function != 3 and reply[:6] != request[:6]:
            raise IOError('寫入 ACK 不符合請求')
        return reply

    def read(self, address, count):
        if not 1 <= count <= 125 or not 0 <= address <= 65536-count:
            raise ValueError('FC03 位址／數量錯誤')
        reply = self.exchange(3, struct.pack('>HH', address, count))
        if reply[2] != 2*count:
            raise IOError('FC03 回覆數量錯誤')
        return list(struct.unpack(f'>{count}H', reply[3:-2]))

    def write_single(self, address, value):
        self.exchange(6, struct.pack('>HH', address, value))

    def write_multiple(self, address, values):
        if not 1 <= len(values) <= 123 or not 0 <= address <= 65536-len(values):
            raise ValueError('FC10 位址／數量錯誤')
        self.exchange(16, struct.pack('>HHB', address, len(values), 2*len(values)) +
                      struct.pack(f'>{len(values)}H', *values))

    def broadcast_test_command(self, address, values, function=16):
        # Narrow test entry only: never broadcast product settings or Fault Reset.
        if function == 16 and address in (0x4960, 0x4970) and len(values) == 4:
            if values[0] not in (0, 1) or values[1] not in (0, 1) or values[3] != 0x5AFE:
                raise ValueError('廣播測試參數錯誤')
            body = struct.pack('>HHB4H', address, 4, 8, *values)
        elif function == 6 and address in (0x4960, 0x4963, 0x4970, 0x4973) and len(values) == 1:
            body = struct.pack('>HH', address, values[0])
        else:
            raise ValueError('廣播只允許指定 SafetyTest 命令位址及數量')
        frame = bytes([0, function]) + body
        frame += struct.pack('<H', crc16(frame))
        time.sleep(0.05)
        self.port.reset_input_buffer()
        self.log(event='TX', broadcast=True, frame=frame.hex().upper())
        if self.port.write(frame) != len(frame):
            raise IOError('廣播傳送不完整')
        self.port.flush()  # Start silence observation after TX drained.
        original_timeout = self.port.timeout
        silence_seconds = max(0.25, original_timeout)
        try:
            self.port.timeout = min(0.05, original_timeout)
            deadline = time.monotonic() + silence_seconds
            while time.monotonic() < deadline:
                reply = self.port.read(256)
                if reply:
                    self.log(event='RX', broadcast=True, frame=reply.hex().upper())
                    raise IOError('廣播收到回覆或 Echo；預期完全靜默')
        finally:
            self.port.timeout = original_timeout
        self.log(event='broadcast_silence_pass', observation_seconds=silence_seconds)


def require(condition, message):
    if not condition:
        raise AssertionError(message)


def diagnostic(client):
    r = client.read(0x4968, 8)
    return dict(status=r[0], enabled=r[1], valid=r[2], temperature=r[3],
                count=r[4], trip=r[5], sequence=(r[6] << 16) | r[7])


def wait_for(client, predicate, seconds, name):
    deadline = time.monotonic() + seconds
    while time.monotonic() < deadline:
        d = diagnostic(client)
        client.log(event='sample', phase=name, **d)
        if predicate(d):
            return d
        time.sleep(0.15)
    raise TimeoutError(f'{name} 逾時')


def inject(client, temperature, enabled=True):
    values = [int(enabled), int(enabled), temperature & 0xFFFF, 0x5AFE]
    client.write_multiple(0x4960, values)
    return wait_for(client, lambda d: d['status'] in (2, 3), 3, '注入命令')


def applied(client, temperature, enabled=True):
    d = inject(client, temperature, enabled)
    require(d['status'] == 2 and d['enabled'] == int(enabled) and
            d['valid'] == int(enabled) and d['temperature'] == temperature & 0xFFFF,
            '注入命令未成功套用')
    return d


def safety(client):
    r = client.read(0x4810, 16)
    return dict(live=(r[2] << 16) | r[3], latch=(r[4] << 16) | r[5],
                events=r[8], snapshots=r[9], event_sequence=(r[10]<<16)|r[11],
                snapshot_sequence=(r[12]<<16)|r[13], state=r[14], inhibit=r[15])


def temperature_cycle(client):
    # Probe the test-only interface before making any test writes.
    diagnostic(client)
    require(client.read(0x4800, 1)[0] == 0, '已有 Fault，請先查明並處理，再開始測試')
    initial = safety(client)
    require(initial['state'] == 0 and initial['inhibit'] == 0 and initial['live'] == 0 and
            initial['latch'] == 0, '初始 Safety 不是正常狀態')
    touched = False
    try:
        # Set before sending: lost write ACK still requires cleanup.
        touched = True
        applied(client, 8499)
        start = diagnostic(client)['sequence']
        def below(d):
            require(not d['trip'] and d['count'] == 0, '84.99°C 意外觸發過溫')
            return ((d['sequence']-start) & 0xFFFFFFFF) >= 3
        wait_for(client, below, 7, '84.99°C 三次取樣')
        require(client.read(0x4800, 1)[0] == 0, '低溫階段出現 Fault')
        before = diagnostic(client)['sequence']
        applied(client, 8500)
        def high(d):
            if d['trip']:
                require(d['count'] >= 3 and ((d['sequence']-before)&0xFFFFFFFF) >= 3,
                        '未達三次取樣就觸發過溫')
                return True
            require(d['count'] < 3, '三次確認後未成立過溫')
            return False
        wait_for(client, high, 7, '85.00°C 三次確認')
        # SystemRoutine may publish diagnostics on the next foreground pass.
        time.sleep(0.25)
        state = safety(client)
        client.log(event='safety_trip', **state)
        require(state['live'] & 1 and state['latch'] & 1 and
                state['state'] == 1 and state['inhibit'] == 1, '過溫未傳至 Safety inhibit')
        require(state['event_sequence'] != initial['event_sequence'] and
                state['snapshot_sequence'] != initial['snapshot_sequence'], '未新增 Event／Snapshot')
        found = False
        for index in range(client.read(0x4800, 1)[0]):
            client.write_single(0x4801, index)
            fault = client.read(0x4802, 2)
            if fault[0] == 0x0201:
                require(fault[1] == 8500, '過溫 Fault detail 不符')
                found = True
                break
        require(found, '找不到 Fault 0x0201')
        try:
            client.write_multiple(0x4809, [0x0201, 0xC1EA])
        except ModbusError as exc:
            require(exc.code == 3, '帶故障 Reset 回覆錯誤異常碼')
        else:
            raise AssertionError('高溫仍成立卻接受 Reset')
        require(client.read(0x480B, 1)[0] == 3, 'Reset 結果不是條件仍成立')
        applied(client, 8499)
        wait_for(client, lambda d: d['count'] == 0 and not d['trip'], 5, '降溫')
        time.sleep(0.25)
        state = safety(client)
        require(not state['live'] & 1 and state['latch'] & 1 and state['inhibit'] == 1,
                '降溫後 live／鎖存行為不符')
        client.write_multiple(0x4809, [0x0201, 0xC1EA])
        require(client.read(0x480B, 1)[0] == 1, '降溫後 Reset 失敗')
        time.sleep(0.25)
        state = safety(client)
        require(not state['latch'] & 1, 'Reset 後 MCU 過溫鎖存未清除')
        require(state['state'] == 0 and state['inhibit'] == 0 and
                client.read(0x4800, 1)[0] == 0, 'Reset 後仍有 Fault／Safety 抑制')
        client.log(event='software_cycle_pass', physical_outputs='待量測 PWM／DO，不由 Register 推定通過')
    finally:
        if touched:
            try:
                applied(client, 0, False)
                client.log(event='cleanup', injection='disabled', fault_reset='不自動清除失敗時的 Fault')
            except Exception as exc:
                client.log(event='cleanup_failed', error=str(exc))
                raise RuntimeError('無法確認注入已關閉；請重新上電並檢查 Fault') from exc


def number(text):
    return int(text, 0)


def low_voltage_diagnostic(client):
    r = client.read(0x4974, 12)
    return dict(status=r[0], enabled=r[1], active=r[2], limit=r[3],
                processed=r[4], peak_assert=r[5], peak_release=r[6],
                confirmed=bool(r[7] & 1), ever_confirmed=bool(r[7] & 2),
                raw=bool(r[7] & 4), finished=bool(r[7] & 8),
                sequence=(r[8] << 16) | r[9], assert_samples=r[10], release_samples=r[11])


def low_voltage_wait(client, predicate, name, seconds=4):
    deadline = time.monotonic() + seconds
    while time.monotonic() < deadline:
        d = low_voltage_diagnostic(client)
        client.log(event='sample', phase=name, **d)
        if predicate(d):
            return d
        time.sleep(0.05)
    raise TimeoutError(f'{name} 逾時')


def low_voltage_apply(client, active=False, limit=0, enabled=True):
    client.write_multiple(0x4970, [int(enabled), int(active), limit, 0x5AFE])
    d = low_voltage_wait(client, lambda d: d['status'] in (2, 3), '低電壓命令')
    require(d['status'] == 2 and d['enabled'] == int(enabled) and
            d['active'] == int(active) and d['limit'] == limit, '低電壓命令未成功套用')
    return d


def low_voltage_cycle(client):
    probe = low_voltage_diagnostic(client)  # Old/production firmware stops before any write.
    require(probe['assert_samples'] == 3 and probe['release_samples'] == 100,
            '低電壓韌體參數不是預期的 3 次成立／100 次恢復')
    require(client.read(0x4800, 1)[0] == 0, '已有 Fault，請先查明並處理，再開始測試')
    initial = safety(client)
    require(initial['state'] == 0 and initial['inhibit'] == 0 and
            initial['live'] == 0 and initial['latch'] == 0, '初始 Safety 不是正常狀態')
    touched = False
    try:
        touched = True  # Includes lost ACKs in cleanup handling.
        d = low_voltage_apply(client)
        start = d['sequence']
        low_voltage_wait(client, lambda d: not d['raw'] and not d['confirmed'] and
                         ((d['sequence'] - start) & 0xFFFFFFFF) >= 3, '正常取樣')
        for samples in (1, 2):
            low_voltage_apply(client, active=True, limit=samples)
            d = low_voltage_wait(client, lambda d: d['finished'] and not d['raw'],
                                 f'低電壓 {samples} 次短脈衝')
            require(d['processed'] == samples and d['peak_assert'] == samples and
                    not d['ever_confirmed'] and not d['confirmed'], '未滿三次卻觸發低電壓')
            state = safety(client)
            require(client.read(0x4800, 1)[0] == 0 and state['live'] == 0 and
                    state['latch'] == 0 and state['inhibit'] == 0 and state['state'] == 0,
                    '短脈衝出現 Fault／Safety 抑制')
            client.log(event='low_voltage_short_pulse_pass', samples=samples, peak=d['peak_assert'])

        low_voltage_apply(client, active=True)
        d = low_voltage_wait(client, lambda d: d['confirmed'], '低電壓連續三次確認')
        require(d['peak_assert'] == 3 and d['processed'] >= 3 and d['ever_confirmed'],
                '低電壓三次確認計數不符')
        time.sleep(0.1)
        state = safety(client)
        client.log(event='low_voltage_safety_trip', **state)
        require(state['live'] & 2 and state['latch'] & 2 and
                state['state'] == 1 and state['inhibit'] == 1, '低電壓未傳至 Safety inhibit')
        require(state['event_sequence'] != initial['event_sequence'] and
                state['snapshot_sequence'] != initial['snapshot_sequence'], '未新增 Event／Snapshot')
        found = False
        for index in range(client.read(0x4800, 1)[0]):
            client.write_single(0x4801, index)
            fault = client.read(0x4802, 2)
            if fault[0] == 0x0202:
                require(fault[1] == 1, '低電壓 Fault detail 不符')
                found = True
                break
        require(found, '找不到 Fault 0x0202')
        try:
            client.write_multiple(0x4809, [0x0202, 0xC1EA])
        except ModbusError as exc:
            require(exc.code == 3, '低電壓持續時 Reset 異常碼錯誤')
        else:
            raise AssertionError('低電壓仍成立卻接受 Reset')
        require(client.read(0x480B, 1)[0] == 3, 'Reset 結果不是條件仍成立')
        low_voltage_apply(client)
        d = low_voltage_wait(client, lambda d: not d['confirmed'] and not d['raw'],
                             '連續正常取樣恢復')
        require(d['peak_release'] == 100, '恢復未經連續 100 次正常取樣')
        time.sleep(0.1)
        state = safety(client)
        require(not state['live'] & 2 and state['latch'] & 2 and state['inhibit'] == 1,
                '電壓恢復後 live／鎖存行為不符')
        client.write_multiple(0x4809, [0x0202, 0xC1EA])
        require(client.read(0x480B, 1)[0] == 1, '電壓恢復後 Reset 失敗')
        time.sleep(0.1)
        state = safety(client)
        require(state['live'] == 0 and state['latch'] == 0 and state['state'] == 0 and
                state['inhibit'] == 0 and client.read(0x4800, 1)[0] == 0,
                'Reset 後仍有 Fault／Safety 抑制')
        client.log(event='low_voltage_software_cycle_pass',
                   physical_outputs='EVK 軟體流程；實際低電壓電路及 PWM／DO 待正式板量測')
    finally:
        if touched:
            try:
                low_voltage_apply(client, enabled=False)
                client.log(event='cleanup', low_voltage_injection='disabled',
                           fault_reset='不自動清除失敗時的 Fault')
            except Exception as exc:
                client.log(event='cleanup_failed', error=str(exc))
                raise RuntimeError('無法確認低電壓注入已關閉；請重新上電並檢查 Fault') from exc


def fault_records(client):
    records = {}
    for index in range(client.read(0x4800, 1)[0]):
        client.write_single(0x4801, index)
        code, detail = client.read(0x4802, 2)
        require(code not in records, 'Fault 清單出現重複代碼')
        records[code] = detail
    return records


def wait_safety(client, live, latch, name, seconds=4):
    deadline = time.monotonic() + seconds
    while time.monotonic() < deadline:
        state = safety(client)
        client.log(event='sample', phase=name, **state)
        inhibited = int(bool(live or latch))
        if (state['live'] == live and state['latch'] == latch and
                state['inhibit'] == inhibited and state['state'] == inhibited):
            return state
        time.sleep(0.1)
    raise TimeoutError(f'{name} Safety 狀態不符／逾時')


def reset_fault(client, code, blocked=False):
    try:
        client.write_multiple(0x4809, [code, 0xC1EA])
    except ModbusError as exc:
        require(blocked and exc.code == 3, f'Fault 0x{code:04X} Reset 回覆異常')
    else:
        require(not blocked, f'Fault 0x{code:04X} 條件仍成立卻接受 Reset')
    expected = 3 if blocked else 1
    require(client.read(0x480B, 1)[0] == expected, f'Fault 0x{code:04X} Reset 結果不符')


def multi_fault_cycle(client):
    # Probe both interfaces before writes. Existing faults are never cleared to start.
    diagnostic(client)
    probe = low_voltage_diagnostic(client)
    require(probe['assert_samples'] == 3 and probe['release_samples'] == 100,
            '低電壓韌體參數不是預期的 3／100 次')
    require(not fault_records(client), '已有 Fault，請先查明並處理，再開始測試')
    initial = safety(client)
    require(initial['live'] == 0 and initial['latch'] == 0 and
            initial['state'] == 0 and initial['inhibit'] == 0, '初始 Safety 不是正常狀態')
    touched_temperature = touched_voltage = False
    codes = {1: 0x0201, 2: 0x0202}
    both_faults = {0x0201: 8500, 0x0202: 1}

    def recover(source):
        if source == 1:
            applied(client, 8499)
            wait_for(client, lambda d: d['count'] == 0 and not d['trip'], 5, '解除過溫')
        else:
            low_voltage_apply(client)
            d = low_voltage_wait(client, lambda d: not d['raw'] and not d['confirmed'],
                                 '解除低電壓')
            require(d['peak_release'] == 100, '低電壓恢復未經 100 次正常取樣')

    try:
        touched_voltage = True
        low_voltage_apply(client)
        touched_temperature = True
        applied(client, 8499)
        wait_for(client, lambda d: d['count'] == 0 and not d['trip'], 5, '組合測試初始正常溫度')
        for first in (1, 2):
            order = 'temperature-first' if first == 1 else 'low-voltage-first'
            remaining = 3 ^ first
            before = wait_safety(client, 0, 0, '本輪開始')
            require(not fault_records(client), '本輪開始仍有 Fault')
            applied(client, 8500)
            low_voltage_apply(client, active=True)
            d = wait_for(client, lambda d: bool(d['trip']), 7, '雙重故障過溫確認')
            require(d['count'] >= 3, '過溫未經三次確認')
            low_voltage_wait(client, lambda d: d['confirmed'], '雙重故障低電壓確認')
            state = wait_safety(client, 3, 3, '雙重故障成立')
            require(fault_records(client) == both_faults, '雙重 Fault 代碼／detail 不符')
            require(state['event_sequence'] != before['event_sequence'] and
                    state['snapshot_sequence'] != before['snapshot_sequence'], '未新增 Event／Snapshot')
            client.log(event='multi_fault_trip', order=order, **state)
            for code in codes.values():
                reset_fault(client, code, blocked=True)
            wait_safety(client, 3, 3, '拒絕重置後保持保護')
            require(fault_records(client) == both_faults, '拒絕 Reset 後故障被清除')

            recover(first)
            wait_safety(client, remaining, 3, '第一項恢復但尚未重置')
            require(fault_records(client) == both_faults, '第一項恢復時 Fault 提早清除')
            reset_fault(client, codes[first])
            state = wait_safety(client, remaining, remaining, '僅清除第一項故障')
            expected = {codes[remaining]: both_faults[codes[remaining]]}
            require(fault_records(client) == expected, '清除第一項時影響另一項 Fault')
            reset_fault(client, codes[remaining], blocked=True)
            wait_safety(client, remaining, remaining, '另一項持續異常時拒絕重置')
            require(fault_records(client) == expected, '另一項持續異常時 Fault 被清除')
            client.log(event='remaining_fault_inhibit_pass', order=order, **state)

            recover(remaining)
            wait_safety(client, 0, remaining, '全部恢復但最後鎖存保持')
            require(fault_records(client) == expected, '最後故障在 Reset 前已清除')
            reset_fault(client, codes[remaining])
            wait_safety(client, 0, 0, '全部重置恢復正常')
            require(not fault_records(client), '全部重置後仍有 Fault')
            client.log(event='multi_fault_order_pass', order=order)
    finally:
        errors = []
        # Attempt both independently, even if the first cleanup loses its ACK.
        for name, touched, disable in (
                ('temperature', touched_temperature, lambda: applied(client, 0, False)),
                ('low_voltage', touched_voltage, lambda: low_voltage_apply(client, enabled=False))):
            if touched:
                try:
                    disable()
                    client.log(event='cleanup', injection=name, state='disabled',
                               fault_reset='不自動清除失敗時的 Fault')
                except Exception as exc:
                    errors.append(name)
                    client.log(event='cleanup_failed', injection=name, error=str(exc))
        if errors:
            raise RuntimeError('無法確認注入已關閉：' + '、'.join(errors) + '；請重新上電並檢查 Fault')
    client.log(event='multi_fault_software_cycle_pass',
               physical_outputs='EVK 軟體互鎖；實際 PWM／DO 待正式板量測')


def reset_guard_cycle(client):
    diagnostic(client)
    probe = low_voltage_diagnostic(client)
    require(probe['assert_samples'] == 3 and probe['release_samples'] == 100,
            '低電壓韌體參數不是預期的 3／100 次')
    require(not fault_records(client), '已有 Fault，請先查明並處理，再開始測試')
    initial = safety(client)
    require(initial['live'] == 0 and initial['latch'] == 0 and
            initial['state'] == 0 and initial['inhibit'] == 0, '初始 Safety 不是正常狀態')
    touched_temperature = touched_voltage = False

    def reject(code, key, result, phase):
        before = safety(client)
        faults = fault_records(client)
        try:
            client.write_multiple(0x4809, [code, key])
        except ModbusError as exc:
            require(exc.code == 3, f'{phase} 回覆的異常碼不符')
        else:
            raise AssertionError(f'{phase} 卻接受 Reset')
        require(client.read(0x480B, 1)[0] == result, f'{phase} Reset result 不符')
        after = safety(client)
        for field in ('live', 'latch', 'state', 'inhibit', 'event_sequence', 'snapshot_sequence'):
            require(after[field] == before[field], f'{phase} 改變了 Safety／Event／Snapshot：{field}')
        require(fault_records(client) == faults, f'{phase} 改變了 Fault 清單')
        client.log(event='reset_guard_rejection_pass', phase=phase, code=f'0x{code:04X}',
                   key=f'0x{key:04X}', result=result)

    try:
        touched_voltage = True
        low_voltage_apply(client)
        touched_temperature = True
        applied(client, 8499)
        wait_for(client, lambda d: not d['trip'] and d['count'] == 0, 5, '重置防護初始正常溫度')
        for source, code, detail in ((1, 0x0201, 8500), (2, 0x0202, 1)):
            wait_safety(client, 0, 0, '防護案例開始')
            require(not fault_records(client), '防護案例開始仍有 Fault')
            if source == 1:
                applied(client, 8500)
                d = wait_for(client, lambda d: bool(d['trip']), 7, '重置防護過溫成立')
                require(d['count'] >= 3, '過溫未經三次確認')
            else:
                low_voltage_apply(client, active=True)
                low_voltage_wait(client, lambda d: d['confirmed'], '重置防護低電壓成立')
            wait_safety(client, source, source, '重置防護故障成立')
            require(fault_records(client) == {code: detail}, '防護案例 Fault／detail 不符')
            for key in (0, 0xFFFF, 0xEAC1):
                reject(code, key, 6, '條件成立時錯誤確認碼')
            for invalid in (0, 0xFFFF):
                reject(invalid, 0xC1EA, 4, '無效故障代碼')
            other = 0x0202 if source == 1 else 0x0201
            reject(other, 0xC1EA, 2, '重置未成立的另一項故障')
            reject(code, 0xC1EA, 3, '條件仍成立時正確確認碼')

            if source == 1:
                applied(client, 8499)
                wait_for(client, lambda d: not d['trip'] and d['count'] == 0, 5, '防護案例降溫')
            else:
                low_voltage_apply(client)
                d = low_voltage_wait(client, lambda d: not d['raw'] and not d['confirmed'],
                                     '防護案例電壓恢復')
                require(d['peak_release'] == 100, '電壓恢復未經 100 次正常取樣')
            wait_safety(client, 0, source, '條件恢復但鎖存保持')
            require(fault_records(client) == {code: detail}, '條件恢復時 Fault 被提早清除')
            for key in (0, 0xFFFF, 0xEAC1):
                reject(code, key, 6, '鎖存期間錯誤確認碼')
            reset_fault(client, code)
            wait_safety(client, 0, 0, '正確重置後恢復正常')
            require(not fault_records(client), '正確重置後仍有 Fault')
            reject(code, 0xC1EA, 2, '重複重置已清除故障')
            client.log(event='reset_guard_source_pass', code=f'0x{code:04X}')
    finally:
        errors = []
        for name, touched, disable in (
                ('temperature', touched_temperature, lambda: applied(client, 0, False)),
                ('low_voltage', touched_voltage, lambda: low_voltage_apply(client, enabled=False))):
            if touched:
                try:
                    disable()
                    client.log(event='cleanup', injection=name, state='disabled',
                               fault_reset='不自動清除失敗時的 Fault')
                except Exception as exc:
                    errors.append(name)
                    client.log(event='cleanup_failed', injection=name, error=str(exc))
        if errors:
            raise RuntimeError('無法確認注入已關閉：' + '、'.join(errors) + '；請重新上電並檢查 Fault')
    client.log(event='reset_guard_software_cycle_pass',
               physical_outputs='EVK 重置命令防護；實際 PWM／DO 待正式板量測')


def sequence_distance(before, after):
    distance = (after - before) & 0xFFFFFFFF
    # Firmware wraps UINT32_MAX to 1, skipping zero.
    if before and after and after < before:
        distance -= 1
    return distance


def verify_retained_history(client, state):
    for name, base, count, latest, capacity, words in (
            ('event', 0x4920, state['events'], state['event_sequence'], 32, 12),
            ('snapshot', 0x4940, state['snapshots'], state['snapshot_sequence'], 16, 27)):
        require(0 < count <= capacity and latest != 0, f'{name} 保留筆數／最新序號不符')
        actual = []
        for index in range(count):
            client.write_single(base, index)
            record = client.read(base, words)
            require(record[0] == index, f'{name} 索引讀回不符')
            sequence = (record[1] << 16) | record[2]
            actual.append(sequence)
            require(1 <= record[5] <= (4 if name == 'event' else 5), f'{name} 紀錄類別無效')
            if name == 'event':
                require(1 <= record[6] <= 4, 'Event 紀錄狀態無效')
            client.log(event='retained_record', kind=name, index=index, registers=record)
        expected = [((latest - count + index) % 0xFFFFFFFF) + 1 for index in range(count)]
        require(actual == expected, f'{name} 環形紀錄順序／覆寫結果不符')
    after = safety(client)
    require(all(after[k] == state[k] for k in state), '讀取環形紀錄期間系統狀態改變')


def repeat_cycle(client, cycles=3):
    require(2 <= cycles <= 100, '連續測試輪數必須為 2..100')
    diagnostic(client)
    low_voltage_diagnostic(client)
    require(not fault_records(client), '已有 Fault，請先查明並處理，再開始測試')
    initial = safety(client)
    require(initial['live'] == 0 and initial['latch'] == 0 and
            initial['state'] == 0 and initial['inhibit'] == 0, '初始 Safety 不是正常狀態')
    previous = initial
    for round_number in range(1, cycles + 1):
        for name, run in (('temperature', temperature_cycle), ('low_voltage', low_voltage_cycle),
                          ('multi_fault', multi_fault_cycle), ('reset_guard', reset_guard_cycle)):
            client.log(event='repeat_case_start', round=round_number, total=cycles, case=name)
            run(client)  # Each case owns its injection cleanup, including failure/Ctrl+C.
            wait_safety(client, 0, 0, '連續測試案例結束')
            require(not fault_records(client), f'第 {round_number} 輪 {name} 結束仍有 Fault')
            require(diagnostic(client)['enabled'] == 0 and low_voltage_diagnostic(client)['enabled'] == 0,
                    f'第 {round_number} 輪 {name} 結束仍有注入')
            client.log(event='repeat_case_pass', round=round_number, case=name)
        state = wait_safety(client, 0, 0, '本輪結束')
        for count, sequence, capacity in (('events', 'event_sequence', 32),
                                          ('snapshots', 'snapshot_sequence', 16)):
            added = sequence_distance(previous[sequence], state[sequence])
            require(0 < added < 0x80000000, f'{count} 序號未前進或疑似重啟／清除')
            require(state[count] == min(capacity, previous[count] + added),
                    f'{count} 保留筆數不符／可能重啟或遺失紀錄')
        verify_retained_history(client, state)
        client.log(event='repeat_round_pass', round=round_number, total=cycles, **state)
        previous = state
    require(sequence_distance(initial['event_sequence'], previous['event_sequence']) > 32 and
            sequence_distance(initial['snapshot_sequence'], previous['snapshot_sequence']) > 16,
            '本次新增紀錄不足以驗證兩個環形緩衝區覆寫')
    require(previous['events'] == 32 and previous['snapshots'] == 16,
            '紀錄達容量後保留筆數不符')
    client.log(event='repeat_software_cycle_pass', rounds=cycles, cases=cycles * 4,
               physical_outputs='EVK 連續軟體測試及紀錄覆寫；實際 PWM／DO 待正式板量測')


def sample_health(client):
    r = client.read(0x4964, 4)
    return dict(valid=bool(r[0]), temperature=r[1], sequence=(r[2] << 16) | r[3])


def invalid_sample_cycle(client):
    sample_health(client)  # Added read-only validity field; old firmware stops here.
    diagnostic(client)
    require(not fault_records(client), '已有 Fault，請先查明並處理，再開始測試')
    initial = safety(client)
    require(initial['live'] == 0 and initial['latch'] == 0 and
            initial['state'] == 0 and initial['inhibit'] == 0, '初始 Safety 不是正常狀態')
    touched = False

    def valid_below():
        applied(client, 8499)
        def ready(d):
            actual = sample_health(client)
            return (actual['valid'] and actual['temperature'] == 8499 and
                    not d['trip'] and d['count'] == 0)
        wait_for(client, ready, 5, '有效低溫樣本')

    def hold_invalid(temperature, count, trip, live, latch, faults):
        baseline = wait_safety(client, live, latch, '無效樣本前的保護狀態')
        client.write_multiple(0x4960, [1, 0, temperature, 0x5AFE])
        d = wait_for(client, lambda d: d['status'] in (2, 3), 3, '無效樣本命令')
        require(d['status'] == 2 and d['enabled'] == 1 and d['valid'] == 0 and
                d['temperature'] == temperature, '無效樣本命令未成功套用')
        wait_for(client, lambda d: not sample_health(client)['valid'], 5, 'ADC 實際 valid 轉為無效')
        actual = sample_health(client)
        deadline = time.monotonic() + 3.2
        while time.monotonic() < deadline:
            d = diagnostic(client)
            current = sample_health(client)
            require(not current['valid'] and current['sequence'] == actual['sequence'] and
                    current['temperature'] == actual['temperature'], '無效樣本卻更新有效量測值／序號')
            require(d['count'] == count and bool(d['trip']) == trip,
                    '無效樣本改變了過溫計數／成立狀態')
            state = safety(client)
            require(all(state[k] == baseline[k] for k in
                        ('live', 'latch', 'state', 'inhibit', 'event_sequence', 'snapshot_sequence')),
                    '無效樣本誤改變 Safety／Event／Snapshot')
            require(fault_records(client) == faults, '無效樣本誤改變 Fault')
            client.log(event='sample', phase='無效樣本保持', requested_temperature=temperature,
                       actual_valid=current['valid'], count=d['count'], trip=d['trip'])
            time.sleep(0.15)
        client.log(event='invalid_sample_hold_pass', requested_temperature=temperature,
                   minimum_hold_seconds=3.2, actual_valid=False, count=count, trip=trip)

    try:
        touched = True
        valid_below()
        hold_invalid(8500, 0, False, 0, 0, {})
        applied(client, 8500)
        d = wait_for(client, lambda d: bool(d['trip']), 7, '有效高溫三次確認')
        require(d['count'] == 3 and sample_health(client)['valid'], '有效高溫未經三次確認')
        wait_safety(client, 1, 1, '有效高溫保護成立')
        require(fault_records(client) == {0x0201: 8500}, '過溫 Fault／detail 不符')
        hold_invalid(8499, 3, True, 1, 1, {0x0201: 8500})
        reset_fault(client, 0x0201, blocked=True)
        wait_safety(client, 1, 1, '無效低溫不能解除既有保護')
        valid_below()
        wait_safety(client, 0, 1, '有效低溫恢復但鎖存保持')
        reset_fault(client, 0x0201)
        wait_safety(client, 0, 0, '有效低溫後重置恢復正常')
        require(not fault_records(client), '有效恢復並重置後仍有 Fault')
    finally:
        if touched:
            try:
                applied(client, 0, False)
                client.log(event='cleanup', injection='temperature', state='disabled',
                           fault_reset='不自動清除失敗時的 Fault')
            except Exception as exc:
                client.log(event='cleanup_failed', error=str(exc))
                raise RuntimeError('無法確認注入已關閉；請重新上電並檢查 Fault') from exc
    client.log(event='invalid_sample_software_cycle_pass',
               physical_outputs='EVK 無效樣本保護；實際 PWM／DO 待正式板量測')


def injection_guard_cycle(client):
    sample_health(client)
    diagnostic(client)
    low_voltage_diagnostic(client)
    require(not fault_records(client), '已有 Fault，請先查明並處理，再開始測試')
    initial = safety(client)
    require(initial['live'] == 0 and initial['latch'] == 0 and
            initial['state'] == 0 and initial['inhibit'] == 0, '初始 Safety 不是正常狀態')
    touched_temperature = touched_voltage = False
    passed = 0
    try:
        touched_temperature = True
        applied(client, 0, False)
        touched_voltage = True
        low_voltage_apply(client, enabled=False)
        baseline = wait_safety(client, 0, 0, '命令防護基準')
        temperature = diagnostic(client)
        voltage = low_voltage_diagnostic(client)
        cases = []
        for name, address, readonly, value in (
                ('temperature', 0x4960, 0x4968, 8500),
                ('low_voltage', 0x4970, 0x4974, 0)):
            for key in (0, 0xFFFF, 0xFE5A):
                cases.append((name + ':wrong-key', 16, address, [1, 1, value, key], 3))
            cases.extend([
                (name + ':invalid-enable', 16, address, [2, 1, value, 0x5AFE], 3),
                (name + ':invalid-flag', 16, address, [1, 2, value, 0x5AFE], 3),
                (name + ':partial-one-word', 16, address, [1], 2),
                (name + ':partial-three-words', 16, address, [1, 1, value], 2),
                (name + ':extra-word', 16, address, [1, 1, value, 0x5AFE, 0], 2),
                (name + ':fc06-enable', 6, address, [1], 2),
                (name + ':fc06-key', 6, address + 3, [0x5AFE], 2),
                (name + ':readonly-write', 16, readonly, [1, 1, value, 0x5AFE], 2),
                (name + ':misaligned-write', 16, address + 3, [1, 1, value, 0x5AFE], 2),
            ])
        cases.extend([
            ('actual-adc:readonly-fc06', 6, 0x4964, [1], 2),
            ('actual-adc:readonly-fc10', 16, 0x4964, [1, 1, 8500, 0x5AFE], 2),
        ])
        for name, function, address, values, exception in cases:
            try:
                if function == 6:
                    client.write_single(address, values[0])
                else:
                    client.write_multiple(address, values)
            except ModbusError as exc:
                require(exc.code == exception, f'{name} 異常碼不符')
            else:
                raise AssertionError(f'{name} 誤接受了非法注入命令')
            current_t = diagnostic(client)
            current_v = low_voltage_diagnostic(client)
            require(all(current_t[k] == temperature[k] for k in
                        ('status', 'enabled', 'valid', 'temperature', 'count', 'trip')),
                    f'{name} 雖拒絕卻改變過溫注入／計數')
            require(all(current_v[k] == voltage[k] for k in ('status', 'enabled', 'active', 'limit')),
                    f'{name} 雖拒絕卻改變低電壓注入')
            state = safety(client)
            require(all(state[k] == baseline[k] for k in
                        ('live', 'latch', 'state', 'inhibit', 'event_sequence', 'snapshot_sequence')) and
                    not fault_records(client), f'{name} 改變了 Fault／Safety／Event／Snapshot')
            passed += 1
            client.log(event='injection_guard_rejection_pass', case=name, function=function,
                       address=f'0x{address:04X}', exception=exception)
    finally:
        errors = []
        for name, touched, disable in (
                ('temperature', touched_temperature, lambda: applied(client, 0, False)),
                ('low_voltage', touched_voltage, lambda: low_voltage_apply(client, enabled=False))):
            if touched:
                try:
                    disable()
                    client.log(event='cleanup', injection=name, state='disabled',
                               fault_reset='不自動清除失敗時的 Fault')
                except Exception as exc:
                    errors.append(name)
                    client.log(event='cleanup_failed', injection=name, error=str(exc))
        if errors:
            raise RuntimeError('無法確認注入已關閉：' + '、'.join(errors) + '；請重新上電並檢查 Fault')
    client.log(event='injection_guard_software_cycle_pass', rejected_cases=passed,
               physical_outputs='EVK 注入命令防護；實際 PWM／DO 待正式板量測')


def broadcast_guard_cycle(client):
    sample_health(client)
    diagnostic(client)
    low_voltage_diagnostic(client)
    require(not fault_records(client), '已有 Fault，請先查明並處理，再開始測試')
    initial = safety(client)
    require(initial['live'] == 0 and initial['latch'] == 0 and
            initial['state'] == 0 and initial['inhibit'] == 0, '初始 Safety 不是正常狀態')
    touched_temperature = touched_voltage = False
    try:
        touched_temperature = True
        applied(client, 0, False)
        touched_voltage = True
        low_voltage_apply(client, enabled=False)
        baseline = wait_safety(client, 0, 0, '廣播防護基準')
        temperature, voltage = diagnostic(client), low_voltage_diagnostic(client)
        cases = (
            ('temperature-fc10', 16, 0x4960, [1, 1, 8500, 0x5AFE]),
            ('low-voltage-fc10', 16, 0x4970, [1, 1, 0, 0x5AFE]),
            ('temperature-enable-fc06', 6, 0x4960, [1]),
            ('temperature-key-fc06', 6, 0x4963, [0x5AFE]),
            ('low-voltage-enable-fc06', 6, 0x4970, [1]),
            ('low-voltage-key-fc06', 6, 0x4973, [0x5AFE]),
        )
        for name, function, address, values in cases:
            client.broadcast_test_command(address, values, function)
            current_t, current_v = diagnostic(client), low_voltage_diagnostic(client)
            require(all(current_t[k] == temperature[k] for k in
                        ('status', 'enabled', 'valid', 'temperature', 'count', 'trip')),
                    f'{name} 廣播誤改變過溫注入／計數')
            require(all(current_v[k] == voltage[k] for k in ('status', 'enabled', 'active', 'limit')),
                    f'{name} 廣播誤改變低電壓注入')
            state = safety(client)
            require(all(state[k] == baseline[k] for k in
                        ('live', 'latch', 'state', 'inhibit', 'event_sequence', 'snapshot_sequence')) and
                    not fault_records(client), f'{name} 廣播改變 Fault／Safety／Event／Snapshot')
            client.log(event='broadcast_guard_case_pass', case=name, function=function,
                       broadcast_station=0, readback_station=client.station)
    finally:
        errors = []
        for name, touched, disable in (
                ('temperature', touched_temperature, lambda: applied(client, 0, False)),
                ('low_voltage', touched_voltage, lambda: low_voltage_apply(client, enabled=False))):
            if touched:
                try:
                    disable()
                    client.log(event='cleanup', injection=name, state='disabled',
                               fault_reset='不自動清除失敗時的 Fault')
                except Exception as exc:
                    errors.append(name)
                    client.log(event='cleanup_failed', injection=name, error=str(exc))
        if errors:
            raise RuntimeError('無法確認注入已關閉：' + '、'.join(errors) + '；請重新上電並檢查 Fault')
    client.log(event='broadcast_guard_software_cycle_pass', cases=6,
               physical_outputs='EVK 廣播注入防護；實際 PWM／DO 待正式板量測')


def pwm_diagnostic(client):
    r = client.read(0x4980, 32)
    return [dict(requested=r[i], applied=r[i+1], inhibited=r[i+2],
                 period=(r[i+3] << 16) | r[i+4], mode=r[i+5],
                 hal_initialized=r[i+6], evk=r[i+7]) for i in range(0, 32, 8)]


def set_pwm(client, periods, duties, modes):
    values = [v for triple in zip(periods, duties, modes) for v in triple]
    client.write_multiple(0x1300, values + [0xA5A5])
    active = client.read(0x1300, 15)
    require(active[:12] == values and active[14] == 0, 'PWM Pending／Apply 未成功套用')


def wait_pwm(client, requested, applied_values, inhibited, name, periods=None, modes=None):
    deadline = time.monotonic() + 4
    while time.monotonic() < deadline:
        states = pwm_diagnostic(client)
        require(all(s['evk'] == 1 and s['hal_initialized'] == 1 for s in states),
                'PWM 測試只允許已初始化的 SafetyTestEVK')
        client.log(event='sample', phase=name, pwm=states)
        if (all(s['requested'] == requested[i] and s['applied'] == applied_values[i] and
                (inhibited is None or s['inhibited'] == int(inhibited)) and
                (periods is None or s['period'] == periods[i]) and
                (modes is None or s['mode'] == modes[i]) for i, s in enumerate(states))):
            return states
        time.sleep(0.1)
    raise TimeoutError(f'{name} PWM 要求值／套用值／抑制狀態不符')


def pwm_inhibit_cycle(client, *, multi=False):
    states = pwm_diagnostic(client)  # Old/production firmware: stop before writes.
    require(all(s['evk'] == 1 and s['hal_initialized'] == 1 for s in states),
            '本測試會輸出非零 PWM，只允許 SafetyTestEVK')
    original = client.read(0x1300, 15)
    require(original[14] == 0, '已有未套用 PWM Pending，請先處理')
    require(all(original[i+1] == 0 for i in range(0, 12, 3)) and
            all(s['requested'] == 0 and s['applied'] == 0 and s['inhibited'] == 0 for s in states),
            'PWM 初始必須為零且四通道均可運作')
    require(not fault_records(client), '已有 Fault，請先查明並處理，再開始測試')
    initial = safety(client)
    require(initial['live'] == 0 and initial['latch'] == 0 and initial['state'] == 0 and
            initial['inhibit'] == 0, '初始 Safety 不是正常狀態')
    periods = original[:12:3]
    original_modes = original[2:12:3]
    require(all(10 <= p <= 10000 for p in periods) and all(m in (0, 1) for m in original_modes),
            '原始 PWM 設定不合法')
    zeros, immediate = [0] * 4, [0] * 4
    touched_pwm = touched_temperature = touched_voltage = False
    try:
        touched_temperature = True
        applied(client, 8499)
        wait_for(client, lambda d: d['count'] == 0 and not d['trip'], 5, 'PWM 測試正常溫度')
        touched_voltage = True
        low_voltage_apply(client)
        if multi:
            touched_pwm = True
            pwm_multi_fault_rounds(client, periods)
        for source, code, detail in (() if multi else ((1, 0x0201, 8500), (2, 0x0202, 1))):
            duties = [100, 200, 300, 400]
            touched_pwm = True  # A lost Apply ACK still requires zeroing outputs.
            set_pwm(client, periods, duties, immediate)
            normal = wait_pwm(client, duties, duties, False, '四路非零 PWM', periods, immediate)
            client.log(event='pwm_nonzero_pass', code=f'0x{code:04X}', pwm=normal)
            if source == 1:
                applied(client, 8500)
                d = wait_for(client, lambda d: bool(d['trip']), 7, 'PWM 測試過溫成立')
                require(d['count'] == 3, '過溫未經三次確認')
            else:
                low_voltage_apply(client, active=True)
                low_voltage_wait(client, lambda d: d['confirmed'], 'PWM 測試低電壓成立')
            wait_safety(client, source, source, 'PWM 安全來源成立')
            off = wait_pwm(client, duties, zeros, True, '故障後四路套用值歸零')
            require(fault_records(client) == {code: detail}, 'PWM 測試 Fault／detail 不符')
            client.log(event='pwm_inhibited_pass', code=f'0x{code:04X}', pwm=off)
            duties = [200, 300, 400, 500]
            set_pwm(client, periods, duties, immediate)
            off = wait_pwm(client, duties, zeros, True, '保護期間改寫要求值仍歸零')
            client.log(event='pwm_write_while_inhibited_pass', code=f'0x{code:04X}', pwm=off)
            if source == 1:
                applied(client, 8499)
                wait_for(client, lambda d: not d['trip'] and d['count'] == 0, 5, 'PWM 測試降溫')
            else:
                low_voltage_apply(client)
                d = low_voltage_wait(client, lambda d: not d['confirmed'] and not d['raw'],
                                     'PWM 測試低電壓恢復')
                require(d['peak_release'] == 100, '低電壓恢復未經 100 次正常取樣')
            wait_safety(client, 0, source, '故障恢復但 PWM 鎖存保持')
            wait_pwm(client, duties, zeros, True, 'Reset 前 PWM 必須仍為零')
            reset_fault(client, code)
            wait_safety(client, 0, 0, 'Reset 後解除 PWM 保護')
            restored = wait_pwm(client, duties, duties, False, 'Reset 後恢復最後要求值')
            require(not fault_records(client), 'PWM 重置後仍有 Fault')
            client.log(event='pwm_inhibit_source_pass', code=f'0x{code:04X}', pwm=restored)
            set_pwm(client, periods, zeros, immediate)
            wait_pwm(client, zeros, zeros, False, '本輪 PWM 歸零')
    finally:
        errors = []
        if touched_pwm:
            try:
                # Zero immediately before disabling injections or restoring update modes.
                set_pwm(client, periods, zeros, immediate)
                wait_pwm(client, zeros, zeros, None, 'PWM 清理歸零')
                set_pwm(client, periods, zeros, original_modes)
                wait_pwm(client, zeros, zeros, None, 'PWM 清理還原週期及模式', periods, original_modes)
                client.log(event='cleanup', pwm='zero', configuration='原週期／模式已還原，Duty 保持零')
            except Exception as exc:
                errors.append('PWM')
                client.log(event='cleanup_failed', pwm='無法確認歸零／還原', error=str(exc))
        for name, touched, disable in (
                ('temperature', touched_temperature, lambda: applied(client, 0, False)),
                ('low_voltage', touched_voltage, lambda: low_voltage_apply(client, enabled=False))):
            if touched:
                try:
                    disable()
                    client.log(event='cleanup', injection=name, state='disabled',
                               fault_reset='不自動清除失敗時的 Fault')
                except Exception as exc:
                    errors.append(name)
                    client.log(event='cleanup_failed', injection=name, error=str(exc))
        if errors:
            raise RuntimeError('無法確認安全清理：' + '、'.join(errors) + '；請停止測試並檢查輸出')
    client.log(event='pwm_multi_fault_software_cycle_pass' if multi else 'pwm_inhibit_software_cycle_pass',
               channels=4, sources=2,
               physical_outputs='套用值表示服務已獲 HAL 成功回覆；實際 PWM 波形仍待儀器量測')



def pwm_multi_fault_rounds(client, periods):
    zeros, immediate = [0] * 4, [0] * 4
    codes = {1: 0x0201, 2: 0x0202}
    both = {0x0201: 8500, 0x0202: 1}

    def recover(source):
        if source == 1:
            applied(client, 8499)
            wait_for(client, lambda d: not d['trip'] and d['count'] == 0, 5, '雙故障 PWM 降溫')
        else:
            low_voltage_apply(client)
            d = low_voltage_wait(client, lambda d: not d['confirmed'] and not d['raw'],
                                 '雙故障 PWM 電壓恢復')
            require(d['peak_release'] == 100, '低電壓恢復未經 100 次正常取樣')

    for first in (1, 2):
        order = 'temperature-first' if first == 1 else 'low-voltage-first'
        remaining = 3 ^ first
        wait_safety(client, 0, 0, '雙故障 PWM 本輪初始狀態')
        require(not fault_records(client), '雙故障 PWM 本輪開始仍有 Fault')
        duties = [100, 200, 300, 400]
        set_pwm(client, periods, duties, immediate)
        wait_pwm(client, duties, duties, False, '雙故障前四路非零 PWM')
        applied(client, 8500)
        low_voltage_apply(client, active=True)
        d = wait_for(client, lambda d: bool(d['trip']), 7, '雙故障 PWM 過溫成立')
        require(d['count'] == 3, '過溫未經三次確認')
        low_voltage_wait(client, lambda d: d['confirmed'], '雙故障 PWM 低電壓成立')
        wait_safety(client, 3, 3, '雙故障 PWM 兩項成立')
        require(fault_records(client) == both, '雙故障 PWM Fault／detail 不符')
        off = wait_pwm(client, duties, zeros, True, '雙故障四路歸零')
        client.log(event='pwm_multi_fault_trip_pass', order=order, pwm=off)
        for code in codes.values():
            reset_fault(client, code, blocked=True)
        wait_safety(client, 3, 3, '雙故障拒絕 Reset 後')
        wait_pwm(client, duties, zeros, True, '雙故障拒絕 Reset 後保持歸零')

        recover(first)
        wait_safety(client, remaining, 3, '第一項恢復但鎖存仍保持')
        wait_pwm(client, duties, zeros, True, '第一項恢復時仍歸零')
        reset_fault(client, codes[first])
        wait_safety(client, remaining, remaining, '第一項重置後另一項仍成立')
        require(fault_records(client) == {codes[remaining]: both[codes[remaining]]},
                '第一項重置影響剩餘 Fault')
        off = wait_pwm(client, duties, zeros, True, '第一項重置不得恢復四路 PWM')
        client.log(event='pwm_remaining_fault_pass', order=order, pwm=off)
        duties = [200, 300, 400, 500]
        set_pwm(client, periods, duties, immediate)
        wait_pwm(client, duties, zeros, True, '剩餘故障時改寫仍歸零')
        reset_fault(client, codes[remaining], blocked=True)
        wait_safety(client, remaining, remaining, '剩餘故障拒絕 Reset 後')
        wait_pwm(client, duties, zeros, True, '剩餘故障拒絕 Reset 後仍歸零')

        recover(remaining)
        wait_safety(client, 0, remaining, '全部恢復但最後鎖存保持')
        wait_pwm(client, duties, zeros, True, '最後鎖存不得恢復 PWM')
        reset_fault(client, codes[remaining])
        wait_safety(client, 0, 0, '最後重置恢復正常')
        restored = wait_pwm(client, duties, duties, False, '最後重置恢復最新要求值')
        require(not fault_records(client), '雙故障 PWM 重置後仍有 Fault')
        client.log(event='pwm_multi_fault_order_pass', order=order, pwm=restored)
        set_pwm(client, periods, zeros, immediate)
        wait_pwm(client, zeros, zeros, False, '雙故障 PWM 本輪結束歸零')



def production_guard_cycle(client):
    # Only zero/disable writes: never request an injection, output or Fault Reset.
    baseline = (safety(client), client.read(0x4800, 1)[0])
    rejected = 0

    def reject(case, function, address, operation):
        nonlocal rejected
        try:
            operation()
        except ModbusError as exc:
            require(exc.code == 2, f'{case} 預期 Illegal Data Address(2)，收到 {exc.code}')
        else:
            raise AssertionError(f'{case} 測試入口仍可存取；請確認下載的是一般 Debug／Release')
        rejected += 1
        client.log(event='production_guard_rejection_pass', case=case,
                   function=function, address=f'0x{address:04X}', exception=2)

    # Read every address first. SafetyTest stops here before any test-region write.
    for address in range(0x4960, 0x49A0):
        reject('test-region-read', 3, address, lambda a=address: client.read(a, 1))
    require((safety(client), client.read(0x4800, 1)[0]) == baseline,
            '讀取檢查期間 Fault／Safety／事件紀錄變化，無法判定測試狀態不變')
    for address in (0x4960, 0x4963, 0x4970, 0x4973):
        reject('test-command-fc06', 6, address, lambda a=address: client.write_single(a, 0))
    for address, values in ((0x4960, [0, 1, 8499, 0x5AFE]),
                            (0x4970, [0, 0, 0, 0x5AFE]),
                            (0x4964, [0] * 4), (0x4974, [0] * 12), (0x4980, [0] * 32)):
        reject('test-region-fc10', 16, address,
               lambda a=address, v=values: client.write_multiple(a, v))
    require((safety(client), client.read(0x4800, 1)[0]) == baseline,
            '寫入檢查期間 Fault／Safety／事件紀錄變化，無法判定測試狀態不變')
    client.log(event='production_guard_cycle_pass', rejected_cases=rejected,
               scope='本次韌體及通訊路徑的測試 Register 封鎖；不代表 ELF 中不存在測試符號')



def check_production_axf(path, log):
    path = pathlib.Path(path)
    data = path.read_bytes()
    require(len(data) >= 52 and data[:7] == b'\x7fELF\x01\x01\x01',
            '必須提供 ELF32 little-endian AXF，不能使用 BIN／MAP')
    header = struct.unpack_from('<HHIIIIIHHHHHH', data, 16)
    kind, machine, version, entry, _, section_offset = header[:6]
    require(kind == 2 and machine == 40 and version == 1, '不是 ARM 可執行 AXF')
    require(0x8000 <= (entry & ~1) < 0x22000, 'AXF 入口不在目前 APP 分區，請勿選 Bootloader')
    section_size, section_count = header[10:12]
    require(section_size == 40 and section_count > 0 and section_offset >= 52 and
            section_offset + section_size * section_count <= len(data), 'ELF section table 不完整')
    sections = [struct.unpack_from('<10I', data, section_offset + i * 40)
                for i in range(section_count)]

    def section_bytes(section):
        offset, size = section[4:6]
        require(offset <= len(data) and size <= len(data) - offset, 'ELF section 內容超出檔案')
        return data[offset:offset + size]

    names = []
    for section in sections:
        if section[1] != 2:  # SHT_SYMTAB: dynamic-only/stripped images are insufficient.
            continue
        require(section[9] == 16 and section[5] % 16 == 0 and section[6] < len(sections),
                'ELF symbol table 格式錯誤')
        strings_section = sections[section[6]]
        require(strings_section[1] == 3, 'ELF symbol string table 格式錯誤')
        strings = section_bytes(strings_section)
        symbols = section_bytes(section)
        for offset in range(0, len(symbols), 16):
            name_offset, _, _, info, _, index = struct.unpack_from('<IIIBBH', symbols, offset)
            require(name_offset < len(strings), 'ELF symbol name 超出範圍')
            end = strings.find(b'\0', name_offset)
            require(end >= 0, 'ELF symbol name 未結束')
            if index and name_offset and info & 15 != 4:  # Skip undefined and source-file names.
                names.append(strings[name_offset:end].decode('utf-8', errors='replace'))
    require(names and any(n in ('ResetISR', 'Reset_Handler') for n in names),
            '缺少完整符號表／Reset 符號，不能將 stripped AXF 判定通過')
    forbidden = sorted({n for n in names if 'ProductSafetyTest_' in n or
                        'ProductInternalAdcDriver_SetMcuTemperatureTestOverride' in n})
    log(event='axf_inspection', path=str(path.resolve()), bytes=len(data),
        sha256=hashlib.sha256(data).hexdigest(), entry=f'0x{entry:08X}',
        defined_symbols=len(names), forbidden_symbols=forbidden)
    require(not forbidden, 'AXF 仍含測試符號：' + '、'.join(forbidden))
    log(event='production_symbol_check_pass',
        scope='已定義具名測試符號未出現；LTO 內嵌及匿名狀態須搭配建置巨集與 Register 封鎖驗證')


def production_symbol_check(args):
    args.log_dir.mkdir(parents=True, exist_ok=True)
    path = args.log_dir / (datetime.datetime.now().strftime('%Y%m%d_%H%M%S_%f') + '.jsonl')
    result = 0
    with path.open('x', encoding='utf-8') as file:
        def log(**entry):
            entry['time'] = datetime.datetime.now().astimezone().isoformat()
            file.write(json.dumps(entry, ensure_ascii=False) + '\n')
            file.flush()
            print(json.dumps(entry, ensure_ascii=False))
        log(event='start', action=args.action, axf=str(args.axf))
        try:
            check_production_axf(args.axf, log)
            log(event='complete', result='PASS')
            print('通過：AXF 未發現指定的已定義測試符號。')
        except (Exception, KeyboardInterrupt) as exc:
            log(event='complete', result='FAIL', error=str(exc))
            print('測試未通過，請查看紀錄。')
            result = 1
    print(f'紀錄：{path}')
    return result



def updater_cycle(open_client, initial_port, updater_port, app_port, log, reconnect_seconds=30):
    def clean_app(client):
        require(not fault_records(client), '已有 Fault，不能開始模式切換測試')
        state = safety(client)
        require(not any(state[k] for k in ('live', 'latch', 'state', 'inhibit')),
                'Safety 不是正常狀態')
        require(not diagnostic(client)['enabled'] and not low_voltage_diagnostic(client)['enabled'],
                '請先關閉兩項測試注入')
        pwm = pwm_diagnostic(client)
        require(all(s['evk'] == 1 and s['hal_initialized'] == 1 and
                    s['requested'] == 0 and s['applied'] == 0 and s['inhibited'] == 0 for s in pwm),
                '切換測試僅允許四路 PWM 已歸零的 SafetyTestEVK')

    with open_client(initial_port) as client:
        original = client.read(0x7000, 10)
        require(original[:3] == [0xA016, 1, 1], '目前不是預期的 LPC55S16 APP 模式')
        version = client.read(0x2000, 3)
        clean_app(client)
        require(client.read(0x7051, 3)[0] == 0, 'APP 更新狀態不是 Idle')
        log(event='updater_preflight_pass', identity=original, app_version=version)
        client.write_single(0x7050, 0xB007)  # USB request only; never retry a mode write.
        log(event='updater_request_ack', port=initial_port)

    def reconnect(port, expected, operation):
        deadline = time.monotonic() + reconnect_seconds
        last_error = '尚未收到識別資料'
        while time.monotonic() < deadline:
            # Only open/read failures are retried. The operation (possibly a write) is not.
            found = False
            try:
                with open_client(port) as client:
                    try:
                        identity = client.read(0x7000, 10)
                    except (OSError, TimeoutError, ModbusError) as exc:
                        last_error = str(exc)
                    else:
                        if identity[:3] == expected:
                            found = True
                            return operation(client, identity)
                        last_error = f'仍在其他模式：{identity[:3]}'
                        if identity[:3] == [0xA016, 1, 1] and expected[0] == 0xB016:
                            require(client.read(0x7051, 3)[0] != 4,
                                    'APP 安全停機失敗，更新狀態 FAILED，未切入 Updater')
            except OSError as exc:
                if found:
                    raise
                last_error = str(exc)
            time.sleep(0.25)
        raise TimeoutError(f'{port} 等待模式 {expected} 逾時：{last_error}；'
                           '若 COM 改變請指定 --updater-port／--app-port；未重送切換命令')

    def run_app(client, identity):
        info = client.read(0x7010, 9)
        status = client.read(0x7030, 8)
        require(tuple(info[:3]) >= (0, 1, 11) and info[7:9] == [0x0016, 1],
                'Bootloader 版本／Target 不符合本測試')
        require(status[0] == 0 and status[1] == 0 and status[7] == 0,
                'Updater 非 Idle／有錯誤或待處理命令，停止測試且不處理 PENDING 映像')
        log(event='updater_mode_pass', port=updater_port, identity=identity,
            boot_version=info[:3], status=status)
        client.write_single(0x7020, 5)  # Verify/run active APP; no erase/download/install.
        log(event='updater_run_app_ack', port=updater_port)

    reconnect(updater_port, [0xB016, 1, 2], run_app)

    def check_return(client, identity):
        require(identity == original and client.read(0x2000, 3) == version,
                '返回 APP 的識別／版本與切換前不同')
        clean_app(client)
        log(event='updater_app_return_pass', port=app_port, identity=identity, app_version=version)

    reconnect(app_port, [0xA016, 1, 1], check_return)
    log(event='updater_cycle_pass', flash_download=False,
        scope='APP→USB Updater→APP 模式切換與重啟後軟體狀態；實體安全輸出仍待量測')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port', help='例如 COM8；未指定時列出串列埠供選擇')
    parser.add_argument('--baud', type=int, default=115200)
    parser.add_argument('--station', type=int, choices=range(1, 248), default=2, metavar='1..247')
    parser.add_argument('--parity', choices=['N', 'E', 'O'], default='N')
    parser.add_argument('--stopbits', type=int, choices=[1, 2], default=1)
    parser.add_argument('--timeout', type=float, default=2)
    parser.add_argument('--log-dir', type=pathlib.Path, default=pathlib.Path('safety_test_results'))
    sub = parser.add_subparsers(dest='action')
    sub.add_parser('temperature-cycle', help='執行一次 MCU 過溫測試循環')
    sub.add_parser('low-voltage-cycle', help='執行一次低電壓測試循環')
    sub.add_parser('multi-fault-cycle', help='雙重故障互鎖，測試兩種解除順序')
    sub.add_parser('reset-guard-cycle', help='故障重置確認碼、代碼與重複命令防護')
    sub.add_parser('invalid-sample-cycle', help='無效溫度樣本不得誤觸發或解除過溫')
    sub.add_parser('injection-guard-cycle', help='注入命令錯誤 key、參數、部分及唯讀誤寫防護')
    sub.add_parser('broadcast-guard-cycle', help='站號 0 不得啟用測試注入或回覆，限單台 DUT 測試線')
    axf = sub.add_parser('production-symbol-check', help='離線檢查一般 Debug／Release AXF 的具名測試符號')
    axf.add_argument('--axf', type=pathlib.Path, required=True, help='同次一般組態建置的 AXF 路徑')
    update = sub.add_parser('updater-cycle', help='SafetyTestEVK APP→USB Updater→APP，不下載映像')
    update.add_argument('--updater-port', help='USB 重新列舉後 Updater COM，預設沿用 --port')
    update.add_argument('--app-port', help='返回 APP 的 COM，預設沿用 --port')
    sub.add_parser('production-guard-cycle', help='一般 Debug／Release 測試 Register 封鎖，僅送零／停用值')
    sub.add_parser('pwm-multi-fault-cycle', help='EVK 雙重故障四路 PWM 互鎖，兩種重置順序')
    sub.add_parser('pwm-inhibit-cycle', help='EVK 四路 PWM 要求值／套用值的安全抑制循環')
    repeat = sub.add_parser('repeat-cycle', help='連續四項案例，檢查殘留狀態與環形紀錄覆寫')
    repeat.add_argument('--cycles', type=int, choices=range(2, 101), default=3, metavar='2..100')
    for action in ['read', 'write06', 'write10']:
        p = sub.add_parser(action)
        p.add_argument('address', type=number, help='HEX 請加 0x 前綴')
        if action == 'read':
            p.add_argument('count', type=number)
        else:
            p.add_argument('values', type=number, nargs='+' if action == 'write10' else 1)
    args = parser.parse_args()
    args.action = args.action or 'temperature-cycle'
    if args.action == 'production-symbol-check':
        return production_symbol_check(args)
    if args.timeout <= 0:
        parser.error('timeout 必須大於零')
    try:
        import serial
    except ImportError:
        print('缺少 pyserial，請先執行：python -m pip install pyserial')
        return 2
    if args.port is None:
        from serial.tools import list_ports
        ports = list(list_ports.comports())
        if not ports:
            print('找不到 COM 埠，請接上 USB CDC 或 RS485 轉接器。')
            return 1
        if len(ports) == 1:
            args.port = ports[0].device
        else:
            for i, p in enumerate(ports, 1):
                print(f'{i}. {p.device} — {p.description}')
            try:
                selected = int(input('請選擇 DUT 的 COM 埠編號：'))
                if not 1 <= selected <= len(ports):
                    raise ValueError()
                args.port = ports[selected-1].device
            except (ValueError, EOFError):
                print('選擇無效，尚未送出測試命令。')
                return 1
        print(f'使用 {args.port}，{args.baud} / 8{args.parity}{args.stopbits}，站號 {args.station}')
    args.log_dir.mkdir(parents=True, exist_ok=True)
    stamp = datetime.datetime.now().strftime('%Y%m%d_%H%M%S_%f')
    path = args.log_dir / (stamp+'.jsonl')
    with path.open('x', encoding='utf-8') as file:
        def log(**entry):
            entry['time'] = datetime.datetime.now().astimezone().isoformat()
            file.write(json.dumps(entry, ensure_ascii=False)+'\n'); file.flush()
            if entry.get('event') not in ('TX', 'RX', 'sample', 'retained_record'):
                print(json.dumps(entry, ensure_ascii=False))
        log(event='start', action=args.action, port=args.port, baud=args.baud, station=args.station)
        try:
            if args.action == 'updater-cycle':
                @contextmanager
                def open_client(name):
                    with serial.Serial(name, args.baud, bytesize=8, parity=args.parity,
                                       stopbits=args.stopbits, timeout=args.timeout,
                                       write_timeout=args.timeout) as port:
                        yield Client(port, args.station, log)
                updater_cycle(open_client, args.port, args.updater_port or args.port,
                              args.app_port or args.port, log)
            else:
                with serial.Serial(args.port, args.baud, bytesize=8, parity=args.parity,
                                   stopbits=args.stopbits, timeout=args.timeout, write_timeout=args.timeout) as port:
                    c = Client(port, args.station, log)
                    if args.action == 'temperature-cycle':
                        temperature_cycle(c)
                    elif args.action == 'low-voltage-cycle':
                        low_voltage_cycle(c)
                    elif args.action == 'multi-fault-cycle':
                        multi_fault_cycle(c)
                    elif args.action == 'reset-guard-cycle':
                        reset_guard_cycle(c)
                    elif args.action == 'repeat-cycle':
                        repeat_cycle(c, args.cycles)
                    elif args.action == 'invalid-sample-cycle':
                        invalid_sample_cycle(c)
                    elif args.action == 'injection-guard-cycle':
                        injection_guard_cycle(c)
                    elif args.action == 'broadcast-guard-cycle':
                        broadcast_guard_cycle(c)
                    elif args.action == 'production-guard-cycle':
                        production_guard_cycle(c)
                    elif args.action == 'pwm-multi-fault-cycle':
                        pwm_inhibit_cycle(c, multi=True)
                    elif args.action == 'pwm-inhibit-cycle':
                        pwm_inhibit_cycle(c)
                    elif args.action == 'read':
                        print(' '.join(f'{v:04X}' for v in c.read(args.address, args.count)))
                    elif args.action == 'write06':
                        c.write_single(args.address, args.values[0])
                    else:
                        c.write_multiple(args.address, args.values)
            log(event='complete', result='PASS')
            if args.action == 'updater-cycle':
                print('通過：APP → USB Updater → APP 切換完成，未下載映像。')
            elif args.action == 'production-guard-cycle':
                print('通過：正式韌體測試 Register 已拒絕 73 項存取。')
            else:
                print('通過：軟體保護循環完成。PWM／DO 實際電位仍需量測。' if args.action in
                  ('temperature-cycle', 'low-voltage-cycle', 'multi-fault-cycle',
                   'reset-guard-cycle', 'repeat-cycle', 'invalid-sample-cycle',
                   'injection-guard-cycle', 'broadcast-guard-cycle', 'pwm-inhibit-cycle', 'pwm-multi-fault-cycle') else '操作成功。')
        except (Exception, KeyboardInterrupt) as exc:
            log(event='complete', result='FAIL', error=str(exc))
            print(f'測試未通過，紀錄：{path}')
            return 1
    print(f'紀錄：{path}')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
