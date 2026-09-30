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
    uint16_t registerReadCount[ADI_AD7124_GAIN7_REG + 1U];
} mock_transport_t;

static bool MockTransfer(void *context, uint8_t *data, size_t length)
{
    mock_transport_t *mock = (mock_transport_t *)context;
    size_t index;
    bool reset = (length == 8U);
    uint8_t address;
    uint8_t registerSize;
    uint32_t value;

    for (index = 0U; reset && (index < length); index++)
    {
        reset = (data[index] == 0xFFU);
    }
    if (reset)
    {
        mock->resetCount++;
        return true;
    }

    address = (uint8_t)(data[0] & 0x3FU);
    registerSize = ADI_AD7124_GetRegisterSize(address);
    if ((data[0] & 0x40U) == 0U)
    {
        (void)memcpy(mock->lastWrite, data, length);
        mock->lastWriteLength = length;
        value = 0U;
        for (index = 1U; index <= registerSize; index++)
        {
            value = (value << 8U) | data[index];
        }
        mock->registers[address] = value;
        return true;
    }

    value = mock->registers[address];
    mock->registerReadCount[address]++;
    if (address == ADI_AD7124_STATUS_REG)
    {
        value = mock->statusValue;
    }
    else if (address == ADI_AD7124_ID_REG)
    {
        value = ADI_AD7124_ID_4_STANDARD;
    }
    else if (address == ADI_AD7124_DATA_REG)
    {
        value = 0x123456UL;
    }
    for (index = registerSize; index > 0U; index--)
    {
        data[index] = (uint8_t)(value & 0xFFU);
        value >>= 8U;
    }
    if (length == ((size_t)registerSize + 2U))
    {
        data[registerSize + 1U] =
            ADI_AD7124_ComputeCrc8(data, registerSize + 1U);
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
    device.expectedVariant = kAdiAd7124_Variant4;
    device.pollLimit = 4U;
    mock.registers[ADI_AD7124_ERROR_REG] = 0x001234UL;

    assert(ADI_AD7124_Init(&device) == kAdiAd7124_Ok);
    assert(mock.resetCount == 1U);
    assert(mock.delayMs == 4U);
    assert(device.deviceId == ADI_AD7124_ID_4_STANDARD);
    assert(device.initialError == 0x001234UL);
    assert(mock.registerReadCount[ADI_AD7124_ERROR_REG] == 1U);
    assert(device.initialized);

    assert(ADI_AD7124_WriteRegister(
               &device, ADI_AD7124_CHANNEL0_REG, 0x8123U) ==
           kAdiAd7124_Ok);
    assert(mock.lastWriteLength == 3U);
    assert(mock.lastWrite[0] == ADI_AD7124_CHANNEL0_REG);
    assert(mock.lastWrite[1] == 0x81U);
    assert(mock.lastWrite[2] == 0x23U);

    assert(ADI_AD7124_EnableCrc(&device) == kAdiAd7124_Ok);
    assert(device.crcEnabled);
    assert((mock.registers[ADI_AD7124_ERROR_ENABLE_REG] &
            ADI_AD7124_ERROR_ENABLE_CRC_MASK) != 0U);
    assert(mock.registerReadCount[ADI_AD7124_ERROR_REG] == 1U);
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
        {0U, 2U, 5U, 0U, 384U, true, true, false, true},
        {1U, 0U, 4U, 0U, 384U, true, true, true, true}
    };
    static const adi_ad7124_channel_config_t channels[] =
    {
        {0U, 0U, 0U, 1U, true},
        {2U, 1U, 4U, 5U, true}
    };
    static const adi_ad7124_io_config_t io = {500U, 0U, 7U};

    device.transfer = MockTransfer;
    device.transportContext = &mock;
    device.initialized = true;
    assert(ADI_AD7124_Configure(&device, setups, 2U, channels, 2U, &io) ==
           kAdiAd7124_Ok);
    assert(mock.registers[ADI_AD7124_CONFIG0_REG] == 0x0875U);
    assert(mock.registers[ADI_AD7124_FILTER0_REG] == 0x100180UL);
    assert(mock.registers[ADI_AD7124_CHANNEL0_REG] == 0x8001U);
    assert(mock.registers[ADI_AD7124_CHANNEL0_REG + 2U] == 0x9085U);
    assert((mock.registers[ADI_AD7124_ADC_CONTROL_REG] & 0x0100U) != 0U);
    assert((mock.registers[ADI_AD7124_ADC_CONTROL_REG] & 0x00FFU) == 0x0080U);
    assert(mock.registers[ADI_AD7124_IO_CONTROL1_REG] == 0x0024F0U);
    assert(mock.registers[ADI_AD7124_IO_CONTROL2_REG] == 0U);
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
