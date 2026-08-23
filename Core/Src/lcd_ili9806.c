#include "lcd_ili9806.h"

#include <stddef.h>

/* FSMC Bank4 + A6 command/data selection from the board contract. */
#define LCD_FSMC_BASE (0x6C000000UL | 0x0000007EUL)

typedef struct {
    __IO uint16_t command;
    __IO uint16_t data;
} lcd_fsmc_regs_t;

#define LCD_FSMC ((lcd_fsmc_regs_t *)LCD_FSMC_BASE)

SRAM_HandleTypeDef hsram4;
static uint8_t lcd_ready;

static void lcd_write_command(uint8_t command)
{
    LCD_FSMC->command = command;
}

static void lcd_write_data(uint8_t data)
{
    LCD_FSMC->data = data;
}

static void lcd_write_command_data(uint8_t command,
                                   const uint8_t *data,
                                   size_t length)
{
    lcd_write_command(command);
    for (size_t i = 0U; i < length; ++i) {
        lcd_write_data(data[i]);
    }
}

static void lcd_set_backlight(uint8_t on)
{
    GPIO_PinState state = (on != 0U) ? GPIO_PIN_SET : GPIO_PIN_RESET;
#if !LCD_BACKLIGHT_ACTIVE_HIGH
    state = (state == GPIO_PIN_SET) ? GPIO_PIN_RESET : GPIO_PIN_SET;
#endif
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_15, state);
}

static void lcd_set_window(uint16_t x0, uint16_t y0,
                           uint16_t x1, uint16_t y1)
{
    uint8_t data[4];

    data[0] = (uint8_t)(x0 >> 8);
    data[1] = (uint8_t)x0;
    data[2] = (uint8_t)(x1 >> 8);
    data[3] = (uint8_t)x1;
    lcd_write_command_data(0x2AU, data, sizeof(data));

    data[0] = (uint8_t)(y0 >> 8);
    data[1] = (uint8_t)y0;
    data[2] = (uint8_t)(y1 >> 8);
    data[3] = (uint8_t)y1;
    lcd_write_command_data(0x2BU, data, sizeof(data));
    lcd_write_command(0x2CU);
}

static void lcd_fill(uint16_t color, uint32_t pixels)
{
    for (uint32_t i = 0U; i < pixels; ++i) {
        LCD_FSMC->data = color;
    }
}

static void lcd_fill_rect(uint16_t x0, uint16_t y0,
                          uint16_t x1, uint16_t y1,
                          uint16_t color)
{
    if ((x0 > x1) || (y0 > y1) ||
        (x1 >= LCD_ILI9806_WIDTH) || (y1 >= LCD_ILI9806_HEIGHT)) {
        return;
    }
    lcd_set_window(x0, y0, x1, y1);
    lcd_fill(color, ((uint32_t)x1 - x0 + 1U) *
                    ((uint32_t)y1 - y0 + 1U));
}

/* A deliberately small 5x7 ASCII subset for the bring-up message. */
static const uint8_t *lcd_glyph(char c)
{
    static const uint8_t blank[5] = {0, 0, 0, 0, 0};
    static const uint8_t glyph_0[5] = {0x3E, 0x51, 0x49, 0x45, 0x3E};
    static const uint8_t glyph_1[5] = {0x00, 0x42, 0x7F, 0x40, 0x00};
    static const uint8_t glyph_2[5] = {0x42, 0x61, 0x51, 0x49, 0x46};
    static const uint8_t glyph_3[5] = {0x21, 0x41, 0x45, 0x4B, 0x31};
    static const uint8_t glyph_4[5] = {0x18, 0x14, 0x12, 0x7F, 0x10};
    static const uint8_t glyph_5[5] = {0x27, 0x45, 0x45, 0x45, 0x39};
    static const uint8_t glyph_6[5] = {0x1E, 0x29, 0x49, 0x49, 0x06};
    static const uint8_t glyph_7[5] = {0x01, 0x71, 0x09, 0x05, 0x03};
    static const uint8_t glyph_8[5] = {0x36, 0x49, 0x49, 0x49, 0x36};
    static const uint8_t glyph_9[5] = {0x30, 0x49, 0x49, 0x4A, 0x3C};
    static const uint8_t glyph_A[5] = {0x7E, 0x11, 0x11, 0x11, 0x7E};
    static const uint8_t glyph_C[5] = {0x3E, 0x41, 0x41, 0x41, 0x22};
    static const uint8_t glyph_D[5] = {0x7F, 0x41, 0x41, 0x22, 0x1C};
    static const uint8_t glyph_E[5] = {0x7F, 0x49, 0x49, 0x49, 0x41};
    static const uint8_t glyph_F[5] = {0x7F, 0x09, 0x09, 0x09, 0x01};
    static const uint8_t glyph_H[5] = {0x7F, 0x08, 0x08, 0x08, 0x7F};
    static const uint8_t glyph_I[5] = {0x00, 0x41, 0x7F, 0x41, 0x00};
    static const uint8_t glyph_K[5] = {0x7F, 0x08, 0x14, 0x22, 0x41};
    static const uint8_t glyph_L[5] = {0x7F, 0x40, 0x40, 0x40, 0x40};
    static const uint8_t glyph_M[5] = {0x7F, 0x02, 0x0C, 0x02, 0x7F};
    static const uint8_t glyph_N[5] = {0x7F, 0x02, 0x0C, 0x10, 0x7F};
    static const uint8_t glyph_O[5] = {0x3E, 0x41, 0x41, 0x41, 0x3E};
    static const uint8_t glyph_P[5] = {0x7F, 0x09, 0x09, 0x09, 0x06};
    static const uint8_t glyph_Q[5] = {0x3E, 0x41, 0x51, 0x21, 0x5E};
    static const uint8_t glyph_R[5] = {0x7F, 0x09, 0x19, 0x29, 0x46};
    static const uint8_t glyph_S[5] = {0x46, 0x49, 0x49, 0x49, 0x31};
    static const uint8_t glyph_T[5] = {0x01, 0x01, 0x7F, 0x01, 0x01};
    static const uint8_t glyph_U[5] = {0x3F, 0x40, 0x40, 0x40, 0x3F};
    static const uint8_t glyph_V[5] = {0x1F, 0x20, 0x40, 0x20, 0x1F};
    static const uint8_t glyph_X[5] = {0x63, 0x14, 0x08, 0x14, 0x63};
    static const uint8_t glyph_Y[5] = {0x07, 0x08, 0x70, 0x08, 0x07};

    switch (c) {
    case '0': return glyph_0;
    case '1': return glyph_1;
    case '2': return glyph_2;
    case '3': return glyph_3;
    case '4': return glyph_4;
    case '5': return glyph_5;
    case '6': return glyph_6;
    case '7': return glyph_7;
    case '8': return glyph_8;
    case '9': return glyph_9;
    case 'A': return glyph_A;
    case 'C': return glyph_C;
    case 'D': return glyph_D;
    case 'E': return glyph_E;
    case 'F': return glyph_F;
    case 'H': return glyph_H;
    case 'I': return glyph_I;
    case 'K': return glyph_K;
    case 'L': return glyph_L;
    case 'M': return glyph_M;
    case 'N': return glyph_N;
    case 'O': return glyph_O;
    case 'P': return glyph_P;
    case 'Q': return glyph_Q;
    case 'R': return glyph_R;
    case 'S': return glyph_S;
    case 'T': return glyph_T;
    case 'U': return glyph_U;
    case 'V': return glyph_V;
    case 'X': return glyph_X;
    case 'Y': return glyph_Y;
    default: return blank;
    }
}

static void lcd_draw_text(uint16_t x, uint16_t y, const char *text,
                          uint16_t color, uint16_t scale)
{
    while (*text != '\0') {
        const uint8_t *glyph = lcd_glyph(*text++);
        for (uint16_t col = 0U; col < 5U; ++col) {
            for (uint16_t row = 0U; row < 7U; ++row) {
                if ((glyph[col] & (1U << row)) != 0U) {
                    lcd_fill_rect((uint16_t)(x + col * scale),
                                  (uint16_t)(y + row * scale),
                                  (uint16_t)(x + (col + 1U) * scale - 1U),
                                  (uint16_t)(y + (row + 1U) * scale - 1U),
                                  color);
                }
            }
        }
        x = (uint16_t)(x + 6U * scale);
    }
}

/* HAL_SRAM_Init() calls this callback to own the complete FSMC pin contract. */
void HAL_SRAM_MspInit(SRAM_HandleTypeDef *sram_handle)
{
    GPIO_InitTypeDef gpio = {0};
    (void)sram_handle;

    __HAL_RCC_FSMC_CLK_ENABLE();
    __HAL_RCC_GPIOF_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOG_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio.Alternate = GPIO_AF12_FSMC;

    gpio.Pin = GPIO_PIN_12;
    HAL_GPIO_Init(GPIOF, &gpio);

    gpio.Pin = GPIO_PIN_7 | GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 |
               GPIO_PIN_11 | GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14 |
               GPIO_PIN_15;
    HAL_GPIO_Init(GPIOE, &gpio);

    gpio.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_4 | GPIO_PIN_5 |
               GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 |
               GPIO_PIN_14 | GPIO_PIN_15;
    HAL_GPIO_Init(GPIOD, &gpio);

    gpio.Pin = GPIO_PIN_12;
    HAL_GPIO_Init(GPIOG, &gpio);

    gpio.Pin = GPIO_PIN_15;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Alternate = 0U;
    HAL_GPIO_Init(GPIOB, &gpio);
    lcd_set_backlight(0U);
}

static HAL_StatusTypeDef lcd_fsmc_init(void)
{
    FSMC_NORSRAM_TimingTypeDef timing = {0};
    FSMC_NORSRAM_TimingTypeDef extended_timing = {0};

    hsram4.Instance = FSMC_NORSRAM_DEVICE;
    hsram4.Extended = FSMC_NORSRAM_EXTENDED_DEVICE;
    hsram4.Init.NSBank = FSMC_NORSRAM_BANK4;
    hsram4.Init.DataAddressMux = FSMC_DATA_ADDRESS_MUX_DISABLE;
    hsram4.Init.MemoryType = FSMC_MEMORY_TYPE_SRAM;
    hsram4.Init.MemoryDataWidth = FSMC_NORSRAM_MEM_BUS_WIDTH_16;
    hsram4.Init.BurstAccessMode = FSMC_BURST_ACCESS_MODE_DISABLE;
    hsram4.Init.WaitSignalPolarity = FSMC_WAIT_SIGNAL_POLARITY_LOW;
    hsram4.Init.WrapMode = FSMC_WRAP_MODE_DISABLE;
    hsram4.Init.WaitSignalActive = FSMC_WAIT_TIMING_BEFORE_WS;
    hsram4.Init.WriteOperation = FSMC_WRITE_OPERATION_ENABLE;
    hsram4.Init.WaitSignal = FSMC_WAIT_SIGNAL_DISABLE;
    hsram4.Init.ExtendedMode = FSMC_EXTENDED_MODE_ENABLE;
    hsram4.Init.AsynchronousWait = FSMC_ASYNCHRONOUS_WAIT_DISABLE;
    hsram4.Init.WriteBurst = FSMC_WRITE_BURST_DISABLE;
    hsram4.Init.PageSize = FSMC_PAGE_SIZE_NONE;

    timing.AddressSetupTime = 2U;
    timing.AddressHoldTime = 15U;
    timing.DataSetupTime = 16U;
    timing.BusTurnAroundDuration = 0U;
    timing.CLKDivision = 16U;
    timing.DataLatency = 17U;
    timing.AccessMode = FSMC_ACCESS_MODE_A;

    extended_timing.AddressSetupTime = 4U;
    extended_timing.AddressHoldTime = 15U;
    extended_timing.DataSetupTime = 9U;
    extended_timing.BusTurnAroundDuration = 0U;
    extended_timing.CLKDivision = 16U;
    extended_timing.DataLatency = 17U;
    extended_timing.AccessMode = FSMC_ACCESS_MODE_A;

    return HAL_SRAM_Init(&hsram4, &timing, &extended_timing);
}

static void lcd_ili9806_controller_init(void)
{
    static const uint8_t unlock[] = {0xFFU, 0x98U, 0x06U};
    static const uint8_t bc[] = {0x03U, 0x0EU, 0x03U, 0x63U, 0x01U, 0x01U,
        0x1BU, 0x12U, 0x6FU, 0x00U, 0x00U, 0x00U, 0x01U, 0x01U, 0x03U,
        0x02U, 0xFFU, 0xF2U, 0x01U, 0x00U, 0xC0U};
    static const uint8_t bd[] = {0x02U, 0x13U, 0x45U, 0x67U, 0x45U, 0x67U,
        0x01U, 0x23U};
    static const uint8_t be[] = {0x01U, 0x22U, 0x22U, 0xDCU, 0xBAU, 0x67U,
        0x22U, 0x22U, 0x22U};
    static const uint8_t ee[] = {0x0AU, 0x1BU, 0x5FU, 0x40U, 0x28U, 0x38U,
        0x02U, 0x2BU, 0x50U, 0x00U, 0x80U};
    static const uint8_t df[] = {0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x20U};
    static const uint8_t f2[] = {0x41U, 0x04U, 0x41U, 0x28U};
    static const uint8_t c1[] = {0x17U, 0x78U, 0x7BU, 0x20U};
    static const uint8_t gamma_pos[] = {0x00U, 0x02U, 0x0CU, 0x0FU, 0x11U,
        0x1CU, 0xC8U, 0x07U, 0x03U, 0x08U, 0x03U, 0x0DU, 0x0CU, 0x31U,
        0x2CU, 0x00U};
    static const uint8_t gamma_neg[] = {0x00U, 0x02U, 0x08U, 0x0EU, 0x12U,
        0x17U, 0x7CU, 0x0AU, 0x03U, 0x09U, 0x06U, 0x0CU, 0x0CU, 0x2EU,
        0x2AU, 0x00U};
    static const uint8_t ba[] = {0x60U};
    static const uint8_t c7[] = {0x66U};
    static const uint8_t ed[] = {0x7FU, 0x0FU, 0x00U};
    static const uint8_t c0[] = {0x03U, 0x0BU, 0x00U};
    static const uint8_t f5[] = {0x20U, 0x43U, 0x00U};
    static const uint8_t fc[] = {0x08U};
    static const uint8_t f3[] = {0x74U};
    static const uint8_t b4[] = {0x02U, 0x02U, 0x02U};
    static const uint8_t f7[] = {0x82U};
    static const uint8_t b1[] = {0x00U, 0x13U, 0x13U};
    static const uint8_t madctl[] = {
#if LCD_ILI9806_LANDSCAPE
        0x68U
#else
        0x00U
#endif
    };
    static const uint8_t pixel_format[] = {0x55U};
    static const uint8_t te_on[] = {0x00U};

    lcd_write_command_data(0xFFU, unlock, sizeof(unlock));
    lcd_write_command_data(0xBAU, ba, sizeof(ba));
    lcd_write_command_data(0xBCU, bc, sizeof(bc));
    lcd_write_command_data(0xBDU, bd, sizeof(bd));
    lcd_write_command_data(0xBEU, be, sizeof(be));
    lcd_write_command_data(0xC7U, c7, sizeof(c7));
    lcd_write_command_data(0xEDU, ed, sizeof(ed));
    lcd_write_command_data(0xC0U, c0, sizeof(c0));
    lcd_write_command_data(0xF5U, f5, sizeof(f5));
    lcd_write_command_data(0xEEU, ee, sizeof(ee));
    lcd_write_command_data(0xFCU, fc, sizeof(fc));
    lcd_write_command_data(0xDFU, df, sizeof(df));
    lcd_write_command_data(0xF3U, f3, sizeof(f3));
    lcd_write_command_data(0xB4U, b4, sizeof(b4));
    lcd_write_command_data(0xF7U, f7, sizeof(f7));
    lcd_write_command_data(0xB1U, b1, sizeof(b1));
    lcd_write_command_data(0xF2U, f2, sizeof(f2));
    lcd_write_command_data(0xC1U, c1, sizeof(c1));
    lcd_write_command_data(0xE0U, gamma_pos, sizeof(gamma_pos));
    lcd_write_command_data(0xE1U, gamma_neg, sizeof(gamma_neg));
    lcd_write_command_data(0x35U, te_on, sizeof(te_on));
    lcd_write_command_data(0x36U, madctl, sizeof(madctl));
    lcd_write_command_data(0x3AU, pixel_format, sizeof(pixel_format));
    lcd_write_command(0x11U);
    HAL_Delay(120U);
    lcd_write_command(0x29U);
}

HAL_StatusTypeDef LCD_ILI9806_Init(void)
{
    GPIO_InitTypeDef gpio = {0};
    HAL_StatusTypeDef status = lcd_fsmc_init();
    if (status != HAL_OK) {
        lcd_ready = 0U;
        return status;
    }

    __HAL_RCC_GPIOB_CLK_ENABLE();
    gpio.Pin = GPIO_PIN_15;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &gpio);
    lcd_set_backlight(0U);
    HAL_Delay(5U);
    lcd_ili9806_controller_init();
    lcd_set_backlight(1U);
    lcd_ready = 1U;
    return HAL_OK;
}

void LCD_ILI9806_Clear(uint16_t color)
{
    if (lcd_ready != 0U) {
        lcd_fill_rect(0U, 0U, LCD_ILI9806_WIDTH - 1U,
                      LCD_ILI9806_HEIGHT - 1U, color);
    }
}

void LCD_ILI9806_FillRect(uint16_t x0, uint16_t y0,
                          uint16_t x1, uint16_t y1, uint16_t color)
{
    if (lcd_ready != 0U) {
        lcd_fill_rect(x0, y0, x1, y1, color);
    }
}

void LCD_ILI9806_DrawText(uint16_t x, uint16_t y, const char *text,
                          uint16_t color, uint16_t scale)
{
    if ((lcd_ready != 0U) && (text != NULL) && (scale != 0U)) {
        lcd_draw_text(x, y, text, color, scale);
    }
}

void LCD_ILI9806_ShowTestPattern(void)
{
    if (lcd_ready == 0U) {
        return;
    }

    lcd_fill_rect(0U, 0U, LCD_ILI9806_WIDTH - 1U,
                  LCD_ILI9806_HEIGHT - 1U, 0x001FU);
    lcd_draw_text(20U, 30U, "ILI9806 OK", 0xFFFFU, 4U);
    lcd_fill_rect(20U, 90U, LCD_ILI9806_WIDTH - 21U, 109U, 0xF800U);
    lcd_fill_rect(20U, 120U, LCD_ILI9806_WIDTH - 21U, 139U, 0x07E0U);
    lcd_fill_rect(20U, 150U, LCD_ILI9806_WIDTH - 21U, 169U, 0xFFE0U);
}

uint8_t LCD_ILI9806_IsReady(void)
{
    return lcd_ready;
}
