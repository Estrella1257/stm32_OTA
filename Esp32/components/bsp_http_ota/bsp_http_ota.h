#ifndef BSP_HTTP_OTA_H
#define BSP_HTTP_OTA_H

#include <stdbool.h>

// 初始化硬盘
bool bsp_spiffs_init(void);
// 从服务器下载文件并存入硬盘
bool bsp_http_download_firmware(void);

#endif