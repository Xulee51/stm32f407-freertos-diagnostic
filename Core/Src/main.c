#include "main.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "event_groups.h"
#include <stdio.h>
#include <string.h>

/* USART1 句柄：PA9=TX、PA10=RX，供 logger 任务和启动日志使用 */
UART_HandleTypeDef huart1;

/*
 * 健康事件位：每个模块启动成功后置位对应 bit。
 * 串口输出 0x07 表示 boot/logger/ui 三个位都已置位。
 */
enum {
    EVT_BOOT_OK = 1U << 0,    /* main() 创建完对象后置位 */
    EVT_LOGGER_OK = 1U << 1,  /* logger 任务进入循环前置位 */
    EVT_UI_OK = 1U << 2       /* ui 任务进入循环前置位 */
};

/* 任务间日志消息：tick 为发送时刻，text 为负载（不含换行） */
typedef struct {
    uint32_t tick;
    char text[48];
} log_message_t;

static QueueHandle_t log_queue;           /* health -> logger，容量 8 条 */
static EventGroupHandle_t health_events;  /* 跨任务共享的健康状态位 */

static void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART1_UART_Init(void);
static void logger_task(void *argument);
static void health_task(void *argument);
static void ui_task(void *argument);
static void uart_write(const char *text);

int main(void)
{
    HAL_Init();                 /* 复位外设、配置 Flash、启动 HAL Tick */
    SystemClock_Config();       /* 8 MHz HSE -> PLL -> 168 MHz SYSCLK */
    MX_GPIO_Init();
    MX_USART1_UART_Init();

    /* 调度器尚未启动，这里直接写串口，证明时钟和 USART1 已可用 */
    uart_write("\r\n[BOOT] freertos_robot_diagnostic\r\n");
    uart_write("[BOOT] STM32F407ZGT6 168 MHz, USART1 115200\r\n");

    log_queue = xQueueCreate(8, sizeof(log_message_t));
    health_events = xEventGroupCreate();
    if ((log_queue == NULL) || (health_events == NULL)) {
        Error_Handler();        /* Heap 不足或创建失败，停机便于排查 */
    }

    /*
     * 三个最小任务：health 采状态，logger 统一出串口，ui 为后续 LCD 预留。
     * 栈 384 word = 1536 字节；优先级数字越大越高（health=4, logger=3, ui=1）。
     */
    if (xTaskCreate(health_task, "health", 384, NULL, 4, NULL) != pdPASS ||
        xTaskCreate(logger_task, "logger", 384, NULL, 3, NULL) != pdPASS ||
        xTaskCreate(ui_task, "ui", 384, NULL, 1, NULL) != pdPASS) {
        Error_Handler();
    }

    xEventGroupSetBits(health_events, EVT_BOOT_OK);
    vTaskStartScheduler();      /* 正常情况下不会返回 */
    Error_Handler();            /* 若 Heap 不够创建 Idle/Timer 任务才会走到这里 */
}

/*
 * 健康监控任务（优先级最高）。
 * 不直接操作串口，而是把快照放入队列，由 logger 统一输出。
 * 使用 vTaskDelayUntil 保证约 1 秒一次，周期相对稳定。
 */
static void health_task(void *argument)
{
    (void)argument;
    TickType_t wake = xTaskGetTickCount();
    log_message_t message;
    for (;;) {
        EventBits_t bits = xEventGroupGetBits(health_events);
        snprintf(message.text, sizeof(message.text),
                 "health bits=0x%02lx heap=%lu",
                 (unsigned long)bits,
                 (unsigned long)xPortGetFreeHeapSize());
        message.tick = xTaskGetTickCount();
        /* 超时 20 ms：队列满时丢弃本条，避免 health 被 logger 拖死 */
        (void)xQueueSend(log_queue, &message, pdMS_TO_TICKS(20));
        vTaskDelayUntil(&wake, pdMS_TO_TICKS(1000));
    }
}

/*
 * 日志任务：阻塞等待队列，收到后格式化并经 USART1 发出。
 * 调度器启动后的周期日志集中在此，后续加 LCD/网口时不必改 health。
 */
static void logger_task(void *argument)
{
    (void)argument;
    log_message_t message;
    char line[96];
    xEventGroupSetBits(health_events, EVT_LOGGER_OK);
    for (;;) {
        if (xQueueReceive(log_queue, &message, portMAX_DELAY) == pdPASS) {
            snprintf(line, sizeof(line), "[%10lu] %s\r\n",
                     (unsigned long)message.tick, message.text);
            uart_write(line);
        }
    }
}

/*
 * UI 占位任务：当前只置位 EVT_UI_OK 并周期休眠。
 * ILI9806 / CST716 等显示与触摸适配，等 RTOS 基线验收后再加入。
 */
static void ui_task(void *argument)
{
    (void)argument;
    xEventGroupSetBits(health_events, EVT_UI_OK);
    for (;;) {
        /* ILI9806/CST716 adapters are added only after the RTOS baseline passes. */
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

static void uart_write(const char *text)
{
    (void)HAL_UART_Transmit(&huart1, (const uint8_t *)text,
                            (uint16_t)strlen(text), 100U);
}

/* USART1：115200-8-N-1，引脚在 HAL_UART_MspInit() 中配置 */
static void MX_USART1_UART_Init(void)
{
    huart1.Instance = USART1;
    huart1.Init.BaudRate = 115200;
    huart1.Init.WordLength = UART_WORDLENGTH_8B;
    huart1.Init.StopBits = UART_STOPBITS_1;
    huart1.Init.Parity = UART_PARITY_NONE;
    huart1.Init.Mode = UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling = UART_OVERSAMPLING_16;
    if (HAL_UART_Init(&huart1) != HAL_OK) {
        Error_Handler();
    }
}

static void MX_GPIO_Init(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();  /* USART1 的 PA9/PA10 所在端口 */
}

/*
 * 时钟树（开发板 8 MHz 外部晶振）：
 *   HSE 8 MHz / PLLM=8 * PLLN=336 / PLLP=2 = 168 MHz SYSCLK
 *   AHB  = 168 MHz
 *   APB1 = 42 MHz（定时器时钟为 84 MHz）
 *   APB2 = 84 MHz（USART1 挂在 APB2）
 * FLASH_LATENCY_5：168 MHz 下 Flash 等待周期。
 */
static void SystemClock_Config(void)
{
    RCC_OscInitTypeDef osc = {0};
    RCC_ClkInitTypeDef clk = {0};

    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    osc.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    osc.HSEState = RCC_HSE_ON;
    osc.PLL.PLLState = RCC_PLL_ON;
    osc.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    osc.PLL.PLLM = 8;
    osc.PLL.PLLN = 336;
    osc.PLL.PLLP = RCC_PLLP_DIV2;
    osc.PLL.PLLQ = 7;
    if (HAL_RCC_OscConfig(&osc) != HAL_OK) {
        Error_Handler();
    }

    clk.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                    RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    clk.AHBCLKDivider = RCC_SYSCLK_DIV1;
    clk.APB1CLKDivider = RCC_HCLK_DIV4;
    clk.APB2CLKDivider = RCC_HCLK_DIV2;
    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_5) != HAL_OK) {
        Error_Handler();
    }
}

/* 严重错误停机：关中断后空转，后续可改为保存现场并串口输出 */
void Error_Handler(void)
{
    __disable_irq();
    for (;;) { }
}

void vApplicationMallocFailedHook(void)
{
    Error_Handler();  /* FreeRTOS Heap 分配失败（configUSE_MALLOC_FAILED_HOOK=1） */
}

void vApplicationStackOverflowHook(TaskHandle_t task, char *name)
{
    (void)task;
    (void)name;
    Error_Handler();  /* 任务栈溢出（configCHECK_FOR_STACK_OVERFLOW=2） */
}
