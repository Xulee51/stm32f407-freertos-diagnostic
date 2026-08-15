#ifndef MAIN_H
#define MAIN_H

#include "stm32f4xx_hal.h"

/* 全局 USART1 句柄，供 main.c 与 newlib 的 _write() 共用 */
extern UART_HandleTypeDef huart1;

/* 不可恢复错误入口：关中断后停机 */
void Error_Handler(void);

#endif
