"""Host checks: EVK excludes hardware callers; production retains them."""
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
PRODUCT = ROOT / '_Product'
PLATFORM = ROOT / '_Modules/PlatformCore'
INCLUDES = sorted({p.parent for base in (PRODUCT, PLATFORM) for p in base.rglob('*.h')})
BASE = ['gcc', '-std=c11', '-Wall', '-Wextra', '-Werror',
        *['-I' + str(p) for p in INCLUDES]]
TEST = ['-DPRODUCT_SAFETY_TEST_BUILD=1', '-DPRODUCT_SAFETY_TEST_TARGET=1']

def source(name):
    return str(next(ROOT.rglob(name + '.c')))

with tempfile.TemporaryDirectory(prefix='safety-evk-') as directory:
    for evk in (0, 1):
        flags = TEST + ['-DPRODUCT_SAFETY_TEST_EVK=' + str(evk)]
        obj = str(Path(directory) / 'application.o')
        subprocess.run(BASE + flags + ['-c', source('product_application'), '-o', obj], check=True)
        undefined = subprocess.check_output(['nm', '-u', obj], text=True)
        for symbol in ('ProductAdcDriver_Init', 'AnalogInputService_Process',
                       'ProductSensorConfigurationConsumer_Process',
                       'NvmConfigurationEventConsumer_Initialize',
                       'NvmConfigurationEventConsumer_Process', 'ProductFramBankTest_Process',
                       'ProductFactoryCalibrationStorage_Initialize'):
            assert (symbol in undefined) == (not evk), (evk, symbol)
        for symbol in ('BspInternalAdc_RequestMcuTemperature', 'ProductMcuTemperatureSafety_Process',
                       'SafetyService_UpdateSource', 'SystemRoutine_ExecuteFast',
                       'SystemRoutine_ExecuteControl', 'ProductSafetyTest_Process'):
            assert symbol in undefined, (evk, symbol)
        executable = str(Path(directory) / 'low_voltage')
        subprocess.run(BASE + flags + [source(n) for n in (
            'product_low_voltage_safety', 'test_product_low_voltage_safety',
            'FaultService', 'SystemFaultService', 'SystemEventService', 'SnapshotService',
            'SafetyService')]
            + ['-o', executable], check=True)
        subprocess.run([executable], check=True)
        print(f'EVK={evk}: hardware isolation and low-voltage behavior PASS')

    # Refuse EVK when either safety build flag is missing or zero.
    for flags in ([], TEST[:1], TEST[1:],
                  ['-DPRODUCT_SAFETY_TEST_BUILD=0', TEST[1]]):
        result = subprocess.run(BASE + flags + ['-DPRODUCT_SAFETY_TEST_EVK=1',
            '-include', 'ProductSafetyTestConfig.h', '-x', 'c', '-fsyntax-only', '-'],
            input='', text=True, capture_output=True)
        assert result.returncode != 0, flags
    print('Invalid EVK build combinations rejected: PASS')
