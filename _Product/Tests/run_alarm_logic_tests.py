"""Build/run L04 and L05 Alarm/Logic tests without CKLIB or MCU hardware."""
from pathlib import Path
import subprocess
import tempfile
import sys
import os

root = Path(__file__).resolve().parents[2]
platform = root / '_Modules/PlatformCore'
includes = [root / '_Product/Tests/Integration/Mocks', root / '_Product/Config',
            root / '_Product/Source/L05_Application', root / '_Product/Source/L03_System',
            platform / 'L04_FunctionBlock/Utility', platform / 'L04_FunctionBlock/Alarm',
            platform / 'L03_SystemPlatform/SystemService/DigitalInputService']
common = [root / '_Product/Source/L05_Application' / name for name in
          ['product_fb_schedule.c', 'product_fb_schedule_integration.c',
           'product_fb_moving_average_adapter.c', 'product_alarm_logic_adapter.c',
           'product_pv_processing_adapter.c']]
common += [platform / 'L04_FunctionBlock' / name for name in
           ['Utility/FbMovingAverage.c', 'Utility/FbLogic.c', 'Alarm/FbAlarm.c',
            'Utility/FbLinearization.c', 'Utility/FbSignalLinearization.c', 'Utility/FbInputSwitchOver.c']]
san = '--sanitize' in sys.argv
with tempfile.TemporaryDirectory() as folder:
    for name in ['test_product_alarm_logic', 'test_product_fb_schedule_integration',
                 'test_product_pv_schedule_integration', 'test_product_processed_alarm_source']:
        binary = Path(folder) / name
        cmd = ['cc', '-std=c99', '-O1' if san else '-O2', '-g', '-Wall',
               '-Wextra', '-Werror', '-pedantic']
        if san:
            cmd += ['-fsanitize=address,undefined', '-fno-omit-frame-pointer']
        cmd += [arg for path in includes for arg in ['-I', str(path)]]
        cmd += [str(root / '_Product/Tests/Integration' / (name + '.c'))]
        cmd += [str(path) for path in common] + ['-lm', '-o', str(binary)]
        subprocess.run(cmd, check=True)
        env = dict(os.environ, ASAN_OPTIONS='detect_leaks=0')
        subprocess.run([str(binary)], check=True, env=env)
    binary = Path(folder) / 'capacity'
    capacity_cmd = ['cc', '-std=c99', '-O2', '-Wall', '-Wextra', '-Werror', '-pedantic',
                   '-DPRODUCT_AL_CHANNEL_COUNT=2U', '-DPRODUCT_AL_ALARMS_PER_CHANNEL=2U',
                   '-DPRODUCT_AL_LOGIC_COUNT=4U', '-DPRODUCT_AL_PRE_LOGIC_COUNT=2U']
    if san:
        capacity_cmd += ['-fsanitize=address,undefined', '-fno-omit-frame-pointer']
    capacity_cmd += [arg for path in includes for arg in ['-I', str(path)]]
    capacity_cmd += [str(root / '_Product/Tests/Unit/test_product_alarm_logic_capacity.c'),
                     str(root / '_Product/Source/L05_Application/product_alarm_logic_adapter.c'),
                     str(platform / 'L04_FunctionBlock/Alarm/FbAlarm.c'),
                     str(platform / 'L04_FunctionBlock/Utility/FbLogic.c'), '-o', str(binary)]
    subprocess.run(capacity_cmd, check=True)
    subprocess.run([str(binary)], check=True, env=env)
