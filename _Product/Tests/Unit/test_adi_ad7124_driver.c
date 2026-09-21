#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "adi_ad7124_driver.h"

typedef struct _mock_transport
{
    uint32_t resetCount;
    uint32_t delayMs;
    uint8_t statusValue;
    uint8_t lastWrite[8];
    size_t lastWriteLength;
    uint32_t registers[ADI_AD7124_GAIN7_REG + 1U];
} mock_transport_t;

static bool MockTransfer(void *context, uint8_t *data, size_t length)
{
    mock_transport_t *mock = (mock_transport_t *)context;
    size_t index;
    bool reset = (length == 8U);

    for (index = 0U; reset && (index < length); index++)
    {
        reset = (data[index] == 0xFFU);
    }
    if (reset)
    {
        mock->resetCount++;
        return true;
    }

    (void)memcpy(mock->lastWrite, data, length);
    mock->lastWriteLength = length;
    if ((data[0] < 0x40U) && (data[0] <= ADI_AD7124_GAIN7_REG))
    {
        uint32_t value = 0U;
        for (index = 1U; index < length; index++)
        {
            value = (value << 8U) | data[index];
        }
        mock->registers[data[0]] = value;
    }
    else if (data[0] == 0x40U)
    {
        data[1] = mock->statusValue;
    }
    else if (data[0] == 0x45U)
    {
        data[1] = ADI_AD7124_ID_8_STANDARD;
    }
    else if (data[0] == 0x42U)
    {
        data[1] = 0x12U;
        data[2] = 0x34U;
        data[3] = 0x56U;
    }
    else if ((data[0] == (0x40U | ADI_AD7124_ADC_CONTROL_REG)) &&
             (length == 3U))
    {
        data[1] = (uint8_t)(mock->registers[ADI_AD7124_ADC_CONTROL_REG] >> 8U);
        data[2] = (uint8_t)mock->registers[ADI_AD7124_ADC_CONTROL_REG];
    }
    return true;
}

static void MockDelay(void *context, uint32_t delayMs)
{
    ((mock_transport_t *)context)->delayMs = delayMs;
}

static void TestRegisterSizes(void)
{
    assert(ADI_AD7124_GetRegisterSize(ADI_AD7124_STATUS_REG) == 1U);
    assert(ADI_AD7124_GetRegisterSize(ADI_AD7124_ADC_CONTROL_REG) == 2U);
    assert(ADI_AD7124_GetRegisterSize(ADI_AD7124_DATA_REG) == 3U);
    assert(ADI_AD7124_GetRegisterSize(ADI_AD7124_CHANNEL15_REG) == 2U);
    assert(ADI_AD7124_GetRegisterSize(ADI_AD7124_GAIN7_REG) == 3U);
    assert(ADI_AD7124_GetRegisterSize(0x39U) == 0U);
}

static void TestCrc(void)
{
    static const uint8_t check[] = "123456789";
    assert(ADI_AD7124_ComputeCrc8(check, sizeof(check) - 1U) == 0xF4U);
}

static void TestInitAndWrite(void)
{
    mock_transport_t mock = {0U};
    adi_ad7124_device_t device = {0U};

    device.transfer = MockTransfer;
    device.delayMs = MockDelay;
    device.transportContext = &mock;
    device.expectedVariant = kAdiAd7124_Variant8;
    device.pollLimit = 4U;

    assert(ADI_AD7124_Init(&device) == kAdiAd7124_Ok);
    assert(mock.resetCount == 1U);
    assert(mock.delayMs == 4U);
    assert(device.deviceId == ADI_AD7124_ID_8_STANDARD);
    assert(device.initialized);

    assert(ADI_AD7124_WriteRegister(
               &device, ADI_AD7124_CHANNEL0_REG, 0x8123U) ==
           kAdiAd7124_Ok);
    assert(mock.lastWriteLength == 3U);
    assert(mock.lastWrite[0] == ADI_AD7124_CHANNEL0_REG);
    assert(mock.lastWrite[1] == 0x81U);
    assert(mock.lastWrite[2] == 0x23U);
}

static void TestNonBlockingRead(void)
{
    mock_transport_t mock = {0U};
    adi_ad7124_device_t device = {0U};
    uint32_t code;
    uint8_t channel;

    device.transfer = MockTransfer;
    device.transportContext = &mock;

    mock.statusValue = 0x80U;
    assert(ADI_AD7124_TryReadData(&device, &code, &channel) ==
           kAdiAd7124_NotReady);

    mock.statusValue = 0x03U;
    assert(ADI_AD7124_TryReadData(&device, &code, &channel) ==
           kAdiAd7124_Ok);
    assert(code == 0x123456UL);
    assert(channel == 3U);
}

static void TestConfigure(void)
{
    mock_transport_t mock = {0U};
    adi_ad7124_device_t device = {0U};
    static const adi_ad7124_setup_config_t setups[] =
    {
        {0U, 2U, 5U, 0U, 384U, true, true, false},
        {1U, 0U, 4U, 0U, 384U, true, true, true}
    };
    static const adi_ad7124_channel_config_t channels[] =
    {
        {0U, 0U, 0U, 1U, true},
        {2U, 1U, 4U, 5U, true}
    };

    device.transfer = MockTransfer;
    device.transportContext = &mock;
    device.initialized = true;
    assert(ADI_AD7124_Configure(&device, setups, 2U, channels, 2U) ==
           kAdiAd7124_Ok);
    assert(mock.registers[ADI_AD7124_CONFIG0_REG] == 0x0875U);
    assert(mock.registers[ADI_AD7124_FILTER0_REG] == 384U);
    assert(mock.registers[ADI_AD7124_CHANNEL0_REG] == 0x8001U);
    assert(mock.registers[ADI_AD7124_CHANNEL0_REG + 2U] == 0x9085U);
    assert((mock.registers[ADI_AD7124_ADC_CONTROL_REG] & 0x0100U) != 0U);
}

int main(void)
{
    TestRegisterSizes();
    TestCrc();
    TestInitAndWrite();
    TestNonBlockingRead();
    TestConfigure();
    return 0;
}
