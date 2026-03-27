#include "bsp_http_ota.h"
#include <stdio.h>
#include <string.h>
#include "esp_spiffs.h"
#include "esp_http_client.h"
#include "esp_log.h"

//static const char *TAG = "HTTP_OTA";

// 1. 初始化 SPIFFS 虚拟硬盘
bool bsp_spiffs_init(void) {
    //ESP_LOGI(TAG, "Initializing SPIFFS...");
    printf("[FS] INIT -> Mounting SPIFFS partition...\n");
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = NULL,
        .max_files = 5,
        .format_if_mount_failed = true // 第一次运行会自动格式化硬盘！
    };

    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK) {
        //ESP_LOGE(TAG, "Failed to mount or format filesystem");
        printf("[FS] FATAL -> Failed to mount/format SPIFFS (%s)\n", esp_err_to_name(ret));
        return false;
    }
    
    size_t total = 0, used = 0;
    esp_spiffs_info(NULL, &total, &used);
    //ESP_LOGI(TAG, "SPIFFS Mounted. Total: %d bytes, Used: %d bytes", total, used);
    printf("[FS] OK -> Total: %d, Used: %d (%.1f%%)\n", total, used, (float)used/total*100);
    return true;
}

// 2. HTTP 下载并写入硬盘 (边下边存，极低内存占用)
bool bsp_http_download_firmware(void) {
    //ESP_LOGI(TAG, "Starting Firmware Download...");
    printf("[HTTP] GET -> Connecting to %s\n", WEB_SERVER_URL);

    // 目标地址：你电脑 Python 服务器的地址
    esp_http_client_config_t config = {
        .url = WEB_SERVER_URL,
        .buffer_size = 1024,
        .timeout_ms = 5000,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_err_t err = esp_http_client_open(client, 0);
    
    if (err != ESP_OK) {
        //ESP_LOGE(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(err));
        printf("[HTTP] ERR -> Connection failed: %s\n", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return false;
    }

    int content_length = esp_http_client_fetch_headers(client);
    //ESP_LOGI(TAG, "Target File Size: %d bytes", content_length);
    printf("[HTTP] OK -> Server found. Payload size: %d bytes\n", content_length);

    // 打开硬盘里的文件准备写入
    FILE *f = fopen("/spiffs/app.bin", "w");
    if (f == NULL) {
        //ESP_LOGE(TAG, "Failed to open file for writing");
        printf("[FS] ERR -> Access denied. Cannot write /spiffs/app.bin\n");
        esp_http_client_cleanup(client);
        return false;
    }

    // 每次只读取 512 字节放入内存，然后立刻写入 Flash，绝不撑爆 RAM
    int read_len;
    int total_read = 0;
    char buffer[512]; 
    
    while ((read_len = esp_http_client_read(client, buffer, sizeof(buffer))) > 0) {
        fwrite(buffer, 1, read_len, f);
        total_read += read_len;
        // 每下载一点打印一下进度
        if (total_read % 10240 == 0) { 
            //ESP_LOGI(TAG, "Downloaded %d / %d bytes...", total_read, content_length);
            printf("[HTTP] RX -> Received: %d / %d bytes\n", total_read, content_length);
        }
    }

    fclose(f);
    esp_http_client_cleanup(client);

    if (total_read == content_length && content_length > 0) {
        //ESP_LOGI(TAG, "Download Complete & Saved to SPIFFS!");
        printf("[HTTP] OK   -> Download complete. File saved: %d bytes\n", total_read);
        return true;
    } else {
        //ESP_LOGE(TAG, "Download Incomplete.");
        printf("[HTTP] ERR  -> Download incomplete! (Expected: %d, Got: %d)\n", content_length, total_read);
        return false;
    }
}