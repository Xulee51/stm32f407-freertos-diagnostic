#include "main.h"
#include "lcd_ili9806.h"
#include "cst716.h"
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
#define UI_EVENT_QUEUE_LENGTH 8U
#define TASK_STACK_WORDS 384U
#define INPUT_TASK_STACK_WORDS 256U
#define IWDG_RELOAD_VALUE 1500U /* 约 3 秒，实际值随 LSI 频率变化 */
#define SUPERVISOR_GRACE_TICKS pdMS_TO_TICKS(2000)
#define UI_REFRESH_TICKS pdMS_TO_TICKS(500)

#define LCD_COLOR_BLUE 0x001FU
#define LCD_COLOR_WHITE 0xFFFFU
#define LCD_COLOR_CYAN 0x07FFU

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
    EVT_UI_OK = 1U << 2,      /* ui 任务进入循环前置位 */
    EVT_INPUT_OK = 1U << 3    /* input 任务进入循环前置位 */
};

typedef enum {
    UI_EVENT_TOUCH_PRESS = 1U,
    UI_EVENT_TOUCH_RELEASE = 2U
} ui_event_type_t;

typedef struct {
    TickType_t tick;
    ui_event_type_t type;
    uint16_t x;
    uint16_t y;
    uint8_t fingers;
} ui_event_t;

/* 任务间日志消息：tick 为发送时刻，text 为负载（不含换行） */
typedef struct {
    uint32_t tick;
    char text[128];
} log_message_t;

static QueueHandle_t log_queue;           /* health -> logger，容量 8 条 */
static QueueHandle_t ui_event_queue;      /* input -> ui，容量 8 个事件 */
static EventGroupHandle_t health_events;  /* 跨任务共享的健康状态位 */
static TaskHandle_t health_task_handle;
static TaskHandle_t logger_task_handle;
static TaskHandle_t input_task_handle;
static TaskHandle_t ui_task_handle;
static volatile uint32_t log_queue_dropped;
static volatile uint32_t ui_event_dropped;
static volatile uint32_t logger_heartbeat;
static volatile uint32_t input_heartbeat;
static volatile uint32_t ui_heartbeat;
static volatile uint32_t touch_irq_count;
static uint8_t lcd_ready;
static uint8_t touch_ready;

static void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_IWDG_Init(void);
static void logger_task(void *argument);
static void health_task(void *argument);
static void input_task(void *argument);
static void ui_task(void *argument);
static void uart_write(const char *text);
static void ui_render_status(const ui_event_t *last_event,
                             uint32_t event_count);

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

    /* 节点 2：在调度器启动前完成 LCD 总线初始化，便于串口报告失败层。 */
    if (LCD_ILI9806_Init() == HAL_OK) {
        lcd_ready = LCD_ILI9806_IsReady();
        LCD_ILI9806_ShowTestPattern();
        uart_write("[LCD] ILI9806 FSMC init OK, test pattern written\r\n");
    } else {
        uart_write("[LCD] ILI9806 FSMC init FAILED\r\n");
    }

    /* 节点 3：保持软件 I2C 轮询，先验证 CST716 总线和坐标格式。 */
    uint16_t touch_version = 0U;
    if (CST716_Init(&touch_version) == HAL_OK) {
        touch_ready = 1U;
        char touch_line[64];
        snprintf(touch_line, sizeof(touch_line),
                 "[TP] CST716 init OK version=0x%04x exti=both+poll\r\n",
                 touch_version);
        uart_write(touch_line);
    } else {
        uart_write("[TP] CST716 init FAILED\r\n");
    }

    log_queue = xQueueCreate(LOG_QUEUE_LENGTH, sizeof(log_message_t));
    ui_event_queue = xQueueCreate(UI_EVENT_QUEUE_LENGTH, sizeof(ui_event_t));
    health_events = xEventGroupCreate();
    if ((log_queue == NULL) || (ui_event_queue == NULL) ||
        (health_events == NULL)) {
        Error_Handler();        /* Heap 不足或创建失败，停机便于排查 */
    }

    /*
     * 四个任务：health 采状态，logger 统一出串口，input 采集触摸，
     * ui 独占 LCD 并消费 UI 事件。输入和显示分开，便于定位阻塞来源。
     * 普通任务栈 384 word，input 使用 256 word；优先级为 health=4、
     * logger=3、input=2、ui=1。
     */
    if (xTaskCreate(health_task, "health", TASK_STACK_WORDS, NULL, 4,
                    &health_task_handle) != pdPASS ||
        xTaskCreate(logger_task, "logger", TASK_STACK_WORDS, NULL, 3,
                    &logger_task_handle) != pdPASS ||
        xTaskCreate(input_task, "input", INPUT_TASK_STACK_WORDS, NULL, 2,
                    &input_task_handle) != pdPASS ||
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
    uint32_t last_input_heartbeat = 0U;
    uint32_t last_ui_heartbeat = 0U;
    for (;;) {
        EventBits_t bits = xEventGroupGetBits(health_events);
        UBaseType_t health_hwm = uxTaskGetStackHighWaterMark(health_task_handle);
        UBaseType_t logger_hwm = uxTaskGetStackHighWaterMark(logger_task_handle);
        UBaseType_t input_hwm = uxTaskGetStackHighWaterMark(input_task_handle);
        UBaseType_t ui_hwm = uxTaskGetStackHighWaterMark(ui_task_handle);
        size_t free_heap = xPortGetFreeHeapSize();
        size_t min_free_heap = xPortGetMinimumEverFreeHeapSize();
        TickType_t now = xTaskGetTickCount();
        const char *supervisor = "ok";
        if ((now >= SUPERVISOR_GRACE_TICKS) &&
            (logger_heartbeat == last_logger_heartbeat)) {
            supervisor = "logger_stalled";
        } else if ((now >= SUPERVISOR_GRACE_TICKS) &&
                   (input_heartbeat == last_input_heartbeat)) {
            supervisor = "input_stalled";
        } else if ((now >= SUPERVISOR_GRACE_TICKS) &&
                   (ui_heartbeat == last_ui_heartbeat)) {
            supervisor = "ui_stalled";
        }
        snprintf(message.text, sizeof(message.text),
                 "health bits=0x%02lx heap=%lu min_heap=%lu qdrop=%lu "
                 "edrop=%lu tirq=%lu stack_hwm=%lu/%lu/%lu/%lu "
                 "supervisor=%s wd=ok",
                 (unsigned long)bits,
                 (unsigned long)free_heap,
                 (unsigned long)min_free_heap,
                 (unsigned long)log_queue_dropped,
                 (unsigned long)ui_event_dropped,
                 (unsigned long)touch_irq_count,
                 (unsigned long)health_hwm,
                 (unsigned long)logger_hwm,
                 (unsigned long)input_hwm,
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
        last_input_heartbeat = input_heartbeat;
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
 * 输入任务：只负责 CST716 采样和触摸状态沿检测。
 * EXTI 只唤醒本任务，真正的 I2C 访问仍在任务上下文中完成。
 */
static void input_task(void *argument)
{
    (void)argument;
    CST716_TouchSample sample;
    ui_event_t event;
    uint8_t was_pressed = 0U;
    uint16_t last_x = 0U;
    uint16_t last_y = 0U;

    xEventGroupSetBits(health_events, EVT_INPUT_OK);
    for (;;) {
        /* 50 ms 超时是回退轮询；PB1 EXTI 到来时会提前唤醒。 */
        (void)ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(50));
        if (touch_ready != 0U && CST716_Poll(&sample) == HAL_OK) {
            if (sample.pressed != 0U && was_pressed == 0U) {
                event.tick = xTaskGetTickCount();
                event.type = UI_EVENT_TOUCH_PRESS;
                event.x = sample.x;
                event.y = sample.y;
                event.fingers = sample.fingers;
                last_x = sample.x;
                last_y = sample.y;
                if (xQueueSend(ui_event_queue, &event, 0U) != pdPASS) {
                    ui_event_dropped++;
                }
            } else if (sample.pressed == 0U && was_pressed != 0U) {
                event.tick = xTaskGetTickCount();
                event.type = UI_EVENT_TOUCH_RELEASE;
                event.x = last_x;
                event.y = last_y;
                event.fingers = 0U;
                if (xQueueSend(ui_event_queue, &event, 0U) != pdPASS) {
                    ui_event_dropped++;
                }
            }
            was_pressed = sample.pressed;
        }
        input_heartbeat++;
    }
}

/* UI 任务独占 LCD，同时消费 input_task 产生的事件队列。 */
static void ui_task(void *argument)
{
    (void)argument;
    ui_event_t event;
    ui_event_t last_event = {0};
    TickType_t next_refresh = xTaskGetTickCount();
    uint32_t event_count = 0U;

    if (lcd_ready != 0U) {
        xEventGroupSetBits(health_events, EVT_UI_OK);
    }
    for (;;) {
        TickType_t now = xTaskGetTickCount();
        TickType_t wait_ticks = (next_refresh > now) ?
                                (next_refresh - now) : 0U;
        if (xQueueReceive(ui_event_queue, &event, wait_ticks) == pdPASS) {
            log_message_t message;
            last_event = event;
            event_count++;
            message.tick = event.tick;
            if (event.type == UI_EVENT_TOUCH_PRESS) {
                snprintf(message.text, sizeof(message.text),
                         "touch press x=%u y=%u fingers=%u",
                         event.x, event.y, event.fingers);
            } else {
                snprintf(message.text, sizeof(message.text),
                         "touch release x=%u y=%u", event.x, event.y);
            }
            if (xQueueSend(log_queue, &message, 0U) != pdPASS) {
                log_queue_dropped++;
            }
        }

        now = xTaskGetTickCount();
        if (now >= next_refresh) {
            ui_render_status(&last_event, event_count);
            next_refresh = now + UI_REFRESH_TICKS;
        }
        ui_heartbeat++;
    }
}

static void ui_render_status(const ui_event_t *last_event,
                             uint32_t event_count)
{
    char line[32];
    EventBits_t bits = xEventGroupGetBits(health_events);
    uint16_t x = (last_event != NULL) ? last_event->x : 0U;
    uint16_t y = (last_event != NULL) ? last_event->y : 0U;
    uint32_t type = (last_event != NULL) ? (uint32_t)last_event->type : 0U;

    if (lcd_ready == 0U) {
        return;
    }

    /* Only redraw the status panel; the startup test pattern remains above. */
    LCD_ILI9806_FillRect(20U, 210U, LCD_ILI9806_WIDTH - 21U,
                         600U, LCD_COLOR_BLUE);
    LCD_ILI9806_DrawText(28U, 220U, "RTOS", LCD_COLOR_WHITE, 3U);

    snprintf(line, sizeof(line), "HEALTH%02lu", (unsigned long)(bits & 0x0FU));
    LCD_ILI9806_DrawText(28U, 270U, line, LCD_COLOR_CYAN, 3U);
    snprintf(line, sizeof(line), "HEAP%05lu",
             (unsigned long)xPortGetFreeHeapSize());
    LCD_ILI9806_DrawText(28U, 320U, line, LCD_COLOR_WHITE, 3U);
    LCD_ILI9806_DrawText(28U, 370U, "TOUCH", LCD_COLOR_CYAN, 3U);
    snprintf(line, sizeof(line), "X%03uY%03u", x, y);
    LCD_ILI9806_DrawText(28U, 420U, line, LCD_COLOR_WHITE, 3U);
    snprintf(line, sizeof(line), "TYPE%u", (unsigned int)type);
    LCD_ILI9806_DrawText(28U, 470U, line, LCD_COLOR_CYAN, 3U);
    snprintf(line, sizeof(line), "EVENT%05lu", (unsigned long)event_count);
    LCD_ILI9806_DrawText(28U, 520U, line, LCD_COLOR_WHITE, 3U);
    snprintf(line, sizeof(line), "DROP%03lu", (unsigned long)ui_event_dropped);
    LCD_ILI9806_DrawText(28U, 570U, line, LCD_COLOR_CYAN, 3U);
}

/* Called by HAL_GPIO_EXTI_IRQHandler() from EXTI1_IRQHandler(). */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == GPIO_PIN_1) {
        BaseType_t higher_priority_task_woken = pdFALSE;
        touch_irq_count++;
        if ((touch_ready != 0U) && (input_task_handle != NULL) &&
            (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING)) {
            vTaskNotifyGiveFromISR(input_task_handle,
                                   &higher_priority_task_woken);
            portYIELD_FROM_ISR(higher_priority_task_woken);
        }
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
