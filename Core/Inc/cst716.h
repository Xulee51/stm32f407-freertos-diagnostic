#ifndef CST716_H
#define CST716_H

#include "stm32f4xx_hal.h"

typedef struct {
    uint16_t x;
    uint16_t y;
    uint8_t fingers;
    uint8_t pressed;
} CST716_TouchSample;

/* Initializes PB0/PF11 software I2C, PB1 INT input, and PC13 reset. */
HAL_StatusTypeDef CST716_Init(uint16_t *version);

/* Polls the CST716 status register and returns the first touch point. */
HAL_StatusTypeDef CST716_Poll(CST716_TouchSample *sample);

#endif /* CST716_H */
