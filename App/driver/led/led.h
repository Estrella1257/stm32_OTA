#ifndef __DRV_LED_H__
#define __DRV_LED_H__

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "stm32f4xx.h"

typedef struct led_desc
{
    uint32_t clk_sourse;
    GPIO_TypeDef *port;
    uint16_t pin;
    BitAction on_lvl;
    BitAction off_lvl;
}led_desc_t;

void led_init(const led_desc_t*desc);
void led_deinit(const led_desc_t*desc);
void led_on(const led_desc_t*desc);
void led_off(const led_desc_t*desc);

#endif