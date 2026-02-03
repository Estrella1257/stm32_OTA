#ifndef __BOARD_H
#define __BOARD_H

#include "led.h"
#include "stm32f4xx.h"

extern const led_desc_t led1;
extern const led_desc_t led2;
extern const led_desc_t led3;
extern const led_desc_t led4;
void board_lowlevel_init(void);

#endif