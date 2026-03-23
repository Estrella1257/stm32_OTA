#ifndef BSP_WIFI_H
#define BSP_WIFI_H

#include <stdbool.h>

#define VCU_WIFI_SSID      "Redmi K60 Pro"
#define VCU_WIFI_PASSWORD  "12345678"

extern volatile bool g_wifi_connected;

void bsp_wifi_init(void);

#endif