#include "main.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "event_groups.h"
#include <stdio.h>
#include <string.h>

UART_HandleTypeDef huart1;

enum {
    EVT_BOOT_OK = 1U << 0,
    EVT_LOGGER_OK = 1U << 1,
    EVT_UI_OK = 1U << 2
};

typedef struct {
    uint32_t tick;
    char text[48];
} log_message_t;

static QueueHandle_t log_queue;
static EventGroupHandle_t health_events;

static void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART1_UART_Init(void);
static void logger_task(void *argument);
static void health_task(void *argument);
static void ui_task(void *argument);
static void uart_write(const char *text);

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_USART1_UART_Init();

    uart_write("\r\n[BOOT] freertos_robot_diagnostic\r\n");
    uart_write("[BOOT] STM32F407ZGT6 168 MHz, USART1 115200\r\n");

    log_queue = xQueueCreate(8, sizeof(log_message_t));
    health_events = xEventGroupCreate();
    if ((log_queue == NULL) || (health_events == NULL)) {
        Error_Handler();
    }

    if (xTaskCreate(health_task, "health", 384, NULL, 4, NULL) != pdPASS ||
        xTaskCreate(logger_task, "logger", 384, NULL, 3, NULL) != pdPASS ||
        xTaskCreate(ui_task, "ui", 384, NULL, 1, NULL) != pdPASS) {
        Error_Handler();
    }

    xEventGroupSetBits(health_events, EVT_BOOT_OK);
    vTaskStartScheduler();
    Error_Handler();
}

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
        (void)xQueueSend(log_queue, &message, pdMS_TO_TICKS(20));
        vTaskDelayUntil(&wake, pdMS_TO_TICKS(1000));
    }
}

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
    __HAL_RCC_GPIOA_CLK_ENABLE();
}

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

void Error_Handler(void)
{
    __disable_irq();
    for (;;) { }
}

void vApplicationMallocFailedHook(void)
{
    Error_Handler();
}

void vApplicationStackOverflowHook(TaskHandle_t task, char *name)
{
    (void)task;
    (void)name;
    Error_Handler();
}
