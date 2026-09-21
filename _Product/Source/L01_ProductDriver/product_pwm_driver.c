#include "product_pwm_driver.h"

#include <stddef.h>
#include <stdint.h>

#include "HalPwm.h"
#include "fsl_clock.h"
#include "fsl_ctimer.h"
#include "peripherals.h"

#define PRODUCT_PWM_COUNTER_FREQUENCY_HZ    (100000UL)
#define PRODUCT_PWM_OUTPUT_FREQUENCY_HZ     (100UL)
#define PRODUCT_PWM_PERIOD_TICKS            \
    (PRODUCT_PWM_COUNTER_FREQUENCY_HZ / PRODUCT_PWM_OUTPUT_FREQUENCY_HZ)

typedef struct
{
    CTIMER_Type *base;
    ctimer_match_t period_channel;
    ctimer_match_t pulse_channel;
    uint8_t timer_index;
} ProductPwmDriverContext_t;

static ProductPwmDriverContext_t g_out1_context =
{
    OUT1_CTIMER0_PERIPHERAL,
    OUT1_CTIMER0_PWM_PERIOD_CH,
    kCTIMER_Match_0,
    0U
};

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
    if (CTIMER_SetupPwmPeriod(
            context->base,
            context->period_channel,
            context->pulse_channel,
            PRODUCT_PWM_PERIOD_TICKS - 1U,
            PRODUCT_PWM_PERIOD_TICKS,
            false) != kStatus_Success)
    {
        return HAL_PWM_STATUS_IO_ERROR;
    }
    CTIMER_StartTimer(context->base);
    return HAL_PWM_STATUS_OK;
}

static HalPwmStatus_t ProductPwmSetDuty(
    void *driver_context,
    uint16_t duty_permille)
{
    ProductPwmDriverContext_t *context =
        (ProductPwmDriverContext_t *)driver_context;
    uint32_t pulse_tick;

    if ((context == NULL) ||
        (duty_permille > HAL_PWM_DUTY_MAX_PERMILLE))
    {
        return HAL_PWM_STATUS_INVALID_ARGUMENT;
    }

    pulse_tick = PRODUCT_PWM_PERIOD_TICKS - (uint32_t)duty_permille;
    CTIMER_UpdatePwmPulsePeriod(context->base,
                               context->pulse_channel,
                               pulse_tick);
    return HAL_PWM_STATUS_OK;
}

bool ProductPwmDriver_Init(void)
{
    static const HalPwmDriverOps_t ops =
    {
        ProductPwmInitialize,
        ProductPwmSetDuty
    };

    return HalPwm_RegisterDriver(0U, &ops, &g_out1_context) ==
           HAL_PWM_STATUS_OK;
}
