#include "key.h"

// 内部维护的当前经过消抖确认的真实档位
static DriveMode_t g_current_mode = MODE_DRIVE; 

void Key_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    // 配置 PB4(低速) 和 PB5(高速) 为输入，必须开启内部上拉！
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4 | GPIO_Pin_5;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP; // 极其重要：上拉电阻
    GPIO_Init(GPIOB, &GPIO_InitStructure);
}

// 这个函数极其轻量，每次执行只要几微秒
void Key_Scan_10ms(void)
{
    static uint8_t debounce_cnt = 0;
    static DriveMode_t last_raw_mode = MODE_DRIVE;
    DriveMode_t raw_mode = MODE_DRIVE; // 默认中速

    // 1. 读取物理引脚原始状态 (0 表示导通)
    if (GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_4) == RESET) {
        raw_mode = MODE_ECO;
    } else if (GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_5) == RESET) {
        raw_mode = MODE_SPORT;
    }

    // 2. 核心状态机消抖逻辑
    if (raw_mode == last_raw_mode) {
        // 如果状态和上次看的一样，开始攒经验值
        debounce_cnt++;
        if (debounce_cnt >= 3) { // 连续 3 次 (30ms) 稳定
            g_current_mode = raw_mode; // 确认为真实档位
            debounce_cnt = 3; // 防止溢出
        }
    } else {
        // 一旦有波动（毛刺），经验值瞬间清零，重新开始验证
        debounce_cnt = 0;
        last_raw_mode = raw_mode;
    }
}

// 供 VCU 随时随地调用，无任何延迟
DriveMode_t Key_Get(void)
{
    return g_current_mode;
}