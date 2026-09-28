################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../component/i2c/fsl_adapter_flexcomm_i2c.c 

C_DEPS += \
./component/i2c/fsl_adapter_flexcomm_i2c.d 

OBJS += \
./component/i2c/fsl_adapter_flexcomm_i2c.o 


# Each subdirectory must supply rules for building sources it contributes
component/i2c/%.o: ../component/i2c/%.c component/i2c/subdir.mk
	@echo 'Building file: $<'
	@echo 'Invoking: MCU C Compiler'
	arm-none-eabi-gcc -D__REDLIB__ -DCPU_LPC55S16JBD100 -DCPU_LPC55S16JBD100_cm33 -DSDK_OS_BAREMETAL -DSDK_DEBUGCONSOLE=1 -DCR_INTEGER_PRINTF -DPRINTF_FLOAT_ENABLE=0 -DSDK_OS_FREE_RTOS -DSERIAL_PORT_TYPE_UART=1 -DSERIAL_PORT_TYPE_VIRTUAL=1 -DDEBUG_CONSOLE_TRANSFER_NON_BLOCKING -D__MCUXPRESSO -D__USE_CMSIS -DDEBUG -I"E:\myGitHub2\myNXPs16_Prj\board" -I"E:\myGitHub2\myNXPs16_Prj\source" -I"E:\myGitHub2\myNXPs16_Prj\freertos\freertos-kernel\include" -I"E:\myGitHub2\myNXPs16_Prj\drivers" -I"E:\myGitHub2\myNXPs16_Prj\LPC55S16\drivers" -I"E:\myGitHub2\myNXPs16_Prj\CMSIS_driver" -I"E:\myGitHub2\myNXPs16_Prj\device" -I"E:\myGitHub2\myNXPs16_Prj\CMSIS" -I"E:\myGitHub2\myNXPs16_Prj\component\serial_manager" -I"E:\myGitHub2\myNXPs16_Prj\utilities" -I"E:\myGitHub2\myNXPs16_Prj\component\uart" -I"E:\myGitHub2\myNXPs16_Prj\usb\phy" -I"E:\myGitHub2\myNXPs16_Prj\usb\include" -I"E:\myGitHub2\myNXPs16_Prj\component\i2c" -I"E:\myGitHub2\myNXPs16_Prj\component\gpio" -I"E:\myGitHub2\myNXPs16_Prj\component\pwm" -I"E:\myGitHub2\myNXPs16_Prj\component\audio" -I"E:\myGitHub2\myNXPs16_Prj\component\lists" -I"E:\myGitHub2\myNXPs16_Prj\component\osa" -I"E:\myGitHub2\myNXPs16_Prj\freertos\freertos-kernel\portable\GCC\ARM_CM33_NTZ\non_secure" -I"E:\myGitHub2\myNXPs16_Prj\source\generated" -O0 -fno-common -g3 -Wall -c -ffunction-sections -fdata-sections -ffreestanding -fno-builtin -fmerge-constants -fmacro-prefix-map="$(<D)/"= -mcpu=cortex-m33 -mfpu=fpv5-sp-d16 -mfloat-abi=hard -mthumb -D__REDLIB__ -fstack-usage -specs=redlib.specs -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.o)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


clean: clean-component-2f-i2c

clean-component-2f-i2c:
	-$(RM) ./component/i2c/fsl_adapter_flexcomm_i2c.d ./component/i2c/fsl_adapter_flexcomm_i2c.o

.PHONY: clean-component-2f-i2c

