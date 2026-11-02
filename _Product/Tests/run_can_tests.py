"""Compile the real transport against a replaceable MCAN boundary, no SDK/submodule required."""
from pathlib import Path
import subprocess
import tempfile
import os
ROOT = Path(__file__).resolve().parents[2]
UNIT = ROOT / '_Product/Tests/Unit'
DRIVER = ROOT / '_Product/Source/L01_ProductDriver'
with tempfile.TemporaryDirectory(prefix='can-tests-') as tmp:
    # Keep production ProductCanConfig.h intact; force only the RAM address after
    # including it. Host writes real allocated RAM instead of an MCU address.
    shim = Path(tmp) / 'ram.h'
    shim.write_text('#include "ProductCanConfig.h"\n#include "fsl_mcan.h"\n'
                    '#undef PRODUCT_CAN_MESSAGE_RAM_BASE\n'
                    '#define PRODUCT_CAN_MESSAGE_RAM_BASE ((uintptr_t)mock_message_ram)\n')
    for test_build, mode in ((0, 0), (1, 0), (1, 1)):
        out = Path(tmp) / f'can-{test_build}-{mode}'
        cmd = [os.environ.get('CC', 'gcc'), '-std=c11', '-Wall', '-Wextra', '-Werror',
               '-g', '-fsanitize=undefined', '-fno-sanitize-recover=all',
               f'-DPRODUCT_CAN_TEST_BUILD={test_build}', f'-DPRODUCT_CAN_INTERNAL_LOOPBACK={mode}',
               '-include', str(shim), '-I'+str(UNIT/'mocks/can'),
               '-I'+str(ROOT/'_Product/Config'), '-I'+str(DRIVER),
               str(DRIVER/'product_can_driver.c'), str(UNIT/'mocks/can/mock_mcan.c'),
               str(UNIT/'test_product_can_driver.c'), '-o', str(out)]
        subprocess.run(cmd, check=True)
        subprocess.run([str(out)], check=True)
        symbols = subprocess.check_output(['nm', str(out)], text=True)
        assert ('ProductCanTest_Process' in symbols) == bool(test_build)
        assert ('g_can_test_watch' in symbols) == bool(test_build)
    # Production must reject accidental loopback.
    bad = subprocess.run([os.environ.get('CC', 'gcc'), '-x', 'c', '-fsyntax-only',
                          '-DPRODUCT_CAN_INTERNAL_LOOPBACK=1',
                          str(ROOT/'_Product/Config/ProductCanConfig.h')], capture_output=True)
    assert bad.returncode != 0 and b'requires PRODUCT_CAN_TEST_BUILD' in bad.stderr
    print('Production loopback guard: PASS')
