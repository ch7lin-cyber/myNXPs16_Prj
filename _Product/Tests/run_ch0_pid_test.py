"""Explicit fixed-MV integration test. No CKLIB archive, MCU or hardware output."""
import argparse
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--sanitize',action='store_true')
    args=parser.parse_args()
    root=Path(__file__).resolve().parents[2]
    fb=root/'_Modules/PlatformCore/L04_FunctionBlock'
    app=root/'_Product/Source/L05_Application'
    includes=[root/'_Product/Tests/Integration/Mocks',root/'_Product/Config',app,
              root/'_Product/Source/L03_System',
              fb/'Control',fb/'Utility',fb/'Alarm',
              root/'_Modules/PlatformCore/L03_SystemPlatform/SystemService/DigitalInputService']
    sources=[app/n for n in ['product_fb_schedule.c','product_fb_schedule_integration.c',
              'product_fb_moving_average_adapter.c','product_alarm_logic_adapter.c',
              'product_pv_processing_adapter.c','product_pid_controller_adapter.c']]
    sources += [fb/n for n in ['Control/FbPid.c','Utility/FbMovingAverage.c','Utility/FbLogic.c',
                'Alarm/FbAlarm.c','Utility/FbLinearization.c','Utility/FbSignalLinearization.c',
                'Utility/FbInputSwitchOver.c']]
    sources += [root/'_Product/Tests/Integration/test_product_ch0_pid_test.c']
    with tempfile.TemporaryDirectory(prefix='ch0-pid-') as directory:
        binary=Path(directory)/('test.exe' if os.name=='nt' else 'test')
        cmd=shlex.split(os.environ.get('CC','cc'))+['-std=c99','-O1','-Wall','-Wextra','-Werror','-pedantic','-g',
             '-DFB_PID_TEST_BACKEND=1','-DPRODUCT_PID_CH0_TEST_ENABLED=1']
        if args.sanitize: cmd+=['-fsanitize=address,undefined','-fno-omit-frame-pointer']
        cmd += ['-I'+str(p) for p in includes]+[str(p) for p in sources]+['-lm','-o',str(binary)]
        subprocess.run(cmd,check=True)
        subprocess.run([str(binary)],check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))

if __name__=='__main__': main()
