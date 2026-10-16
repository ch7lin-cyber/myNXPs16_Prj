"""Cross-check firmware, CSV and PC tools without connecting hardware."""
import csv
import re
import sys
import unittest
from pathlib import Path
from unittest.mock import patch

PRODUCT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(PRODUCT / 'Tools'))
import product_register_addresses as addresses
import calys1500_calibration as calibration
import calys1500_loop_test as loop
import modbus_rtu_test_menu as menu


class RegisterMapTests(unittest.TestCase):
    def test_firmware_csv_and_tools_diagnostic_blocks(self):
        header = (PRODUCT / 'Source/L03_System/product_modbus_register_adapter.h').read_text()
        with (PRODUCT / 'RegisterMap/register_map.csv').open(newline='') as source:
            rows = list(csv.DictReader(source))
        occupied = set()
        for row in rows:
            start = int(row['ModbusAddress'], 16)
            for address in range(start, start + int(row['RegisterCount'])):
                self.assertNotIn(address, occupied)
                occupied.add(address)
        self.assertEqual(menu.ADC_DIAGNOSTIC_BASE_ADDRESS, 0x4E00)
        self.assertEqual(menu.ADC_DIAGNOSTIC_DEVICE_STRIDE, 0x40)
        for channel, expected in enumerate((0x4E00, 0x4E40, 0x4E80, 0x4EC0)):
            self.assertEqual(addresses.adc_diagnostic_address(channel), expected)
            row = next(r for r in rows if r['MemberName'] == f'ADC{channel} Diagnostic Block')
            self.assertEqual(int(row['ModbusAddress'], 16), expected)
            self.assertEqual(int(row['RegisterCount']), 64)
            match = re.search(rf'#define PRODUCT_MODBUS_ADC{channel}_DIAGNOSTICS_BASE_ADDRESS\s+\((0x[0-9A-F]+)U\)', header)
            self.assertIsNotNone(match)
            self.assertEqual(int(match[1], 16), expected)
        aliases = {int(r['ModbusAddress'], 16): r for r in rows
                   if r['ClassName'] == 'DTX Compatibility'}
        self.assertEqual(set(aliases), set(range(0x4908, 0x4920)))
        for row in aliases.values():
            self.assertEqual(row['Access'], 'R/W')
            self.assertEqual(row['ApplyPolicy'], 'Immediate')
        for channel in range(4):
            self.assertIn(f'0x{0x1301 + channel * 3:04X}', aliases[0x4910 + channel]['Description'])
            self.assertIn(f'0x{0x1400 + channel:04X}', aliases[0x4910 + channel]['Description'])

    def test_calys_snapshot_and_sweep_addresses_all_channels(self):
        for channel in range(4):
            calls = []
            base = 0x4E00 + channel * 0x40
            class Dut:
                def read_holding_registers(self, address, count):
                    calls.append((address, count))
                    replies = {base + 0x11: [0, 42], base + 0x2F: [0x80, 1, 0, 0, 0xFFFF, 0xFFFF],
                               base + 3: [0], 0x1000 + channel * 0x10: [0x3F80, 0, 61]}
                    return replies[address]
            dut = Dut()
            sample = calibration._read_snapshot(dut, channel)
            self.assertEqual(sample['count'], 42)
            self.assertEqual(sample['microvolts'], -1)
            self.assertEqual(calls, [(base + 0x11, 2), (0x1000 + channel * 0x10, 3),
                                    (base + 0x2F, 6), (base + 3, 1)])
            with patch.object(loop, 'modbus_helpers', return_value=(None, None, None, calibration._u32)):
                self.assertEqual(loop.sample_count(dut, channel), 42)
            self.assertEqual(calls[-1], (base + 0x11, 2))

    def test_calys_sources_have_no_legacy_adc_addresses(self):
        for name in ('calys1500_calibration.py', 'calys1500_loop_test.py'):
            source = (PRODUCT / 'Tools' / name).read_text()
            self.assertNotRegex(source, r'0x48(?:[2-9A-Fa-f][0-9A-Fa-f])')


if __name__ == '__main__':
    unittest.main()
