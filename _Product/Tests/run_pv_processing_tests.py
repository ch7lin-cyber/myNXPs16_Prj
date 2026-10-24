"""Build/run L05 PV processing integration; no MCU or production IO."""
import argparse
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--sanitize', action='store_true')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    utility = root / '_Modules/PlatformCore/L04_FunctionBlock/Utility'
    application = root / '_Product/Source/L05_Application'
    sources = [application / 'product_pv_processing_adapter.c',
               utility / 'FbLinearization.c', utility / 'FbSignalLinearization.c',
               utility / 'FbInputSwitchOver.c',
               root / '_Product/Tests/Unit/test_product_pv_processing_adapter.c']
    with tempfile.TemporaryDirectory(prefix='pv-processing-') as directory:
        binary = Path(directory) / ('test.exe' if os.name == 'nt' else 'test')
        command = shlex.split(os.environ.get('CC', 'cc'))
        command += ['-std=c99', '-Wall', '-Wextra', '-Werror', '-g',
                    '-I' + str(application), '-I' + str(utility)]
        if args.sanitize:
            command += ['-fsanitize=address,undefined']
        command += [str(source) for source in sources] + ['-lm', '-o', str(binary)]
        subprocess.run(command, check=True)
        subprocess.run([str(binary)], check=True)

if __name__ == '__main__':
    main()
