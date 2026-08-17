#ifndef STM32F4XX_IT_H
#define STM32F4XX_IT_H

/* Cortex-M4 内核异常与 SysTick，实现见 stm32f4xx_it.c */
void NMI_Handler(void);
void HardFault_Handler(void);
void MemManage_Handler(void);
void BusFault_Handler(void);
void UsageFault_Handler(void);
void DebugMon_Handler(void);
void SysTick_Handler(void);
void EXTI1_IRQHandler(void);

#endif
