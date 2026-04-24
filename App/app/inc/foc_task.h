#ifndef __FOC_TASK_H
#define __FOC_TASK_H

#include <stdint.h>
#include <stdbool.h>

// 包含底层 FOC 肌肉层依赖的头文件
// 请确保这些头文件的名字与你工程中的实际名称一致
#include "foc.h"
#include "encoder.h"
#include "pid.h"
#include "filter.h"

// 将这两个变量暴露出去，方便你在 main.c 的 USART3 / VOFA 任务中打印波形
extern float g_filtered_rpm;
extern float target_voltage;

// 你的 PI 控制器实例（由于你之前定义在 main.c 中，这里需要声明 extern 以供 foc_task.c 内部调用）
extern PI_Controller_t motor_speed_pi;

/**
 * @brief FOC 核心高频调度任务 (1000Hz)
 * @note  必须且只能放入 TIM6 等 1ms 绝对中断服务函数中执行，绝不要在主循环调用
 */
void FOC_Hardware_1ms_ISR(void);

#endif /* __FOC_TASK_H */