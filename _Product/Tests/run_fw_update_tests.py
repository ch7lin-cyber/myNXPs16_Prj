"""APP adapter regression, SDK-free library tests and IDE/linker checks.

Run from any directory with Python 3 and GCC available. No board reset occurs.
"""
from pathlib import Path
import subprocess
import tempfile
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[2]
P = ROOT / '_Product'
LIB = ROOT / '_Modules/PlatformCore/FwUpdateApp'
with tempfile.TemporaryDirectory() as directory:
    common = ['gcc', '-std=c99', '-Wall', '-Wextra', '-Werror',
              '-I' + str(P / 'Config'), '-I' + str(P / 'Shared'),
              '-I' + str(P / 'Source/L03_System')]
    cases = {
        'product_adapter': [P / 'Tests/Unit/test_product_fw_update.c',
                            P / 'Source/L03_System/product_fw_update.c'],
        'reusable_library': [LIB / 'tests/test_fw_update_app.c'],
    }
    for name, sources in cases.items():
        output = str(Path(directory) / name)
        subprocess.run(common + [str(p) for p in sources]
                       + [str(LIB / 'src/fw_update_app.c'), '-o', output], check=True)
        subprocess.run([output], check=True)

config = (ROOT / '.cproject').read_text()
assert 'id="PROGRAM_FLASH" location="0x8000" size="0x18000"' in config
assert 'id="SRAMX" location="0x4000000" size="0x3fc0"' in config
source_entries = ET.fromstring(config).findall('.//sourceEntries')
assert len(source_entries) == 2
for entries in source_entries:
    assert len([e for e in entries if e.get('name') == '_Modules/PlatformCore/FwUpdateApp/src']) == 1
print('Debug/Release library source, APP Flash 0x8000/96KiB and reserved SRAMX mailbox: PASS')

