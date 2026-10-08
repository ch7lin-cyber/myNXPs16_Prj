"""Host regression tests for calibration targets, persistence and Modbus slots.
Run: python _Product/Tests/run_factory_calibration_tests.py (requires GCC).
"""
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
PLATFORM = ROOT / '_Modules/PlatformCore'
PRODUCT = ROOT / '_Product'
includes = sorted({str(p.parent) for p in PLATFORM.rglob('*.h')} |
                  {str(p.parent) for p in PRODUCT.rglob('*.h') if 'Tests' not in p.parts})
def platform_sources(names):
    return [str(next(PLATFORM.rglob(n + '.c'))) for n in names]
def product_source(name):
    return str(next(PRODUCT.rglob(name + '.c')))
suites = {
    'storage': platform_sources(['FactoryCalibrationService', 'FactoryModeService', 'HalAdcMeasurement']) +
        [product_source('product_factory_calibration_storage'), product_source('test_product_factory_calibration_storage')],
    'fram': platform_sources(['FactoryCalibrationService', 'FactoryModeService', 'HalAdcMeasurement', 'HalNvm']) +
        [product_source('product_fram_bank_test'), product_source('test_product_fram_bank_test')],
    'conversion': platform_sources(['test_sensor_conversion_service', 'SensorConversionService',
        'PiecewiseLinearTable', 'FactoryCalibrationService', 'FactoryModeService', 'HalAdcMeasurement']),
    'hal': platform_sources(['test_adc_conversion_diagnostics', 'HalAdcMeasurement']),
    'measurement': platform_sources(['SensorConversionService', 'PiecewiseLinearTable', 'HalAdcMeasurement']) +
        [str(p) for p in (PLATFORM / 'L03_SystemPlatform/SystemService/AnalogInputService/SensorTables').glob('*.c')] +
        [product_source('product_sensor_measurement_service'), product_source('test_product_sensor_measurement_service')],
    'modbus': platform_sources(['EventService', 'FaultService', 'SafetyService', 'SnapshotService',
        'SystemEventService', 'WarningService', 'SystemFaultService', 'PwmOutputService', 'DigitalInputService',
        'DigitalOutputService', 'FactoryCalibrationService', 'ModbusRegisterAdapter', 'HalPwm', 'HalDac',
        'HalGpio', 'HalAdcMeasurement', 'FactoryModeService', 'SerialConfiguration', 'HalNvm', 'NvmService', 'HalSerial']) +
        [product_source(n) for n in ['bsp_analog_output', 'product_fram_bank_test', 'product_temperature_range_resolver',
          'product_modbus_register_adapter', 'product_factory_calibration_storage', 'test_product_modbus_register_adapter']],
}
with tempfile.TemporaryDirectory(prefix='factory-tests-') as directory:
    for name, sources in suites.items():
        for mode in ([1, 0] if name == 'modbus' else [1]):
            output = str(Path(directory) / (name + str(mode)))
            flags = ['-DFACTORY_CALIBRATION_INPUT_COUNT=4'] if name != 'conversion' else []
            cmd = ['gcc', '-std=c11', '-Os', '-Wall', '-Wextra', '-Werror', '-ffunction-sections',
                   '-fdata-sections', '-Wl,--gc-sections', '-DPRODUCT_ADC_DEBUG_ENABLE=' + str(mode)]
            subprocess.run(cmd + flags + ['-I' + p for p in includes] + sources + ['-lm', '-o', output], check=True)
            subprocess.run([output], check=True)
            print(f'{name} DEBUG={mode}: PASS', flush=True)
