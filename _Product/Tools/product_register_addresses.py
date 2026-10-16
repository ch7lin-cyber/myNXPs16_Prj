"""Shared firmware register addresses for DTX diagnostic and CALYS tools."""

ADC_DIAGNOSTIC_BASE_ADDRESS = 0x4E00
ADC_DIAGNOSTIC_DEVICE_STRIDE = 0x0040
ADC_SUCCESSFUL_SAMPLES_OFFSET = 0x11
ADC_RAW_CODE_OFFSET = 0x2F
ADC_ACTIVE_FAULTS_OFFSET = 0x03


def adc_diagnostic_address(channel, offset=0):
    if channel not in range(4):
        raise ValueError("ADC channel must be 0..3")
    if offset not in range(ADC_DIAGNOSTIC_DEVICE_STRIDE):
        raise ValueError("ADC diagnostic offset must be 0..63")
    return (ADC_DIAGNOSTIC_BASE_ADDRESS +
            channel * ADC_DIAGNOSTIC_DEVICE_STRIDE + offset)
