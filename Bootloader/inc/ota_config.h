#ifndef __OTA_CONFIG_H
#define __OTA_CONFIG_H

#include "stm32f4xx.h"

/* OTA status flag */
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