#!/usr/bin/env python3
"""SafetyTest 上板工具：FC03／06／10、過溫循環與 JSONL 紀錄。"""
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
                elif args.action == 'read':
                    print(' '.join(f'{v:04X}' for v in c.read(args.address, args.count)))
                elif args.action == 'write06':
                    c.write_single(args.address, args.values[0])
                else:
                    c.write_multiple(args.address, args.values)
            log(event='complete', result='PASS')
            print('通過：軟體保護循環完成。PWM／DO 實際電位仍需量測。' if args.action == 'temperature-cycle' else '操作成功。')
        except (Exception, KeyboardInterrupt) as exc:
            log(event='complete', result='FAIL', error=str(exc))
            print(f'測試未通過，紀錄：{path}')
            return 1
    print(f'紀錄：{path}')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
