#ifndef __DEBUG_SHELL_H__
#define __DEBUG_SHELL_H__

#include <stdio.h>
#include "nvs_api.h"
#include "usart.h"

extern const NvsPort_t stm32_nvs_port;
extern uint8_t global_sim_enable;

void DebugShell_PrintStatus(void);
void DebugShell_ShowMenu(void);
void DebugShell_ProcessCommand(char cmd);

#endif 