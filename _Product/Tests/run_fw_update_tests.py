"""APP transition tests and shared ABI/linker checks. No board reset is executed."""
from pathlib import Path
import subprocess,tempfile
ROOT=Path(__file__).resolve().parents[2]
P=ROOT/'_Product'
with tempfile.TemporaryDirectory() as d:
 out=str(Path(d)/'test')
 subprocess.run(['gcc','-std=c99','-Wall','-Wextra','-Werror',
  '-I'+str(P/'Config'),'-I'+str(P/'Shared'),'-I'+str(P/'Source/L03_System'),
  str(P/'Tests/Unit/test_product_fw_update.c'),str(P/'Source/L03_System/product_fw_update.c'),'-o',out],check=True)
 subprocess.run([out],check=True)
config=(ROOT/'.cproject').read_text()
assert 'id="PROGRAM_FLASH" location="0x8000" size="0x18000"' in config
assert 'id="SRAMX" location="0x4000000" size="0x3fc0"' in config
print('APP Flash 0x8000/96KiB and reserved SRAMX mailbox: PASS')
