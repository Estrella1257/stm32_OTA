#include "stm32f4xx.h"
#include "uart.h"

// 假设你的 APB2 总线频率是 84MHz (STM32F407 标准主频 168MHz 下)
// 如果你的时钟配置不同，波特率可能会有偏差，但这不影响程序运行，顶多是乱码
#define PCLK2_FREQ      84000000
#define BAUDRATE        115200

void UART_Init(void)
{
    // ------------------ 1. 开启时钟 ------------------
    // 使能 GPIOA 时钟 (AHB1)
    RCC->AHB1ENR |= (1UL << 0); 
    // 使能 USART1 时钟 (APB2)
    RCC->APB2ENR |= (1UL << 4);

    // ------------------ 2. 配置 GPIO (PA9) ------------------
    // 模式：复用功能 (Alternate Function) -> MODER[19:18] = 10
    GPIOA->MODER &= ~(3UL << 18); // 清除
    GPIOA->MODER |=  (2UL << 18); // 设置为复用

    // 输出类型：推挽 (Push-Pull) -> OTYPER[9] = 0 (默认就是)
    GPIOA->OTYPER &= ~(1UL << 9);

    // 速度：高速 (High Speed) -> OSPEEDR[19:18] = 10
    GPIOA->OSPEEDR &= ~(3UL << 18);
    GPIOA->OSPEEDR |=  (2UL << 18);

    // 复用映射：AF7 (USART1) -> AFRH[7:4] = 0111
    // PA9 在 AFR[1] (即 AFRH) 里
    GPIOA->AFR[1] &= ~(0xFUL << 4); // 清除
    GPIOA->AFR[1] |=  (0x7UL << 4); // 设为 AF7

    // ------------------ 3. 配置 USART1 ------------------
    // 暂时禁用串口以便配置
    USART1->CR1 = 0;

    // 波特率计算 (Mantissa + Fraction)
    // Formula: USARTDIV = PCLK / (16 * BaudRate)
    // 这是一个简化的近似值，足够 Bootloader 打印日志用了
    uint32_t usartdiv = (PCLK2_FREQ + (BAUDRATE / 2)) / BAUDRATE;
    USART1->BRR = usartdiv;

    // 使能发送 (TE) 和 接收 (RE - 虽然你可能不用)
    USART1->CR1 |= (1UL << 3); // TE
    USART1->CR1 |= (1UL << 2); // RE

    // 使能串口 (UE)
    USART1->CR1 |= (1UL << 13);
}

// 发送一个字符
void UART_SendChar(uint8_t ch)
{
    // 等待发送数据寄存器为空 (TXE)
    while (!(USART1->SR & (1UL << 7)));
    // 写入数据
    USART1->DR = ch;
}

// ------------------ 4. 重定向 printf ------------------
// 如果你用的是 GCC (Makefile/VSCode)，需要覆盖 _write 函数
int _write(int file, char *ptr, int len)
{
    for (int i = 0; i < len; i++)
    {
        UART_SendChar((uint8_t)ptr[i]);
    }
    return len;
}