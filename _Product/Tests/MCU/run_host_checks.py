#!/usr/bin/env python3
"""Run from any directory with Python 3 and gcc on PATH; no third-party packages."""
from pathlib import Path
import subprocess
import tempfile
import sys

root = Path(__file__).resolve().parents[3]
source = root / '_Product/Source/L05_Application'
utility = root / '_Modules/PlatformCore/L04_FunctionBlock/Utility'
with tempfile.TemporaryDirectory() as tmp:
    for enabled in (0, 1):
        test = root / ('_Product/Tests/MCU/test_ch0_trace.c' if enabled else
                       '_Product/Tests/Integration/test_product_fb_schedule_integration.c')
        exe = Path(tmp) / ('trace%d.exe' % enabled)
        cmd = ['gcc','-std=c99','-Wall','-Wextra','-Werror','-pedantic',
               '-DPRODUCT_FB_CH0_TRACE_ENABLED=%d' % enabled]
        for inc in (root/'_Product/Tests/Integration/Mocks',root/'_Product/Config',source,
                    root/'_Product/Source/L03_System',utility,
                    utility.parent/'Alarm',
                    root/'_Modules/PlatformCore/L03_SystemPlatform/SystemService/DigitalInputService'):
            cmd += ['-I',str(inc)]
        cmd += [str(test)] + [str(source / name) for name in (
            'product_fb_schedule.c','product_fb_moving_average_adapter.c',
            'product_fb_schedule_integration.c','product_alarm_logic_adapter.c',
            'product_pv_processing_adapter.c')]
        cmd += [str(utility/'FbMovingAverage.c'),str(utility/'FbLogic.c'),
                str(utility.parent/'Alarm/FbAlarm.c'), str(utility/'FbLinearization.c'),
                str(utility/'FbSignalLinearization.c'), str(utility/'FbInputSwitchOver.c'),
                '-lm','-o',str(exe)]
        subprocess.run(cmd,check=True)
        csv = Path(tmp) / 'host_only.csv'
        subprocess.run([str(exe)] + ([str(csv)] if enabled else []),check=True)
        if enabled:
            import json
            result = subprocess.run([sys.executable,
                str(Path(__file__).parent / 'analyze_ch0_trace.py'),str(csv)],
                capture_output=True,text=True)
            report = json.loads(result.stdout)
            assert result.returncode == 2, result.stderr
            assert report['functional'] == 'PASS', report
            assert report['timing'] == 'INCOMPLETE', report
            print('PASS: actual C trace CSV -> analyzer (HOST ONLY, no MCU timing)')
        else:
            symbols = subprocess.run(['nm',str(exe)],capture_output=True,text=True,check=True)
            assert 'g_product_fb_ch0_trace' not in symbols.stdout
subprocess.run([sys.executable,'-m','unittest','-v','test_analyze_ch0_trace.py'],
               cwd=Path(__file__).parent,check=True)
