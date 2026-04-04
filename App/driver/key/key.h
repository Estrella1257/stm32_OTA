#ifndef KEY_H
#define KEY_H

#include "stm32f4xx.h"

// 定义三大驾驶模式
typedef enum {
    MODE_ECO = 0,   // 低速档
    MODE_DRIVE,     // 中速档 (默认)
    MODE_SPORT      // 高速档
} DriveMode_t;

// 暴露给外部的 3 个 API
void Key_Init(void);           // 初始化引脚
void Key_Scan_10ms(void);      // 放在 10ms 任务里扫描消抖
DriveMode_t Key_Get(void);     // VCU 用这个获取当前真实档位

#endif