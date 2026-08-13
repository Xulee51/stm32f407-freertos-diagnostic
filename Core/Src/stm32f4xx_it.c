#include "stm32f4xx_it.h"
#include "main.h"
#include "FreeRTOS.h"
#include "task.h"

extern void xPortSysTickHandler(void);

void NMI_Handler(void) { }
void HardFault_Handler(void) { while (1) { } }   /* 非法访问、错误异常等，当前空转停机 */
void MemManage_Handler(void) { while (1) { } }   /* MPU / 存储器保护故障 */
void BusFault_Handler(void) { while (1) { } }    /* 总线访问故障 */
void UsageFault_Handler(void) { while (1) { } }  /* 未定义指令、对齐错误等 */
void DebugMon_Handler(void) { }

/*
 * 系统节拍：1 kHz。
 * HAL 需要 HAL_IncTick() 做超时；调度器启动后再把同一拍交给 FreeRTOS。
 * 启动阶段调度器尚未运行，不能调用 xPortSysTickHandler()。
 */
void SysTick_Handler(void)
{
    HAL_IncTick();
    if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) {
        xPortSysTickHandler();
    }
}
