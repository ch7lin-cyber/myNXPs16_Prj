################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../drivers/fsl_anactrl.c \
../drivers/fsl_casper.c \
../drivers/fsl_clock.c \
../drivers/fsl_cmp.c \
../drivers/fsl_common.c \
../drivers/fsl_common_arm.c \
../drivers/fsl_crc.c \
../drivers/fsl_ctimer.c \
../drivers/fsl_dma.c \
../drivers/fsl_flexcomm.c \
../drivers/fsl_gint.c \
../drivers/fsl_gpio.c \
../drivers/fsl_hashcrypt.c \
../drivers/fsl_i2c.c \
../drivers/fsl_i2c_dma.c \
../drivers/fsl_i2s.c \
../drivers/fsl_i2s_dma.c \
../drivers/fsl_iap.c \
../drivers/fsl_inputmux.c \
../drivers/fsl_lpadc.c \
../drivers/fsl_mcan.c \
../drivers/fsl_mrt.c \
../drivers/fsl_ostimer.c \
../drivers/fsl_pint.c \
../drivers/fsl_plu.c \
../drivers/fsl_power.c \
../drivers/fsl_puf.c \
../drivers/fsl_reset.c \
../drivers/fsl_rng.c \
../drivers/fsl_rtc.c \
../drivers/fsl_sctimer.c \
../drivers/fsl_spi.c \
../drivers/fsl_spi_cmsis.c \
../drivers/fsl_spi_dma.c \
../drivers/fsl_sysctl.c \
../drivers/fsl_usart.c \
../drivers/fsl_usart_cmsis.c \
../drivers/fsl_usart_dma.c \
../drivers/fsl_utick.c \
../drivers/fsl_wwdt.c 

C_DEPS += \
./drivers/fsl_anactrl.d \
./drivers/fsl_casper.d \
./drivers/fsl_clock.d \
./drivers/fsl_cmp.d \
./drivers/fsl_common.d \
./drivers/fsl_common_arm.d \
./drivers/fsl_crc.d \
./drivers/fsl_ctimer.d \
./drivers/fsl_dma.d \
./drivers/fsl_flexcomm.d \
./drivers/fsl_gint.d \
./drivers/fsl_gpio.d \
./drivers/fsl_hashcrypt.d \
./drivers/fsl_i2c.d \
./drivers/fsl_i2c_dma.d \
./drivers/fsl_i2s.d \
./drivers/fsl_i2s_dma.d \
./drivers/fsl_iap.d \
./drivers/fsl_inputmux.d \
./drivers/fsl_lpadc.d \
./drivers/fsl_mcan.d \
./drivers/fsl_mrt.d \
./drivers/fsl_ostimer.d \
./drivers/fsl_pint.d \
./drivers/fsl_plu.d \
./drivers/fsl_power.d \
./drivers/fsl_puf.d \
./drivers/fsl_reset.d \
./drivers/fsl_rng.d \
./drivers/fsl_rtc.d \
./drivers/fsl_sctimer.d \
./drivers/fsl_spi.d \
./drivers/fsl_spi_cmsis.d \
./drivers/fsl_spi_dma.d \
./drivers/fsl_sysctl.d \
./drivers/fsl_usart.d \
./drivers/fsl_usart_cmsis.d \
./drivers/fsl_usart_dma.d \
./drivers/fsl_utick.d \
./drivers/fsl_wwdt.d 

OBJS += \
./drivers/fsl_anactrl.o \
./drivers/fsl_casper.o \
./drivers/fsl_clock.o \
./drivers/fsl_cmp.o \
./drivers/fsl_common.o \
./drivers/fsl_common_arm.o \
./drivers/fsl_crc.o \
./drivers/fsl_ctimer.o \
./drivers/fsl_dma.o \
./drivers/fsl_flexcomm.o \
./drivers/fsl_gint.o \
./drivers/fsl_gpio.o \
./drivers/fsl_hashcrypt.o \
./drivers/fsl_i2c.o \
./drivers/fsl_i2c_dma.o \
./drivers/fsl_i2s.o \
./drivers/fsl_i2s_dma.o \
./drivers/fsl_iap.o \
./drivers/fsl_inputmux.o \
./drivers/fsl_lpadc.o \
./drivers/fsl_mcan.o \
./drivers/fsl_mrt.o \
./drivers/fsl_ostimer.o \
./drivers/fsl_pint.o \
./drivers/fsl_plu.o \
./drivers/fsl_power.o \
./drivers/fsl_puf.o \
./drivers/fsl_reset.o \
./drivers/fsl_rng.o \
./drivers/fsl_rtc.o \
./drivers/fsl_sctimer.o \
./drivers/fsl_spi.o \
./drivers/fsl_spi_cmsis.o \
./drivers/fsl_spi_dma.o \
./drivers/fsl_sysctl.o \
./drivers/fsl_usart.o \
./drivers/fsl_usart_cmsis.o \
./drivers/fsl_usart_dma.o \
./drivers/fsl_utick.o \
./drivers/fsl_wwdt.o 


# Each subdirectory must supply rules for building sources it contributes
drivers/%.o: ../drivers/%.c drivers/subdir.mk
	@echo 'Building file: $<'
	@echo 'Invoking: MCU C Compiler'
	arm-none-eabi-gcc -D__REDLIB__ -DCPU_LPC55S16JBD100 -DCPU_LPC55S16JBD100_cm33 -DSDK_OS_BAREMETAL -DSDK_DEBUGCONSOLE=1 -DCR_INTEGER_PRINTF -DPRINTF_FLOAT_ENABLE=0 -DSDK_OS_FREE_RTOS -DSERIAL_PORT_TYPE_UART=1 -DSERIAL_PORT_TYPE_VIRTUAL=1 -DDEBUG_CONSOLE_TRANSFER_NON_BLOCKING -D__MCUXPRESSO -D__USE_CMSIS -DDEBUG -I"E:\myGitHub2\myNXPs16_Prj\board" -I"E:\myGitHub2\myNXPs16_Prj\source" -I"E:\myGitHub2\myNXPs16_Prj\freertos\freertos-kernel\include" -I"E:\myGitHub2\myNXPs16_Prj\drivers" -I"E:\myGitHub2\myNXPs16_Prj\LPC55S16\drivers" -I"E:\myGitHub2\myNXPs16_Prj\CMSIS_driver" -I"E:\myGitHub2\myNXPs16_Prj\device" -I"E:\myGitHub2\myNXPs16_Prj\CMSIS" -I"E:\myGitHub2\myNXPs16_Prj\component\serial_manager" -I"E:\myGitHub2\myNXPs16_Prj\utilities" -I"E:\myGitHub2\myNXPs16_Prj\component\uart" -I"E:\myGitHub2\myNXPs16_Prj\usb\phy" -I"E:\myGitHub2\myNXPs16_Prj\usb\include" -I"E:\myGitHub2\myNXPs16_Prj\component\i2c" -I"E:\myGitHub2\myNXPs16_Prj\component\gpio" -I"E:\myGitHub2\myNXPs16_Prj\component\pwm" -I"E:\myGitHub2\myNXPs16_Prj\component\audio" -I"E:\myGitHub2\myNXPs16_Prj\component\lists" -I"E:\myGitHub2\myNXPs16_Prj\component\osa" -I"E:\myGitHub2\myNXPs16_Prj\freertos\freertos-kernel\portable\GCC\ARM_CM33_NTZ\non_secure" -I"E:\myGitHub2\myNXPs16_Prj\source\generated" -O0 -fno-common -g3 -Wall -c -ffunction-sections -fdata-sections -ffreestanding -fno-builtin -fmerge-constants -fmacro-prefix-map="$(<D)/"= -mcpu=cortex-m33 -mfpu=fpv5-sp-d16 -mfloat-abi=hard -mthumb -D__REDLIB__ -fstack-usage -specs=redlib.specs -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.o)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


clean: clean-drivers

clean-drivers:
	-$(RM) ./drivers/fsl_anactrl.d ./drivers/fsl_anactrl.o ./drivers/fsl_casper.d ./drivers/fsl_casper.o ./drivers/fsl_clock.d ./drivers/fsl_clock.o ./drivers/fsl_cmp.d ./drivers/fsl_cmp.o ./drivers/fsl_common.d ./drivers/fsl_common.o ./drivers/fsl_common_arm.d ./drivers/fsl_common_arm.o ./drivers/fsl_crc.d ./drivers/fsl_crc.o ./drivers/fsl_ctimer.d ./drivers/fsl_ctimer.o ./drivers/fsl_dma.d ./drivers/fsl_dma.o ./drivers/fsl_flexcomm.d ./drivers/fsl_flexcomm.o ./drivers/fsl_gint.d ./drivers/fsl_gint.o ./drivers/fsl_gpio.d ./drivers/fsl_gpio.o ./drivers/fsl_hashcrypt.d ./drivers/fsl_hashcrypt.o ./drivers/fsl_i2c.d ./drivers/fsl_i2c.o ./drivers/fsl_i2c_dma.d ./drivers/fsl_i2c_dma.o ./drivers/fsl_i2s.d ./drivers/fsl_i2s.o ./drivers/fsl_i2s_dma.d ./drivers/fsl_i2s_dma.o ./drivers/fsl_iap.d ./drivers/fsl_iap.o ./drivers/fsl_inputmux.d ./drivers/fsl_inputmux.o ./drivers/fsl_lpadc.d ./drivers/fsl_lpadc.o ./drivers/fsl_mcan.d ./drivers/fsl_mcan.o ./drivers/fsl_mrt.d ./drivers/fsl_mrt.o ./drivers/fsl_ostimer.d ./drivers/fsl_ostimer.o ./drivers/fsl_pint.d ./drivers/fsl_pint.o ./drivers/fsl_plu.d ./drivers/fsl_plu.o ./drivers/fsl_power.d ./drivers/fsl_power.o ./drivers/fsl_puf.d ./drivers/fsl_puf.o ./drivers/fsl_reset.d ./drivers/fsl_reset.o ./drivers/fsl_rng.d ./drivers/fsl_rng.o ./drivers/fsl_rtc.d ./drivers/fsl_rtc.o ./drivers/fsl_sctimer.d ./drivers/fsl_sctimer.o ./drivers/fsl_spi.d ./drivers/fsl_spi.o ./drivers/fsl_spi_cmsis.d ./drivers/fsl_spi_cmsis.o ./drivers/fsl_spi_dma.d ./drivers/fsl_spi_dma.o ./drivers/fsl_sysctl.d ./drivers/fsl_sysctl.o ./drivers/fsl_usart.d ./drivers/fsl_usart.o ./drivers/fsl_usart_cmsis.d ./drivers/fsl_usart_cmsis.o ./drivers/fsl_usart_dma.d ./drivers/fsl_usart_dma.o ./drivers/fsl_utick.d ./drivers/fsl_utick.o ./drivers/fsl_wwdt.d ./drivers/fsl_wwdt.o

.PHONY: clean-drivers

