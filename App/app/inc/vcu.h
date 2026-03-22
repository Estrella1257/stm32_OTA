#ifndef __VCU_H__
#define __VCU_H__

#include "protocol.h"
#include "nvs_api.h"

extern NvsContext_t   g_nvs_ctx;
extern SystemConfig_t g_sys_cfg;
extern const NvsPort_t stm32_nvs_port;

void VCU_Task_50ms(void);
void VCU_Config_Init(void);

#endif 