#include "bsp_lcd.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "driver/uart.h"    
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_err.h"
#include "esp_lcd_ili9341.h"
#include "ui.h"
#include "esp_lvgl_port.h"

#define LCD_HOST       SPI2_HOST
#define LCD_PIXEL_CLOCK_HZ (20 * 1000 * 1000) 
#define LCD_PIN_MOSI   11
#define LCD_PIN_CLK    12
#define LCD_PIN_CS     10
#define LCD_PIN_DC     46   
#define LCD_PIN_RST    -1  
#define LCD_PIN_BLK    45 
#define LCD_H_RES      240
#define LCD_V_RES      320

static esp_lcd_panel_handle_t global_panel_handle = NULL;

static void my_hijacked_flush_cb(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p)
{
    // 1. 算出当前这块小图像的宽 (w) 和高 (h)
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);
    uint16_t *buf = (uint16_t *)color_p;

    // 2. 双重循环：像扫描仪一样，一行一行地扫，从左到右扫一半
    for (uint32_t y = 0; y < h; y++) {
        for (uint32_t x = 0; x < w / 2; x++) {
            // 算出左边像素的索引和右边对应像素的索引
            uint32_t left_idx = y * w + x;
            uint32_t right_idx = y * w + (w - 1 - x);

            // 取出像素，并强行翻转颜色高低字节 (完美切除边缘彩色线条(抗锯齿修补))
            uint16_t left_pixel = (buf[left_idx] >> 8) | (buf[left_idx] << 8);
            uint16_t right_pixel = (buf[right_idx] >> 8) | (buf[right_idx] << 8);

            // 左右互换位置 (强行在内存里完成镜像反转)
            buf[left_idx] = right_pixel;
            buf[right_idx] = left_pixel;
        }
        // 如果宽度是奇数，处理最中间那个不用换位置的像素
        if (w % 2 != 0) {
            uint32_t mid_idx = y * w + (w / 2);
            buf[mid_idx] = (buf[mid_idx] >> 8) | (buf[mid_idx] << 8);
        }
    }

    // 3. 映射物理坐标：既然图像左右反了，那么它要贴在屏幕上的物理坐标 X 也必须反过来
    uint16_t physical_x1 = LCD_H_RES - 1 - area->x2;
    uint16_t physical_x2 = LCD_H_RES - 1 - area->x1;

    // 4. 发射给屏幕：底层 DMA 会在后台自动把这些数据运过去。
    esp_lcd_panel_draw_bitmap(global_panel_handle, physical_x1, area->y1, physical_x2 + 1, area->y2 + 1, buf);
}

lv_disp_t* bsp_lcd_init(void) {
    gpio_config_t bk_gpio_config = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = 1ULL << LCD_PIN_BLK
    };
    gpio_config(&bk_gpio_config);
    // 1. 背光拉高 (开启 PNP 三极管)
    gpio_set_level(LCD_PIN_BLK, 1); 

    // 2. 修建高速公路：配置 SPI 总线，申请 DMA 通道。
    // max_transfer_sz 是关键，它决定了一次最多能运多少像素
    spi_bus_config_t buscfg = {
        .sclk_io_num = LCD_PIN_CLK,
        .mosi_io_num = LCD_PIN_MOSI,
        .miso_io_num = -1,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = LCD_H_RES * 80 * sizeof(uint16_t),
    };
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));

    esp_lcd_panel_io_handle_t io_handle = NULL;
    // 3. 建立调度中心：绑定 CS 和 DC 引脚，设置 SPI 频率
    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = LCD_PIN_DC,
        .cs_gpio_num = LCD_PIN_CS,
        .pclk_hz = LCD_PIXEL_CLOCK_HZ,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .spi_mode = 0,
        .trans_queue_depth = 10,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_config, &io_handle));

    // 4. 挂载 ILI9341 驱动：并告诉它红蓝顺序是 BGR
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = LCD_PIN_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR, 
        .bits_per_pixel = 16,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_ili9341(io_handle, &panel_config, &global_panel_handle)); // 存入全局句柄

    // 5. 解锁屏幕：复位 -> 初始化 -> 反转色彩 (解决发白问题) -> 开启显示
    ESP_ERROR_CHECK(esp_lcd_panel_reset(global_panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(global_panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(global_panel_handle, true)); 
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(global_panel_handle, true));

    // 1. 乐鑫官方的自动接驳库。帮你建好了负责刷新的后台 FreeRTOS 任务
    lvgl_port_cfg_t lvgl_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    lvgl_port_init(&lvgl_cfg);

    // 2. 分配 1/10 屏幕大小的 DMA 显存
    lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = io_handle,
        .panel_handle = global_panel_handle,
        .buffer_size = LCD_H_RES * LCD_V_RES / 10, 
        .double_buffer = 1,
        .hres = LCD_H_RES,
        .vres = LCD_V_RES,
        .monochrome = false,
        .flags = { .buff_dma = true }
    };
    lv_disp_t * disp = lvgl_port_add_disp(&disp_cfg);

    // 强行拔掉乐鑫写好的标准发货员，换成我们上面写的那个自带镜像和色彩修复的黑客外挂
    disp->driver->flush_cb = my_hijacked_flush_cb;
    //lv_disp_set_rotation(disp, LV_DISP_ROT_90);
    return disp;
}
