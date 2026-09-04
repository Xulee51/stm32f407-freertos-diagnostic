#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

#include <stdint.h>
extern uint32_t SystemCoreClock;

#define configUSE_PREEMPTION                    1   /* 抢占式调度：高优先级任务就绪后立即抢占 */
#define configUSE_TIME_SLICING                   1   /* 同优先级任务按时间片轮转 */
#define configCPU_CLOCK_HZ                       SystemCoreClock
#define configTICK_RATE_HZ                       ((TickType_t)1000)  /* 1 Tick = 1 ms */
#define configMAX_PRIORITIES                     7   /* 可用优先级 0..6，数字越大越高 */
#define configMINIMAL_STACK_SIZE                 ((uint16_t)128)
#define configTOTAL_HEAP_SIZE                    ((size_t)(32 * 1024))  /* heap_4.c 动态内存池 */
#define configMAX_TASK_NAME_LEN                  16
#define configUSE_16_BIT_TICKS                   0   /* 32 位 Tick，避免约 65 秒回绕 */
#define configIDLE_SHOULD_YIELD                  1
#define configUSE_MUTEXES                        1
#define configUSE_RECURSIVE_MUTEXES              0
#define configUSE_COUNTING_SEMAPHORES            1
#define configUSE_TASK_NOTIFICATIONS             1
#define configSUPPORT_STATIC_ALLOCATION          1  /* 节点 10：静态 StreamBuffer 需要 */
#define configSUPPORT_DYNAMIC_ALLOCATION         1  /* 现有 Queue/任务仍用 heap */
#define configUSE_QUEUE_SETS                     0
#define configUSE_TIMERS                         1
#define configTIMER_TASK_PRIORITY                2
#define configTIMER_QUEUE_LENGTH                 8
#define configTIMER_TASK_STACK_DEPTH             256
#define configCHECK_FOR_STACK_OVERFLOW           2   /* 方法 2：任务切换时检查栈指针及栈边界填充值 */
#define configUSE_MALLOC_FAILED_HOOK             1   /* 分配失败调用 vApplicationMallocFailedHook */
#define configUSE_IDLE_HOOK                      0
#define configUSE_TICK_HOOK                      0
#define configUSE_TRACE_FACILITY                 1   /* 启用任务状态和跟踪相关功能 */
#define configUSE_STATS_FORMATTING_FUNCTIONS     1
#define configGENERATE_RUN_TIME_STATS            0

/*
 * Cortex-M4 NVIC：优先级寄存器高 4 位有效。
 * 数值越大优先级越低。内核 SysTick/PendSV 用最低优先级 15；
 * 允许调用 FreeRTOS API 的 ISR 优先级不得高于 5（数字更小）。
 */
#define configPRIO_BITS                          4
#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY  15
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY 5
#define configKERNEL_INTERRUPT_PRIORITY          (configLIBRARY_LOWEST_INTERRUPT_PRIORITY << (8 - configPRIO_BITS))
#define configMAX_SYSCALL_INTERRUPT_PRIORITY     (configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << (8 - configPRIO_BITS))

#define INCLUDE_vTaskPrioritySet                 1
#define INCLUDE_uxTaskPriorityGet                1
#define INCLUDE_vTaskDelete                      1
#define INCLUDE_vTaskSuspend                     1
#define INCLUDE_vTaskDelayUntil                  1
#define INCLUDE_vTaskDelay                       1
#define INCLUDE_xTaskGetSchedulerState           1
#define INCLUDE_uxTaskGetStackHighWaterMark      1  /* 后续测量各任务栈余量 */

/*
 * 把 FreeRTOS 端口处理函数映射到启动文件中的 CMSIS 向量名。
 * SysTick 不在此映射：由 stm32f4xx_it.c 先喂 HAL Tick，再转给内核。
 */
#define vPortSVCHandler                          SVC_Handler
#define xPortPendSVHandler                       PendSV_Handler
#endif
