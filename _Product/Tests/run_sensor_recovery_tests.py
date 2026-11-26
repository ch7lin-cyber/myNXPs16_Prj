"""Run production recovery=3 and legacy conversion regression=1 on native GCC/Clang."""
from pathlib import Path
import os
import subprocess
import tempfile
import sys
ROOT=Path(__file__).resolve().parents[2]
PRODUCT=ROOT/'_Product'
PLATFORM=ROOT/'_Modules/PlatformCore'
includes=sorted({str(p.parent) for p in PLATFORM.rglob('*.h')} |
                {str(p.parent) for p in PRODUCT.rglob('*.h') if 'Tests' not in p.parts})
analog=PLATFORM/'L03_SystemPlatform/SystemService/AnalogInputService'
sources=[analog/'SensorConversionService.c',analog/'PiecewiseLinearTable.c']
sources+=list((analog/'SensorTables').glob('*.c'))
sources+=[PRODUCT/'Source/L03_System/product_sensor_measurement_service.c']
with tempfile.TemporaryDirectory(prefix='sensor-recovery-') as directory:
    for name,flags in [('test_product_sensor_recovery',[]),('test_product_sensor_measurement_service',['-DPRODUCT_SENSOR_RECOVERY_GOOD_SAMPLES=1'])]:
        binary=Path(directory)/name
        cmd=[os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-DFACTORY_CALIBRATION_INPUT_COUNT=4']
        if '--sanitize' in sys.argv: cmd+=['-fsanitize=address,undefined','-fno-omit-frame-pointer']
        cmd+=flags+['-I'+p for p in includes]
        cmd+=[str(p) for p in sources]+[str(PRODUCT/'Tests/Unit'/(name+'.c')),'-lm','-o',str(binary)]
        subprocess.run(cmd,check=True)
        subprocess.run([str(binary)],check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
        print(name+': PASS',flush=True)

with tempfile.TemporaryDirectory(prefix='adc-health-') as directory:
    for debug in (0,1):
        binary=Path(directory)/('adc_health_'+str(debug))
        cmd=[os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror',
             '-DPRODUCT_HOST_TEST=1','-DPRODUCT_ADC_DEBUG_ENABLE='+str(debug)]
        if '--sanitize' in sys.argv: cmd+=['-fsanitize=address,undefined','-fno-omit-frame-pointer']
        cmd+=['-I'+str(PRODUCT/'Tests/Unit/mocks')]+['-I'+p for p in includes]
        cmd+=[str(PLATFORM/'L02_PlatformHAL/src/HalAdc.c'),
              str(PLATFORM/'L02_PlatformHAL/src/HalAdcMeasurement.c'),
              str(PRODUCT/'Tests/Unit/test_product_adc_read_trace.c'),'-lm','-o',str(binary)]
        subprocess.run(cmd,check=True)
        subprocess.run([str(binary)],check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
        print('ADC sample health DEBUG='+str(debug)+': PASS',flush=True)

for definition in ('PRODUCT_SENSOR_STALE_TIMEOUT_MS=0',
                   'PRODUCT_SENSOR_STALE_TIMEOUT_MS=2147483648UL',
                   'PRODUCT_SENSOR_RECOVERY_GOOD_SAMPLES=0',
                   'PRODUCT_SENSOR_RECOVERY_GOOD_SAMPLES=65536'):
    result=subprocess.run([os.environ.get('CC','cc'),'-std=c11','-D'+definition,
                           '-I'+str(PRODUCT/'Config'),'-x','c','-fsyntax-only','-'],
                          input='#include "ProductSensorMeasurementConfig.h"\n',
                          text=True,capture_output=True)
    assert result.returncode!=0,definition
print('Invalid sensor quality configuration bounds: PASS')
