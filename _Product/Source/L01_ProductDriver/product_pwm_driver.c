#include "product_pwm_driver.h"

#include <stddef.h>
#include <stdint.h>

#include "HalPwm.h"
#include "fsl_clock.h"
#include "fsl_ctimer.h"
#include "peripherals.h"

#define PRODUCT_PWM_CHANNEL_COUNT             (4U)
#define PRODUCT_PWM_COUNTER_FREQUENCY_HZ      (100000UL)
#define PRODUCT_PWM_TICKS_PER_MILLISECOND     \
    (PRODUCT_PWM_COUNTER_FREQUENCY_HZ / 1000UL)
#define PRODUCT_PWM_DEFAULT_PERIOD_MS         (10UL)
#define PRODUCT_PWM_MATCH_INTERRUPT_MASK      \
    (kCTIMER_Match0InterruptEnable |          \
     kCTIMER_Match1InterruptEnable |          \
     kCTIMER_Match2InterruptEnable |          \
     kCTIMER_Match3InterruptEnable)

typedef struct
{
    CTIMER_Type *base;
    ctimer_match_t period_channel;
    ctimer_match_t pulse_channel;
    uint8_t timer_index;
    uint32_t period_ticks;
    uint16_t duty_permille;
} ProductPwmDriverContext_t;

static ProductPwmDriverContext_t g_pwm_context[PRODUCT_PWM_CHANNEL_COUNT] =
{
    {OUT1_CTIMER0_PERIPHERAL, OUT1_CTIMER0_PWM_PERIOD_CH,
     kCTIMER_Match_0, 0U, 0U, 0U},
    {OUT2_CTIMER1_PERIPHERAL, OUT2_CTIMER1_PWM_PERIOD_CH,
     kCTIMER_Match_0, 1U, 0U, 0U},
    {OUT3_CTIMER3_PERIPHERAL, OUT3_CTIMER3_PWM_PERIOD_CH,
     kCTIMER_Match_1, 3U, 0U, 0U},
    {OUT4_CTIMER2_PERIPHERAL, OUT4_CTIMER2_PWM_PERIOD_CH,
     kCTIMER_Match_3, 2U, 0U, 0U}
};

static uint32_t ProductPwmCalculatePulseMatch(
    uint32_t period_ticks,
    uint16_t duty_permille)
{
    uint32_t period_match = period_ticks - 1U;

    if (duty_permille == 0U)
    {
        return period_ticks;
    }
    return (uint32_t)((
        ((uint64_t)period_match *
         (uint64_t)(HAL_PWM_DUTY_MAX_PERMILLE - duty_permille)) + 500ULL) /
        1000ULL);
}

static HalPwmStatus_t ProductPwmApplyImmediate(
    ProductPwmDriverContext_t *context,
    uint32_t period_ticks)
{
    uint32_t pulse_match = ProductPwmCalculatePulseMatch(
        period_ticks, context->duty_permille);

    CTIMER_StopTimer(context->base);
    CTIMER_EnableMatchChannelReload(
        context->base, context->period_channel, false);
    CTIMER_EnableMatchChannelReload(
        context->base, context->pulse_channel, false);
    if (CTIMER_SetupPwmPeriod(
            context->base,
            context->period_channel,
            context->pulse_channel,
            period_ticks - 1U,
            pulse_match,
            false) != kStatus_Success)
    {
        return HAL_PWM_STATUS_IO_ERROR;
    }
    CTIMER_Reset(context->base);
    CTIMER_StartTimer(context->base);
    context->period_ticks = period_ticks;
    return HAL_PWM_STATUS_OK;
}

static HalPwmStatus_t ProductPwmInitialize(void *driver_context)
{
    ProductPwmDriverContext_t *context =
        (ProductPwmDriverContext_t *)driver_context;
    uint32_t source_frequency;
    uint32_t prescale_divider;

    if (context == NULL)
    {
        return HAL_PWM_STATUS_INVALID_ARGUMENT;
    }

    source_frequency = CLOCK_GetCTimerClkFreq(context->timer_index);
    if ((source_frequency < PRODUCT_PWM_COUNTER_FREQUENCY_HZ) ||
        ((source_frequency % PRODUCT_PWM_COUNTER_FREQUENCY_HZ) != 0U))
    {
        return HAL_PWM_STATUS_IO_ERROR;
    }
    prescale_divider = source_frequency / PRODUCT_PWM_COUNTER_FREQUENCY_HZ;

    CTIMER_StopTimer(context->base);
    context->base->PR = prescale_divider - 1U;
    context->duty_permille = 0U;
    CTIMER_DisableInterrupts(context->base,
                             PRODUCT_PWM_MATCH_INTERRUPT_MASK);
    return ProductPwmApplyImmediate(
        context,
        PRODUCT_PWM_DEFAULT_PERIOD_MS * PRODUCT_PWM_TICKS_PER_MILLISECOND);
}

static HalPwmStatus_t ProductPwmSetDuty(
    void *driver_context,
    uint16_t duty_permille)
{
    ProductPwmDriverContext_t *context =
        (ProductPwmDriverContext_t *)driver_context;
    uint32_t pulse_match;

    if ((context == NULL) || (context->period_ticks == 0U) ||
        (duty_permille > HAL_PWM_DUTY_MAX_PERMILLE))
    {
        return HAL_PWM_STATUS_INVALID_ARGUMENT;
    }

    context->duty_permille = duty_permille;
    pulse_match = ProductPwmCalculatePulseMatch(
        context->period_ticks, duty_permille);
    CTIMER_UpdatePwmPulsePeriod(context->base,
                               context->pulse_channel,
                               pulse_match);
    CTIMER_SetShadowValue(context->base,
                         context->pulse_channel,
                         pulse_match);
    return HAL_PWM_STATUS_OK;
}

static HalPwmStatus_t ProductPwmSetPeriod(
    void *driver_context,
    uint32_t period_ms,
    HalPwmPeriodUpdateMode_t update_mode)
{
    ProductPwmDriverContext_t *context =
        (ProductPwmDriverContext_t *)driver_context;
    uint32_t period_ticks;
    uint32_t pulse_match;

    if ((context == NULL) || (period_ms == 0U) ||
        ((update_mode != HAL_PWM_PERIOD_UPDATE_IMMEDIATE) &&
         (update_mode != HAL_PWM_PERIOD_UPDATE_NEXT_CYCLE)))
    {
        return HAL_PWM_STATUS_INVALID_ARGUMENT;
    }
    if (period_ms > (UINT32_MAX / PRODUCT_PWM_TICKS_PER_MILLISECOND))
    {
        return HAL_PWM_STATUS_INVALID_ARGUMENT;
    }

    period_ticks = period_ms * PRODUCT_PWM_TICKS_PER_MILLISECOND;
    if (update_mode == HAL_PWM_PERIOD_UPDATE_IMMEDIATE)
    {
        return ProductPwmApplyImmediate(context, period_ticks);
    }

    pulse_match = ProductPwmCalculatePulseMatch(
        period_ticks, context->duty_permille);
    CTIMER_SetShadowValue(context->base,
                         context->period_channel,
                         period_ticks - 1U);
    CTIMER_SetShadowValue(context->base,
                         context->pulse_channel,
                         pulse_match);
    CTIMER_EnableMatchChannelReload(
        context->base, context->period_channel, true);
    CTIMER_EnableMatchChannelReload(
        context->base, context->pulse_channel, true);
    context->period_ticks = period_ticks;
    return HAL_PWM_STATUS_OK;
}

bool ProductPwmDriver_Init(void)
{
    static const HalPwmDriverOps_t ops =
    {
        ProductPwmInitialize,
        ProductPwmSetDuty,
        ProductPwmSetPeriod
    };
    uint8_t channel;

    for (channel = 0U; channel < PRODUCT_PWM_CHANNEL_COUNT; channel++)
    {
        if (HalPwm_RegisterDriver(
                channel, &ops, &g_pwm_context[channel]) != HAL_PWM_STATUS_OK)
        {
            return false;
        }
    }
    return true;
}
