"""Transport regression tests, no physical serial port required."""
import contextlib
import io
import sys
import unittest
from pathlib import Path
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'Tools'))
import modbus_rtu_test_menu as m


class FakeSerial:
    is_open = True

    def __init__(self, response):
        self.response = io.BytesIO(response)
        self.sent = b''

    def read(self, count):
        return self.response.read(count)

    def read_until(self, terminator, size):
        return self.response.read(size)

    def write(self, data):
        self.sent = data

    def flush(self):
        pass

    def reset_input_buffer(self):
        pass


class TransportTests(unittest.TestCase):
    def client(self, mode, payload, raw=None):
        wire = raw if raw is not None else (
            m.encode_ascii_frame(payload) if mode == 'ascii' else m.append_crc(payload))
        port = FakeSerial(wire)
        with patch.object(m.serial, 'Serial', return_value=port) as ctor:
            client = m.ModbusRtuClient('fake', mode=mode)
        self.assertEqual(ctor.call_args.kwargs['bytesize'], 7 if mode == 'ascii' else 8)
        self.assertEqual(ctor.call_args.kwargs['parity'], 'E' if mode == 'ascii' else 'N')
        return client, port

    def test_known_request_vectors(self):
        payload = bytes.fromhex('020310000004')
        self.assertEqual(m.append_crc(payload).hex().upper(), '02031000000440FA')
        self.assertEqual(m.encode_ascii_frame(payload), b':020310000004E7\r\n')

    def test_read_and_write_both_modes(self):
        for mode in ('rtu', 'ascii'):
            with self.subTest(mode=mode):
                client, port = self.client(mode, bytes.fromhex('0203041234FFFF'))
                self.assertEqual(client.read_holding_registers(0x1000, 2), [0x1234, 0xFFFF])
                expected = bytes.fromhex('020310000002')
                self.assertEqual(port.sent, m.encode_ascii_frame(expected) if mode == 'ascii' else m.append_crc(expected))
                client, _ = self.client(mode, bytes.fromhex('02061000FFFF'))
                client.write_single_register(0x1000, 0xFFFF)
                client, _ = self.client(mode, bytes.fromhex('021010000002'))
                client.write_multiple_registers(0x1000, [1, 2])

    def test_errors_both_modes(self):
        for mode in ('rtu', 'ascii'):
            for payload in ('028302', '0303020000', '0206020000', '0203040000'):
                with self.subTest(mode=mode, payload=payload):
                    client, _ = self.client(mode, bytes.fromhex(payload))
                    with self.assertRaises(m.ModbusError):
                        client.read_holding_registers(0x1000, 1)
        client, _ = self.client('rtu', b'', raw=b'\x02\x03\x02\x00\x00\x00\x00')
        with self.assertRaisesRegex(m.ModbusError, 'CRC'):
            client.read_holding_registers(0x1000, 1)

    def test_ascii_malformed_frames(self):
        for frame in (b'', b'0203020000F9\r\n', b':0203020000F9\n',
                      b':0203020000F8\r\n', b':0203020000G9\r\n',
                      b':0203020000 F9\r\n', b':0203020000F\r\n'):
            with self.subTest(frame=frame), self.assertRaises(m.ModbusError):
                m.decode_ascii_frame(frame)

    def test_large_ascii_read_and_echo_mismatch(self):
        client, _ = self.client('ascii', b'\x02\x03\xFA' + b'\x00\x01' * 125)
        self.assertEqual(client.read_holding_registers(0, 125), [1] * 125)
        for mode in ('rtu', 'ascii'):
            client, _ = self.client(mode, bytes.fromhex('020610000001'))
            with self.assertRaisesRegex(m.ModbusError, 'echo'):
                client.write_single_register(0x1000, 2)


if __name__ == '__main__':
    with patch.object(m.time, 'sleep'), contextlib.redirect_stdout(io.StringIO()):
        unittest.main()
