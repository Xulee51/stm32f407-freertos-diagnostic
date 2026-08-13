#ifndef STM32F4XX_HAL_CONF_H
#define STM32F4XX_HAL_CONF_H

/* 当前基线只用到时钟、GPIO、UART 及相关支撑模块 */
#define HAL_MODULE_ENABLED
#define HAL_CORTEX_MODULE_ENABLED
#define HAL_DMA_MODULE_ENABLED
#define HAL_FLASH_MODULE_ENABLED
#define HAL_GPIO_MODULE_ENABLED
#define HAL_PWR_MODULE_ENABLED
#define HAL_RCC_MODULE_ENABLED
#define HAL_UART_MODULE_ENABLED

#define HSE_VALUE              8000000U    /* 开发板外部晶振 8 MHz */
#define HSE_STARTUP_TIMEOUT    100U
#define HSI_VALUE              16000000U   /* 内部高速振荡器 */
#define LSI_VALUE              32000U
#define LSE_VALUE              32768U
#define LSE_STARTUP_TIMEOUT    5000U
#define EXTERNAL_CLOCK_VALUE   12288000U
#define VDD_VALUE              3300U       /* 供电 3.3 V */
#define TICK_INT_PRIORITY      15U         /* HAL Tick 用最低优先级，避免抢占 FreeRTOS */
#define USE_RTOS               0U          /* 不使用 HAL 自带 RTOS 封装，由应用直接调 FreeRTOS */
#define PREFETCH_ENABLE        1U
#define INSTRUCTION_CACHE_ENABLE 1U
#define DATA_CACHE_ENABLE      1U

#include "stm32f4xx_hal_rcc.h"
#include "stm32f4xx_hal_gpio.h"
#include "stm32f4xx_hal_dma.h"
#include "stm32f4xx_hal_cortex.h"
#include "stm32f4xx_hal_flash.h"
#include "stm32f4xx_hal_pwr.h"
#include "stm32f4xx_hal_uart.h"

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line);
#define assert_param(expr) ((expr) ? (void)0U : assert_failed((uint8_t *)__FILE__, __LINE__))
#else
#define assert_param(expr) ((void)0U)
#endif

#endif
