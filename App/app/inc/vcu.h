#ifndef __VCU_H__
#define __VCU_H__

#include "protocol.h"
#include "nvs_api.h"

typedef enum {
    VCU_STATE_INIT = 0,    // 0: 初始化自检
    VCU_STATE_STANDBY,     // 1: 驻车待机（电机锁死）
    VCU_STATE_DRIVING,     // 2: 行驶模式（响应油门）
    VCU_STATE_FAULT        // 3: 故障模式（切断动力）
} VCU_State_t;

extern VCU_State_t g_vcu_state;
extern NvsContext_t   g_nvs_ctx;
extern SystemConfig_t g_sys_cfg;
extern const NvsPort_t stm32_nvs_port;

void VCU_Task_50ms(void);
void VCU_Config_Init(void);

#endif 