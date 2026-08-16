#include "main.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "event_groups.h"
#include "portable.h"
#include <stdio.h>
#include <string.h>

#ifndef DIAG_FAULT_MODE
#define DIAG_FAULT_MODE 0
#endif

#define LOG_QUEUE_LENGTH 8U
#define TASK_STACK_WORDS 384U
#define IWDG_RELOAD_VALUE 1500U /* 约 3 秒，实际值随 LSI 频率变化 */
#define SUPERVISOR_GRACE_TICKS pdMS_TO_TICKS(2000)

/* USART1 句柄：PA9=TX、PA10=RX，供 logger 任务和启动日志使用 */
UART_HandleTypeDef huart1;
IWDG_HandleTypeDef hiwdg;

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
    char text[128];
} log_message_t;

static QueueHandle_t log_queue;           /* health -> logger，容量 8 条 */
static EventGroupHandle_t health_events;  /* 跨任务共享的健康状态位 */
static TaskHandle_t health_task_handle;
static TaskHandle_t logger_task_handle;
static TaskHandle_t ui_task_handle;
static volatile uint32_t log_queue_dropped;
static volatile uint32_t logger_heartbeat;
static volatile uint32_t ui_heartbeat;

static void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_IWDG_Init(void);
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

    const uint32_t iwdg_reset = (__HAL_RCC_GET_FLAG(RCC_FLAG_IWDGRST) != RESET) ? 1U : 0U;
    __HAL_RCC_CLEAR_RESET_FLAGS();

    /* 调度器尚未启动，这里直接写串口，证明时钟和 USART1 已可用 */
    uart_write("\r\n[BOOT] freertos_robot_diagnostic\r\n");
    uart_write("[BOOT] STM32F407ZGT6 168 MHz, USART1 115200\r\n");
    uart_write(iwdg_reset != 0U ? "[BOOT] reset cause=IWDG\r\n"
                               : "[BOOT] reset cause=other\r\n");

    log_queue = xQueueCreate(LOG_QUEUE_LENGTH, sizeof(log_message_t));
    health_events = xEventGroupCreate();
    if ((log_queue == NULL) || (health_events == NULL)) {
        Error_Handler();        /* Heap 不足或创建失败，停机便于排查 */
    }

    /*
     * 三个最小任务：health 采状态，logger 统一出串口，ui 为后续 LCD 预留。
     * 栈 384 word = 1536 字节；优先级数字越大越高（health=4, logger=3, ui=1）。
     */
    if (xTaskCreate(health_task, "health", TASK_STACK_WORDS, NULL, 4,
                    &health_task_handle) != pdPASS ||
        xTaskCreate(logger_task, "logger", TASK_STACK_WORDS, NULL, 3,
                    &logger_task_handle) != pdPASS ||
        xTaskCreate(ui_task, "ui", TASK_STACK_WORDS, NULL, 1,
                    &ui_task_handle) != pdPASS) {
        Error_Handler();
    }

    MX_IWDG_Init();
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
#if DIAG_FAULT_MODE == 1 || DIAG_FAULT_MODE == 2
    uint8_t fault_injected = 0U;
#endif
    uint32_t last_logger_heartbeat = 0U;
    uint32_t last_ui_heartbeat = 0U;
    for (;;) {
        EventBits_t bits = xEventGroupGetBits(health_events);
        UBaseType_t health_hwm = uxTaskGetStackHighWaterMark(health_task_handle);
        UBaseType_t logger_hwm = uxTaskGetStackHighWaterMark(logger_task_handle);
        UBaseType_t ui_hwm = uxTaskGetStackHighWaterMark(ui_task_handle);
        size_t free_heap = xPortGetFreeHeapSize();
        size_t min_free_heap = xPortGetMinimumEverFreeHeapSize();
        TickType_t now = xTaskGetTickCount();
        const char *supervisor = "ok";
        if ((now >= SUPERVISOR_GRACE_TICKS) &&
            (logger_heartbeat == last_logger_heartbeat)) {
            supervisor = "logger_stalled";
        } else if ((now >= SUPERVISOR_GRACE_TICKS) &&
                   (ui_heartbeat == last_ui_heartbeat)) {
            supervisor = "ui_stalled";
        }
        snprintf(message.text, sizeof(message.text),
                 "health bits=0x%02lx heap=%lu min_heap=%lu qdrop=%lu "
                 "stack_hwm=%lu/%lu/%lu supervisor=%s wd=ok",
                 (unsigned long)bits,
                 (unsigned long)free_heap,
                 (unsigned long)min_free_heap,
                 (unsigned long)log_queue_dropped,
                 (unsigned long)health_hwm,
                 (unsigned long)logger_hwm,
                 (unsigned long)ui_hwm,
                 supervisor);
        message.tick = now;
        /* 超时 20 ms：队列满时丢弃本条，避免 health 被 logger 拖死 */
        if (xQueueSend(log_queue, &message, pdMS_TO_TICKS(20)) != pdPASS) {
            log_queue_dropped++;
        }

#if DIAG_FAULT_MODE == 1
        /* 队列故障注入：第 5 秒快速塞入消息，验证满队列计数。 */
        if ((fault_injected == 0U) && (xTaskGetTickCount() >= pdMS_TO_TICKS(5000))) {
            fault_injected = 1U;
            for (uint32_t i = 0U; i < 32U; ++i) {
                if (xQueueSend(log_queue, &message, 0U) != pdPASS) {
                    log_queue_dropped++;
                }
            }
        }
#elif DIAG_FAULT_MODE == 2
        /* 看门狗故障注入：第 5 秒停止刷新，预期约 3 秒后自动复位。 */
        if ((fault_injected == 0U) && (xTaskGetTickCount() >= pdMS_TO_TICKS(5000))) {
            fault_injected = 1U;
            uart_write("[FAULT] watchdog refresh stopped\r\n");
            for (;;) {
            }
        }
#endif

        if (strcmp(supervisor, "ok") != 0) {
            uart_write("[SUPERVISOR] task heartbeat stalled; withholding IWDG refresh\r\n");
            for (;;) {
            }
        }

        if (HAL_IWDG_Refresh(&hiwdg) != HAL_OK) {
            Error_Handler();
        }
        last_logger_heartbeat = logger_heartbeat;
        last_ui_heartbeat = ui_heartbeat;
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
    char line[192];
    xEventGroupSetBits(health_events, EVT_LOGGER_OK);
    for (;;) {
#if DIAG_FAULT_MODE == 3
        if (xTaskGetTickCount() >= pdMS_TO_TICKS(5000)) {
            uart_write("[FAULT] logger heartbeat stopped\r\n");
            for (;;) {
            }
        }
#endif
        if (xQueueReceive(log_queue, &message, portMAX_DELAY) == pdPASS) {
            snprintf(line, sizeof(line), "[%10lu] %s\r\n",
                     (unsigned long)message.tick, message.text);
            uart_write(line);
            logger_heartbeat++;
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
        ui_heartbeat++;
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

/* IWDG 使用 LSI，独立于主时钟；health_task 每秒刷新一次。 */
static void MX_IWDG_Init(void)
{
    hiwdg.Instance = IWDG;
    hiwdg.Init.Prescaler = IWDG_PRESCALER_64;
    hiwdg.Init.Reload = IWDG_RELOAD_VALUE;
    if (HAL_IWDG_Init(&hiwdg) != HAL_OK) {
        Error_Handler();
    }
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
