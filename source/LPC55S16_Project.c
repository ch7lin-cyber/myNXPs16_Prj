/*
 * Copyright 2016-2026 NXP
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * o Redistributions of source code must retain the above copyright notice, this list
 *   of conditions and the following disclaimer.
 *
 * o Redistributions in binary form must reproduce the above copyright notice, this
 *   list of conditions and the following disclaimer in the documentation and/or
 *   other materials provided with the distribution.
 *
 * o Neither the name of NXP Semiconductor, Inc. nor the names of its
 *   contributors may be used to endorse or promote products derived from this
 *   software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @file    LPC55S16_Project.c
 * @brief   Application entry point.
 */
#include <stdio.h>
#include "board.h"
#include "peripherals.h"
#include "pin_mux.h"
#include "clock_config.h"
#include "LPC55S16.h"
#include "fsl_debug_console.h"
#include "ProductFeatureConfig.h"
#include "l03_product_modbus.h"
#include "l03_product_modbus_master.h"
#include "product_application.h"
#include "product_pwm_driver.h"
#include "product_nvm_driver.h"
#include "product_rs485_driver.h"
/* TODO: insert other include files here. */

/* TODO: insert other definitions and declarations here. */

//void ctimer0_match0_callback(uint32_t flags);
//void ctimer1_match0_callback(uint32_t flags);
void ctimer2_match3_callback(uint32_t flags);
void ctimer3_match1_callback(uint32_t flags);
void COMM_TMOut_callback(uint32_t flags);
/*
void ctimer0_match0_callback(uint32_t flags)
{
    (void)flags;
}

void ctimer1_match0_callback(uint32_t flags)
{
    (void)flags;
}
*/
void ctimer0_match0_callback(uint32_t flags)
{
    (void)flags;
}

void ctimer1_match0_callback(uint32_t flags)
{
    (void)flags;
}

void ctimer2_match3_callback(uint32_t flags)
{
    (void)flags;
}

void ctimer3_match1_callback(uint32_t flags)
{
    (void)flags;
}

void COMM_TMOut_callback(uint32_t flags)
{
    /*
     * Legacy Config Tools callback retained so peripherals.c remains linkable.
     * ProductRs485Driver_Initialize() disables COMM_CTIMER4; MRT0 owns
     * communication
     * receive gaps and turnaround delays.
     */
    (void)flags;
}

void drv_internalbus_clk_callback(pint_pin_int_t pintr, uint32_t pmatch_status)
{
    (void)pintr;
    (void)pmatch_status;
}

void drv_level_detect_callback(pint_pin_int_t pintr, uint32_t pmatch_status)
{
    (void)pintr;
    (void)pmatch_status;
}

volatile uint32_t g_systemTick100us = 0U;

void SysTick_Handler(void)
{
    g_systemTick100us++;
}


/*
 * @brief   Application entry point.
 */
int main(void) {
    uint32_t processedTick100us = 0U;

    /* Init board hardware. */
    BOARD_InitBootPins();
    BOARD_InitBootClocks();
    BOARD_InitBootPeripherals();
#if (PRODUCT_FC0_BOOT_DEBUG_ENABLE != 0U)
    /* FC0 debug ownership ends when ProductRs485Driver_Initialize runs. */
    BOARD_InitDebugConsole();
    PRINTF("\r\nLPC55S16 boot: FC0 switching to Modbus Slave\r\n");
#endif

    if (!ProductRs485Driver_Initialize())
    {
        while (1) {};
    }
    if (!ProductPwmDriver_Init())
    {
        while (1) {};
    }
    if (!ProductNvmDriver_Init())
    {
        while (1) {};
    }
    if (!ProductApplication_Init())
    {
        while (1) {};
    }
    if (!L03_ProductModbus_Init() || !L03_ProductModbusMaster_Init())
    {
        while (1) {};
    }

    // use 0.1ms as base tick
    if (SysTick_Config(SystemCoreClock / 10000))
    {
        while (1) {};
    }

    /* Force the counter to be placed into memory. */
    volatile static int i = 0 ;
    /* Enter an infinite loop, just incrementing a counter. */
    while(1) {
        /*
         * Deferred RS-485 work runs outside IRQ context. A TXIDLE callback
         * requests the final post-TX delay and DIR switch back to receive.
         */
        ProductRs485Driver_Process();
        while ((uint32_t)(g_systemTick100us - processedTick100us) >= 10U)
        {
            processedTick100us += 10U;
            L03_ProductModbus_Tick1ms();
            L03_ProductModbusMaster_Tick1ms();
        }
        L03_ProductModbus_Process();
        L03_ProductModbusMaster_Process();
        ProductApplication_Process();

        i++ ;
        /* 'Dummy' NOP to allow source level single stepping of
            tight while() loop */
        __asm volatile ("nop");


        /* WWDT is disabled during hardware bring-up. */

    }
    return 0 ;
}
