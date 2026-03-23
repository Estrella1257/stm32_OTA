#include "bsp_wifi.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sys.h"

static const char *TAG = "VCU_WIFI";

volatile bool g_wifi_connected = false;
static int s_retry_num = 0;

// 核心事件回调机：处理连接、断开、获取IP等异步事件
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    // 1. WiFi 硬件启动成功，开始呼叫路由器
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "WiFi hardware started. Connecting to AP...");
        esp_wifi_connect();
    } 
    // 2. 糟糕，连接断开了！（密码错、信号差、或者路由器重启）
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        g_wifi_connected = false;
        if (s_retry_num < 100) { // 这里写死了一直重连，车规级产品通常是无限重连
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGW(TAG, "Connection failed. Retrying... (%d)", s_retry_num);
        } else {
            ESP_LOGE(TAG, "Connection failed permanently.");
        }
    } 
    // 3. 狂喜！成功获取到了路由器的 IP 地址
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "SUCCESS! Got IP Address: " IPSTR, IP2STR(&event->ip_info.ip));
        
        s_retry_num = 0; // 重置重连次数
        g_wifi_connected = true; // 升起全局旗帜！
        
        // TODO: 未来可以在这里触发 LVGL 屏幕右上角的 WiFi 图标变亮
    }
}

// WiFi 初始化入口
void bsp_wifi_init(void)
{
    ESP_LOGI(TAG, "Initializing WiFi in Station Mode...");

    // 1. 初始化底层 TCP/IP 协议栈
    ESP_ERROR_CHECK(esp_netif_init());

    // 2. 创建默认的事件循环
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // 3. 创建默认的 WiFi Station 网卡
    esp_netif_create_default_wifi_sta();

    // 4. 初始化 WiFi 硬件底层 (分配内存、加载固件)
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // 5. 注册事件监听器 (极其重要！把我们的回调函数挂载到系统总线上)
    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    // 6. 配置账号密码
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = VCU_WIFI_SSID,
            .password = VCU_WIFI_PASSWORD,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK, // 大多数路由器的安全模式
        },
    };
    
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    
    // 7. 启动！
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "WiFi initialization complete. Async connection started.");
}