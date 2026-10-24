#!/usr/bin/env python3
"""SafetyTest 上板工具：FC03／06／10、過溫／低電壓循環與 JSONL 紀錄。"""
import argparse
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
    for action in ['read', 'write06', 'write10']:
        p = sub.add_parser(action)
        p.add_argument('address', type=number, help='HEX 請加 0x 前綴')
        if action == 'read':
            p.add_argument('count', type=number)
        else:
            p.add_argument('values', type=number, nargs='+' if action == 'write10' else 1)
    args = parser.parse_args()
    args.action = args.action or 'temperature-cycle'
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
            if entry.get('event') not in ('TX', 'RX', 'sample'):
                print(json.dumps(entry, ensure_ascii=False))
        log(event='start', action=args.action, port=args.port, baud=args.baud, station=args.station)
        try:
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
                elif args.action == 'read':
                    print(' '.join(f'{v:04X}' for v in c.read(args.address, args.count)))
                elif args.action == 'write06':
                    c.write_single(args.address, args.values[0])
                else:
                    c.write_multiple(args.address, args.values)
            log(event='complete', result='PASS')
            print('通過：軟體保護循環完成。PWM／DO 實際電位仍需量測。' if args.action in
                  ('temperature-cycle', 'low-voltage-cycle', 'multi-fault-cycle',
                   'reset-guard-cycle') else '操作成功。')
        except (Exception, KeyboardInterrupt) as exc:
            log(event='complete', result='FAIL', error=str(exc))
            print(f'測試未通過，紀錄：{path}')
            return 1
    print(f'紀錄：{path}')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
