#include "board.h"

const led_desc_t led1 =
{
    .clk_sourse = RCC_AHB1Periph_GPIOF,
    .port = GPIOF,
    .pin = GPIO_Pin_9,
    .on_lvl = Bit_RESET,
    .off_lvl = Bit_SET,
};

const led_desc_t led2 =
{
    .clk_sourse = RCC_AHB1Periph_GPIOF,
    .port = GPIOF,
    .pin = GPIO_Pin_10,
    .on_lvl = Bit_RESET,
    .off_lvl = Bit_SET,
};

const led_desc_t led3 =
{
    .clk_sourse = RCC_AHB1Periph_GPIOE,
    .port = GPIOE,
    .pin = GPIO_Pin_13,
    .on_lvl = Bit_RESET,
    .off_lvl = Bit_SET,
};

const led_desc_t led4 =
{
    .clk_sourse = RCC_AHB1Periph_GPIOE,
    .port = GPIOE,
    .pin = GPIO_Pin_14,
    .on_lvl = Bit_RESET,
    .off_lvl = Bit_SET,
};

void board_lowlevel_init(void)
{
	NVIC_SetPriorityGrouping(NVIC_PriorityGroup_4);

	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA,ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1,ENABLE);
}
