#ifndef LCD_ILI9806_H
#define LCD_ILI9806_H

#include "stm32f4xx_hal.h"

/*
 * The existing calendar firmware used portrait mode as its default.  Keep
 * that known-good orientation for the first bring-up; set this to 1 only
 * after the panel orientation has been confirmed on the new project.
 */
#ifndef LCD_ILI9806_LANDSCAPE
#define LCD_ILI9806_LANDSCAPE 0
#endif

#if LCD_ILI9806_LANDSCAPE
#define LCD_ILI9806_WIDTH  800U
#define LCD_ILI9806_HEIGHT 480U
#else
#define LCD_ILI9806_WIDTH  480U
#define LCD_ILI9806_HEIGHT 800U
#endif

/* The old board driver left PB15 low while the backlight was on. */
#ifndef LCD_BACKLIGHT_ACTIVE_HIGH
#define LCD_BACKLIGHT_ACTIVE_HIGH 0
#endif

HAL_StatusTypeDef LCD_ILI9806_Init(void);
void LCD_ILI9806_ShowTestPattern(void);
uint8_t LCD_ILI9806_IsReady(void);

#endif /* LCD_ILI9806_H */
