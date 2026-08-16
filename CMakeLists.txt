cmake_minimum_required(VERSION 3.25)

project(freertos_robot_diagnostic C ASM)

set(TARGET_NAME freertos_robot_diagnostic)
set(DIAG_FAULT_MODE "none" CACHE STRING "Fault injection mode: none, queue, or watchdog")
set_property(CACHE DIAG_FAULT_MODE PROPERTY STRINGS none queue watchdog logger)
if(DIAG_FAULT_MODE STREQUAL "none")
    set(DIAG_FAULT_MODE_VALUE 0)
elseif(DIAG_FAULT_MODE STREQUAL "queue")
    set(DIAG_FAULT_MODE_VALUE 1)
elseif(DIAG_FAULT_MODE STREQUAL "watchdog")
    set(DIAG_FAULT_MODE_VALUE 2)
elseif(DIAG_FAULT_MODE STREQUAL "logger")
    set(DIAG_FAULT_MODE_VALUE 3)
else()
    message(FATAL_ERROR "DIAG_FAULT_MODE must be none, queue, watchdog, or logger")
endif()

set(MCU_FLAGS
    -mcpu=cortex-m4
    -mthumb
    -mfpu=fpv4-sp-d16
    -mfloat-abi=hard
)

set(FIRMWARE_SOURCES
    Core/Src/main.c
    Core/Src/stm32f4xx_it.c
    Core/Src/stm32f4xx_hal_msp.c
    Core/Src/syscalls.c
    Core/Src/system_stm32f4xx.c
    Core/Startup/startup_stm32f407xx.s
    Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal.c
    Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_cortex.c
    Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_dma.c
    Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_flash.c
    Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_flash_ex.c
    Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_gpio.c
    Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_iwdg.c
    Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_pwr.c
    Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_pwr_ex.c
    Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_rcc.c
    Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_rcc_ex.c
    Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_uart.c
    Middlewares/FreeRTOS/Source/croutine.c
    Middlewares/FreeRTOS/Source/event_groups.c
    Middlewares/FreeRTOS/Source/list.c
    Middlewares/FreeRTOS/Source/queue.c
    Middlewares/FreeRTOS/Source/stream_buffer.c
    Middlewares/FreeRTOS/Source/tasks.c
    Middlewares/FreeRTOS/Source/timers.c
    Middlewares/FreeRTOS/portable/GCC/ARM_CM4F/port.c
    Middlewares/FreeRTOS/portable/MemMang/heap_4.c
)

add_executable(${TARGET_NAME} ${FIRMWARE_SOURCES})
set_target_properties(${TARGET_NAME} PROPERTIES SUFFIX ".elf")

target_compile_definitions(${TARGET_NAME} PRIVATE
    USE_HAL_DRIVER
    STM32F407xx
    DIAG_FAULT_MODE=${DIAG_FAULT_MODE_VALUE}
)

target_include_directories(${TARGET_NAME} PRIVATE
    Core/Inc
    Drivers/STM32F4xx_HAL_Driver/Inc
    Drivers/STM32F4xx_HAL_Driver/Inc/Legacy
    Drivers/CMSIS/Device/ST/STM32F4xx/Include
    Drivers/CMSIS/Include
    Middlewares/FreeRTOS/include
    Middlewares/FreeRTOS/portable/GCC/ARM_CM4F
)

target_compile_options(${TARGET_NAME} PRIVATE
    ${MCU_FLAGS}
    -ffunction-sections
    -fdata-sections
    -fstack-usage
    -Wall
    -Wextra
    $<$<CONFIG:Debug>:-Og;-g3>
    $<$<CONFIG:Release>:-O2;-g1>
)

target_link_options(${TARGET_NAME} PRIVATE
    ${MCU_FLAGS}
    --specs=nano.specs
    -T${CMAKE_SOURCE_DIR}/STM32F407ZGTX_FLASH.ld
    -Wl,-Map=${CMAKE_BINARY_DIR}/${TARGET_NAME}.map,--cref
    -Wl,--gc-sections
)

target_link_libraries(${TARGET_NAME} PRIVATE c m nosys)

add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary
            $<TARGET_FILE:${TARGET_NAME}>
            ${CMAKE_BINARY_DIR}/${TARGET_NAME}.bin
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:${TARGET_NAME}>
    COMMENT "Generating BIN and printing firmware size"
)

find_program(STM32_PROGRAMMER
    NAMES STM32_Programmer_CLI.exe STM32_Programmer_CLI
    HINTS "$ENV{STM32_PROGRAMMER_BIN}"
)

if(STM32_PROGRAMMER)
    add_custom_target(probe
        COMMAND ${STM32_PROGRAMMER} -l st-link
        USES_TERMINAL
        COMMENT "Listing connected ST-LINK probes"
    )
    add_custom_target(flash
        COMMAND ${STM32_PROGRAMMER}
                -c port=SWD mode=UR
                -w $<TARGET_FILE:${TARGET_NAME}>
                -v -rst
        DEPENDS ${TARGET_NAME}
        USES_TERMINAL
        COMMENT "Flashing ${TARGET_NAME} through ST-LINK"
    )
else()
    message(WARNING "STM32_Programmer_CLI was not found; probe/flash targets are unavailable.")
endif()
