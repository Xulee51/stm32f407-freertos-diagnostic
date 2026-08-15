set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

if(NOT DEFINED ENV{ARM_GCC_BIN})
    message(FATAL_ERROR "ARM_GCC_BIN is not set. Run: source tools/env.sh")
endif()

file(TO_CMAKE_PATH "$ENV{ARM_GCC_BIN}" ARM_GCC_BIN)

set(CMAKE_C_COMPILER "${ARM_GCC_BIN}/arm-none-eabi-gcc.exe")
set(CMAKE_ASM_COMPILER "${ARM_GCC_BIN}/arm-none-eabi-gcc.exe")
set(CMAKE_OBJCOPY "${ARM_GCC_BIN}/arm-none-eabi-objcopy.exe" CACHE FILEPATH "objcopy")
set(CMAKE_SIZE "${ARM_GCC_BIN}/arm-none-eabi-size.exe" CACHE FILEPATH "size")

set(CMAKE_C_COMPILER_WORKS TRUE)
set(CMAKE_ASM_COMPILER_WORKS TRUE)
