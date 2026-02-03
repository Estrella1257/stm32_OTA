#ifndef __BOOT_H__
#define __BOOT_H__

#include <stdint.h>

#define APP_START_ADDR  0x08008000

typedef void (*app_entry_t)(void);

void boot_jump_to_app(uint32_t app_addr);
int  boot_check_app(uint32_t app_addr);

#endif
