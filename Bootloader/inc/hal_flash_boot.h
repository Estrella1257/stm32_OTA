#ifndef HAL_FLASH_BOOT_H
#define HAL_FLASH_BOOT_H

#include <stdint.h>
#include "system_memory_map.h"

// 错误码定义
#define HAL_OK                  0
#define HAL_ERR_PARAM           -1  // 参数错误 (对齐或越界)
#define HAL_ERR_FLASH           -2  // 硬件操作失败
#define HAL_ERR_VERIFY          -3  // 写入校验失败

int hal_flash_init(void);
int hal_flash_erase(uint32_t address);
int hal_flash_write(uint32_t address, const uint8_t *buffer, uint32_t len);
int hal_flash_read(uint32_t address, uint8_t *buffer, uint32_t len);

#endif