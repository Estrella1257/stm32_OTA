#ifndef __OTA_CONFIG_H
#define __OTA_CONFIG_H

#include "stm32f4xx.h"

// 1. 划分地盘 (STM32F407)
#define NVS_SECTOR_ADDR      0x08010000  // Sector 4: 用于存放 OTA 状态 (64KB)
#define APP_A_ADDR           0x08020000  // Sector 5: 主程序起始地址 (128KB)
#define APP_B_ADDR           0x08080000  // Sector 8: 接收新固件的暂存区

// 分区容量定义改为 384KB
#define NVS_SECTOR_SIZE      (64 * 1024)   
#define APP_PART_SIZE        (384 * 1024)  // A区和B区各占 384KB

// 2. 定义状态标志 (魔术字)
#define OTA_FLAG_NORMAL      0xAAAAAAAA  // 正常启动 APP_A
#define OTA_FLAG_UPDATE      0xBBBBBBBB  // 收到更新请求，需要搬运固件
#define OTA_FLAG_EMPTY       0xFFFFFFFF  // Flash 刚擦除后的默认状态

// 3. 定义你之前构思的结构体
typedef struct {
    uint32_t boot_flag;      // 启动标志
    uint32_t app_version;    // 固件版本号
    uint32_t firmware_len;   // 固件大小
    uint32_t firmware_crc;   // 校验和
} ota_info_t;

#endif