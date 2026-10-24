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

    # Exercise the real board quiesce function with register/DAC mocks.
    mocks = Path(directory) / 'board_mocks'
    mocks.mkdir()
    (mocks / 'fsl_device_registers.h').write_text('''
#ifndef UPDATE_BOARD_REGISTER_MOCKS
#define UPDATE_BOARD_REGISTER_MOCKS
#include <stdint.h>
typedef struct { uint32_t TCR; } TestTimer;
typedef struct { uint32_t DIRSET[2]; } TestGpio;
typedef struct { uint32_t RESETCTRL; } TestPmc;
typedef struct { uint32_t SWR_RESET; } TestSyscon;
extern TestTimer timers[4]; extern TestGpio gpio;
extern TestPmc pmc; extern TestSyscon syscon;
#define CTIMER0 (&timers[0])
#define CTIMER1 (&timers[1])
#define CTIMER2 (&timers[2])
#define CTIMER3 (&timers[3])
#define GPIO (&gpio)
#define PMC (&pmc)
#define SYSCON (&syscon)
#define PMC_RESETCTRL_SWRRESETENABLE_MASK 1U
uint32_t __get_PRIMASK(void);
void __disable_irq(void); void __set_PRIMASK(uint32_t);
void __DSB(void); void __WFI(void);
#endif
''')
    (mocks / 'fsl_gpio.h').write_text('''
#include "fsl_device_registers.h"
void GPIO_PortClear(TestGpio *, unsigned, uint32_t);
''')
    (mocks / 'fsl_iocon.h').write_text('''
#define IOCON ((void *)0)
#define IOCON_FUNC0 0U
#define IOCON_MODE_PULLDOWN 1U
#define IOCON_DIGITAL_EN 2U
void IOCON_PinMuxSet(void *, unsigned, unsigned, unsigned);
''')
    for evk in (0, 1):
        output = str(Path(directory) / ('quiesce' + str(evk)))
        flags = ['-DPRODUCT_SAFETY_TEST_EVK=1', '-DPRODUCT_SAFETY_TEST_BUILD=1',
                 '-DPRODUCT_SAFETY_TEST_TARGET=1'] if evk else []
        subprocess.run(common + flags + ['-I' + str(mocks),
            '-I' + str(P / 'Source/L01_ProductDriver'),
            '-I' + str(P / 'Source/L02_BSP'),
            str(P / 'Tests/Unit/test_product_fw_update_quiesce.c'),
            str(P / 'Source/L01_ProductDriver/product_fw_update_board.c'),
            '-o', output], check=True)
        subprocess.run([output], check=True)
        print(f'EVK={evk}: real updater quiesce and DAC failure policy PASS')

config = (ROOT / '.cproject').read_text()
assert 'id="PROGRAM_FLASH" location="0x8000" size="0x1a000"' in config
linker = (P / 'linker/app_update_assert.ld').read_text()
assert 'LENGTH(PROGRAM_FLASH) == 0x1A000' in linker
assert '_image_end <= 0x8000 + 0x1A000 - 48' in linker
assert 'id="SRAMX" location="0x4000000" size="0x3fc0"' in config
source_entries = ET.fromstring(config).findall('.//sourceEntries')
assert len(source_entries) == 2
for entries in source_entries:
    assert len([e for e in entries if e.get('name') == '_Modules/PlatformCore/FwUpdateApp/src']) == 1
print('Debug/Release library source, APP Flash 0x8000/104KiB and reserved SRAMX mailbox: PASS')

