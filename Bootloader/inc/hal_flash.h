#ifndef HAL_FLASH_H
#define HAL_FLASH_H

#include <stdint.h>
#include "ota_config.h"

/* =========================================================================
 * Flash 物理边界与沙盒定义 (基于 STM32F407 1MB Flash)
 * Bootloader 占据 Sector 0-3 (0x08000000 - 0x0800FFFF)，被底层驱动绝对保护
 * ========================================================================= */

// 分区容量定义
#define NVS_SECTOR_SIZE      (64 * 1024)   // Sector 4: 64KB (用于键值对存储)
#define APP_PART_SIZE        (384 * 1024)  // Sector 5-7 (APP_A) 或 8-10 (APP_B): 384KB
#define RES_PART_SIZE        (128 * 1024)  // Sector 11: 128KB (保留给字库/图片资源)

// 错误码定义
#define HAL_OK                  0
#define HAL_ERR_PARAM           -1  // 参数错误 (对齐或越界)
#define HAL_ERR_FLASH           -2  // 硬件操作失败
#define HAL_ERR_VERIFY          -3  // 写入校验失败

int hal_flash_init(void);
int hal_flash_erase(uint32_t address);
int hal_flash_write(uint32_t address, const uint8_t *buffer, uint32_t len);
int hal_flash_read(uint32_t address, const uint8_t *buffer, uint32_t len);

#endif