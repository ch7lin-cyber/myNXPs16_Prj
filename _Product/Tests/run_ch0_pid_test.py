"""Explicit fixed-MV integration test. No CKLIB archive, MCU or hardware output."""
import argparse
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import tempfile

def compiler_command(parser, explicit=None):
    """Resolve a native GCC-compatible compiler, preserving Windows paths."""
    if explicit is not None:
        command=[explicit]  # --cc is one executable path, never a shell command.
    elif os.environ.get('CC','').strip():
        value=os.environ['CC'].strip()
        # An existing path is a single argument even when it contains spaces.
        unquoted=value.strip('"')
        if Path(unquoted).is_file(): command=[unquoted]
        else:
            try:
                command=shlex.split(value,posix=os.name!='nt')
                if os.name=='nt': command=[part.strip('"') for part in command]
            except ValueError as error:
                parser.error('Invalid CC setting: '+str(error))
    else:
        names=['gcc','clang','cc'] if os.name=='nt' else ['cc','gcc','clang']
        command=next(([path] for name in names if (path:=shutil.which(name))),None)
        if command is None and os.name=='nt':
            command=next(([str(path)] for path in
                [Path('C:/msys64/ucrt64/bin/gcc.exe'),Path('C:/msys64/mingw64/bin/gcc.exe')]
                if path.is_file()),None)
    if not command or not shutil.which(command[0]):
        parser.error('Host C compiler not found. Python alone cannot build this test. '
                     'Use a Windows/native gcc or clang compiler on PATH, or '
                     '--cc "C:/path/to/gcc.exe". MCUXpresso arm-none-eabi-gcc targets '
                     'the MCU and cannot build a runnable Windows Host test.')
    executable=shutil.which(command[0])
    if 'arm-none-eabi' in Path(executable).name.lower():
        parser.error('arm-none-eabi-gcc is an MCU cross compiler; select native gcc/clang for Host tests.')
    command[0]=executable
    return command

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--sanitize',action='store_true')
    parser.add_argument('--cc',help='Native gcc/clang executable name or full path (no compiler flags)')
    args=parser.parse_args()
    compiler=compiler_command(parser,args.cc)
    print('Host compiler:',compiler[0],flush=True)
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
        cmd=compiler+['-std=c99','-O1','-Wall','-Wextra','-Werror','-pedantic','-g',
             '-DFB_PID_TEST_BACKEND=1','-DPRODUCT_PID_CH0_TEST_ENABLED=1']
        if args.sanitize: cmd+=['-fsanitize=address,undefined','-fno-omit-frame-pointer']
        cmd += ['-I'+str(p) for p in includes]+[str(p) for p in sources]+['-lm','-o',str(binary)]
        try:
            subprocess.run(cmd,check=True)
        except FileNotFoundError:
            parser.error('Compiler could not be launched: '+compiler[0]+'. Check its path and installation.')
        except subprocess.CalledProcessError as error:
            parser.exit(error.returncode or 1,'Host compilation failed; see compiler output above. '
                        'If sanitizers are unavailable, retry without --sanitize.\n')
        subprocess.run([str(binary)],check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))

if __name__=='__main__': main()
