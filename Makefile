TARGET = freertos_robot_diagnostic
BUILD_DIR = Build

C_SOURCES = \
Core/Src/main.c Core/Src/stm32f4xx_it.c Core/Src/stm32f4xx_hal_msp.c \
Core/Src/syscalls.c Core/Src/system_stm32f4xx.c \
Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal.c \
Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_cortex.c \
Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_dma.c \
Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_flash.c \
Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_flash_ex.c \
Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_gpio.c \
Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_pwr.c \
Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_pwr_ex.c \
Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_rcc.c \
Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_rcc_ex.c \
Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_uart.c \
Middlewares/FreeRTOS/Source/croutine.c Middlewares/FreeRTOS/Source/event_groups.c \
Middlewares/FreeRTOS/Source/list.c Middlewares/FreeRTOS/Source/queue.c \
Middlewares/FreeRTOS/Source/stream_buffer.c Middlewares/FreeRTOS/Source/tasks.c \
Middlewares/FreeRTOS/Source/timers.c \
Middlewares/FreeRTOS/portable/GCC/ARM_CM4F/port.c \
Middlewares/FreeRTOS/portable/MemMang/heap_4.c

ASM_SOURCES = Core/Startup/startup_stm32f407xx.s
PREFIX = arm-none-eabi-
CC = $(PREFIX)gcc
AS = $(PREFIX)gcc -x assembler-with-cpp
CP = $(PREFIX)objcopy
SZ = $(PREFIX)size
MCU = -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard
C_DEFS = -DUSE_HAL_DRIVER -DSTM32F407xx
C_INCLUDES = -ICore/Inc -IDrivers/STM32F4xx_HAL_Driver/Inc \
-IDrivers/STM32F4xx_HAL_Driver/Inc/Legacy -IDrivers/CMSIS/Device/ST/STM32F4xx/Include \
-IDrivers/CMSIS/Include -IMiddlewares/FreeRTOS/include \
-IMiddlewares/FreeRTOS/portable/GCC/ARM_CM4F
CFLAGS = $(MCU) $(C_DEFS) $(C_INCLUDES) -O1 -g3 -Wall -Wextra \
-ffunction-sections -fdata-sections -fstack-usage -std=gnu11 -MMD -MP
ASFLAGS = $(MCU) $(C_DEFS) $(C_INCLUDES) -g3
LDFLAGS = $(MCU) -specs=nano.specs -TSTM32F407ZGTX_FLASH.ld \
-Wl,-Map=$(BUILD_DIR)/$(TARGET).map,--cref -Wl,--gc-sections -lc -lm -lnosys
OBJECTS = $(addprefix $(BUILD_DIR)/,$(notdir $(C_SOURCES:.c=.o)))
vpath %.c $(sort $(dir $(C_SOURCES)))
OBJECTS += $(addprefix $(BUILD_DIR)/,$(notdir $(ASM_SOURCES:.s=.o)))
vpath %.s $(sort $(dir $(ASM_SOURCES)))

all: $(BUILD_DIR)/$(TARGET).elf $(BUILD_DIR)/$(TARGET).bin
	$(SZ) $(BUILD_DIR)/$(TARGET).elf
$(BUILD_DIR)/%.o: %.c Makefile | $(BUILD_DIR)
	$(CC) -c $(CFLAGS) $< -o $@
$(BUILD_DIR)/%.o: %.s Makefile | $(BUILD_DIR)
	$(AS) -c $(ASFLAGS) $< -o $@
$(BUILD_DIR)/$(TARGET).elf: $(OBJECTS) Makefile
	$(CC) $(OBJECTS) $(LDFLAGS) -o $@
$(BUILD_DIR)/$(TARGET).bin: $(BUILD_DIR)/$(TARGET).elf
	$(CP) -O binary $< $@
$(BUILD_DIR):
	mkdir -p $@
clean:
	rm -rf $(BUILD_DIR)
-include $(wildcard $(BUILD_DIR)/*.d)
.PHONY: all clean
