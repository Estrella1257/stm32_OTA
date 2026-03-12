#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "stm32f4xx.h"
#include "led.h"

void led_init(const led_desc_t *desc)
{
    RCC_AHB1PeriphClockCmd(desc->clk_sourse,ENABLE);

    GPIO_InitTypeDef ginit;
    memset(&ginit,0,sizeof(GPIO_InitTypeDef));
    ginit.GPIO_Pin=desc->pin;
    ginit.GPIO_Mode=GPIO_Mode_OUT;
    ginit.GPIO_OType=GPIO_OType_PP;
    ginit.GPIO_Speed=GPIO_Medium_Speed;
    GPIO_Init(desc->port,&ginit);

    GPIO_WriteBit(desc->port,desc->pin,desc->off_lvl);
}

void led_deinit(const led_desc_t *desc)
{
    GPIO_WriteBit(desc->port,desc->pin,desc->on_lvl);
}

void led_on(const led_desc_t *desc)
{
    GPIO_WriteBit(desc->port,desc->pin,desc->on_lvl);
}

void led_off(const led_desc_t *desc)
{
    GPIO_WriteBit(desc->port,desc->pin,desc->off_lvl);
}

void led_toggle(const led_desc_t *desc)
{
    // 读取当前引脚的输出状态
    if (GPIO_ReadOutputDataBit(desc->port, desc->pin)) {
        // 如果当前是高电平，则拉低
        GPIO_WriteBit(desc->port, desc->pin, Bit_RESET); 
    } else {
        // 如果当前是低电平，则拉高
        GPIO_WriteBit(desc->port, desc->pin, Bit_SET);   
    }
}