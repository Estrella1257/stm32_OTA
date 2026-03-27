#ifndef BSP_HTTP_OTA_H
#define BSP_HTTP_OTA_H

#include <stdbool.h>

#define WEB_SERVER_URL  "http://192.168.201.91:5000/download/app.bin"

// 初始化硬盘
bool bsp_spiffs_init(void);
// 从服务器下载文件并存入硬盘
bool bsp_http_download_firmware(void);

#endif