#include "cst716.h"

#include "lcd_ili9806.h"

/* These are the validated 8-bit bus values from the original board driver. */
#define CST716_WRITE_ADDRESS 0x2AU
#define CST716_READ_ADDRESS  0x2BU
#define CST716_REG_MODE      0x00U
#define CST716_REG_FINGERS   0x02U
#define CST716_REG_POINT0    0x03U
#define CST716_REG_VERSION   0xA1U
#define CST716_REG_DEVICE_ID 0xA7U
#define CST716_REG_INT_MODE  0xA4U
#define CST716_REG_THRESHOLD 0x80U
#define CST716_REG_PERIOD    0x88U

#define CST716_MAX_FINGERS 2U

static void cst716_delay(void)
{
    /* Approximately a few microseconds at 168 MHz; this is not a timing
     * guarantee, but matches the validated software-I2C implementation. */
    for (volatile uint32_t i = 0U; i < 280U; ++i) {
        __NOP();
    }
}

static void cst716_sda_output(void)
{
    GPIO_InitTypeDef gpio = {0};
    gpio.Pin = GPIO_PIN_11;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOF, &gpio);
}

static void cst716_sda_input(void)
{
    GPIO_InitTypeDef gpio = {0};
    gpio.Pin = GPIO_PIN_11;
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOF, &gpio);
}

static void cst716_start(void)
{
    cst716_sda_output();
    HAL_GPIO_WritePin(GPIOF, GPIO_PIN_11, GPIO_PIN_SET);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_SET);
    cst716_delay();
    HAL_GPIO_WritePin(GPIOF, GPIO_PIN_11, GPIO_PIN_RESET);
    cst716_delay();
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_RESET);
}

static void cst716_stop(void)
{
    cst716_sda_output();
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_SET);
    cst716_delay();
    HAL_GPIO_WritePin(GPIOF, GPIO_PIN_11, GPIO_PIN_RESET);
    cst716_delay();
    HAL_GPIO_WritePin(GPIOF, GPIO_PIN_11, GPIO_PIN_SET);
}

static uint8_t cst716_wait_ack(void)
{
    uint16_t timeout = 0U;
    cst716_sda_input();
    HAL_GPIO_WritePin(GPIOF, GPIO_PIN_11, GPIO_PIN_SET);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_SET);
    cst716_delay();
    while (HAL_GPIO_ReadPin(GPIOF, GPIO_PIN_11) == GPIO_PIN_SET) {
        if (++timeout > 250U) {
            cst716_stop();
            return 0U;
        }
        cst716_delay();
    }
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_RESET);
    return 1U;
}

static void cst716_send_ack(uint8_t ack)
{
    cst716_sda_output();
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOF, GPIO_PIN_11,
                      (ack != 0U) ? GPIO_PIN_RESET : GPIO_PIN_SET);
    cst716_delay();
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_SET);
    cst716_delay();
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_RESET);
}

static void cst716_send_byte(uint8_t value)
{
    cst716_sda_output();
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_RESET);
    for (uint8_t bit = 0U; bit < 8U; ++bit) {
        HAL_GPIO_WritePin(GPIOF, GPIO_PIN_11,
                          ((value & 0x80U) != 0U) ? GPIO_PIN_SET : GPIO_PIN_RESET);
        value <<= 1;
        cst716_delay();
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_SET);
        cst716_delay();
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_RESET);
    }
}

static uint8_t cst716_read_byte(uint8_t ack)
{
    uint8_t value = 0U;
    cst716_sda_input();
    for (uint8_t bit = 0U; bit < 8U; ++bit) {
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_RESET);
        cst716_delay();
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_SET);
        cst716_delay();
        value = (uint8_t)(value << 1);
        if (HAL_GPIO_ReadPin(GPIOF, GPIO_PIN_11) == GPIO_PIN_SET) {
            value |= 1U;
        }
    }
    cst716_send_ack(ack);
    return value;
}

static HAL_StatusTypeDef cst716_write_reg(uint8_t reg,
                                          const uint8_t *data,
                                          uint8_t length)
{
    cst716_start();
    cst716_send_byte(CST716_WRITE_ADDRESS);
    if (cst716_wait_ack() == 0U) {
        cst716_stop();
        return HAL_ERROR;
    }
    cst716_send_byte(reg);
    if (cst716_wait_ack() == 0U) {
        cst716_stop();
        return HAL_ERROR;
    }
    for (uint8_t i = 0U; i < length; ++i) {
        cst716_send_byte(data[i]);
        if (cst716_wait_ack() == 0U) {
            cst716_stop();
            return HAL_ERROR;
        }
    }
    cst716_stop();
    return HAL_OK;
}

static HAL_StatusTypeDef cst716_read_reg(uint8_t reg,
                                         uint8_t *data,
                                         uint8_t length)
{
    cst716_start();
    cst716_send_byte(CST716_WRITE_ADDRESS);
    if (cst716_wait_ack() == 0U) {
        cst716_stop();
        return HAL_ERROR;
    }
    cst716_send_byte(reg);
    if (cst716_wait_ack() == 0U) {
        cst716_stop();
        return HAL_ERROR;
    }
    cst716_start();
    cst716_send_byte(CST716_READ_ADDRESS);
    if (cst716_wait_ack() == 0U) {
        cst716_stop();
        return HAL_ERROR;
    }
    for (uint8_t i = 0U; i < length; ++i) {
        data[i] = cst716_read_byte((i + 1U < length) ? 1U : 0U);
    }
    cst716_stop();
    return HAL_OK;
}

HAL_StatusTypeDef CST716_Init(uint16_t *version)
{
    GPIO_InitTypeDef gpio = {0};
    uint8_t value = 0U;
    uint8_t version_bytes[2] = {0U, 0U};

    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOF_CLK_ENABLE();

    gpio.Pin = GPIO_PIN_1;
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOB, &gpio);

    gpio.Pin = GPIO_PIN_13;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOC, &gpio);

    gpio.Pin = GPIO_PIN_0;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOB, &gpio);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_SET);

    cst716_sda_output();
    HAL_GPIO_WritePin(GPIOF, GPIO_PIN_11, GPIO_PIN_SET);

    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);
    HAL_Delay(20U);
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);
    HAL_Delay(50U);

    if (cst716_read_reg(CST716_REG_DEVICE_ID, &value, 1U) != HAL_OK) {
        return HAL_ERROR;
    }

    value = 0U;
    if (cst716_write_reg(CST716_REG_MODE, &value, 1U) != HAL_OK ||
        cst716_write_reg(CST716_REG_INT_MODE, &value, 1U) != HAL_OK) {
        return HAL_ERROR;
    }
    value = 22U;
    if (cst716_write_reg(CST716_REG_THRESHOLD, &value, 1U) != HAL_OK) {
        return HAL_ERROR;
    }
    value = 12U;
    if (cst716_write_reg(CST716_REG_PERIOD, &value, 1U) != HAL_OK ||
        cst716_read_reg(CST716_REG_VERSION, version_bytes, 2U) != HAL_OK) {
        return HAL_ERROR;
    }

    if (version != NULL) {
        *version = (uint16_t)(((uint16_t)version_bytes[0] << 8) |
                              version_bytes[1]);
    }
    return HAL_OK;
}

HAL_StatusTypeDef CST716_Poll(CST716_TouchSample *sample)
{
    uint8_t fingers = 0U;
    uint8_t point[4] = {0U, 0U, 0U, 0U};

    if (sample == NULL) {
        return HAL_ERROR;
    }
    sample->x = 0U;
    sample->y = 0U;
    sample->fingers = 0U;
    sample->pressed = 0U;

    if (cst716_read_reg(CST716_REG_FINGERS, &fingers, 1U) != HAL_OK) {
        return HAL_ERROR;
    }
    fingers &= 0x0FU;
    if ((fingers == 0U) || (fingers > CST716_MAX_FINGERS)) {
        return HAL_OK;
    }
    if (cst716_read_reg(CST716_REG_POINT0, point, sizeof(point)) != HAL_OK) {
        return HAL_ERROR;
    }

    sample->x = (uint16_t)(((point[0] & 0x0FU) << 8) | point[1]);
    sample->y = (uint16_t)(((point[2] & 0x0FU) << 8) | point[3]);
#if LCD_ILI9806_LANDSCAPE
    {
        uint16_t coordinate = sample->x;
        sample->x = sample->y;
        sample->y = coordinate;
    }
#endif
    if ((sample->x >= LCD_ILI9806_WIDTH) ||
        (sample->y >= LCD_ILI9806_HEIGHT)) {
        return HAL_OK;
    }
    sample->fingers = fingers;
    sample->pressed = 1U;
    return HAL_OK;
}
