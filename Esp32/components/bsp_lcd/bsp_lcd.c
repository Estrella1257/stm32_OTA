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
#define LCD_PIXEL_CLOCK_HZ (40 * 1000 * 1000) 
#define LCD_PIN_MOSI   11
#define LCD_PIN_CLK    12
#define LCD_PIN_CS     10
#define LCD_PIN_DC     46   
#define LCD_PIN_RST    -1  
#define LCD_PIN_BLK    45 
#define LCD_H_RES      240
#define LCD_V_RES      320

static esp_lcd_panel_handle_t global_panel_handle = NULL;

// 极速指针级软件镜像 (完美替代失效的硬件镜像)
static void my_fast_mirror_cb(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p)
{
    // 1. 算出当前这块小图像的宽 (w) 和高 (h)
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);
    uint16_t *buf = (uint16_t *)color_p;

    // 极速互换算法：直接操作内存地址，跳过所有位运算
    for (uint32_t y = 0; y < h; y++) {
        uint16_t *row = &buf[y * w]; // 锁定这一行的首地址
        for (uint32_t x = 0; x < w / 2; x++) {
            uint16_t temp = row[x];
            row[x] = row[w - 1 - x];
            row[w - 1 - x] = temp;
        }
    }

    // 映射物理坐标反转
    uint16_t physical_x1 = LCD_H_RES - 1 - area->x2;
    uint16_t physical_x2 = LCD_H_RES - 1 - area->x1;

    // 扔给 DMA 发送
    esp_lcd_panel_draw_bitmap(global_panel_handle, physical_x1, area->y1, physical_x2 + 1, area->y2 + 1, buf);
}

lv_disp_t* bsp_lcd_init(void) {
    gpio_config_t bk_gpio_config = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = 1ULL << LCD_PIN_BLK
    };
    gpio_config(&bk_gpio_config);
    // 1. 背光拉高
    gpio_set_level(LCD_PIN_BLK, 1); 

    // 2. 配置 SPI 总线，申请 DMA 通道
    spi_bus_config_t buscfg = {
        .sclk_io_num = LCD_PIN_CLK,
        .mosi_io_num = LCD_PIN_MOSI,
        .miso_io_num = -1,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = LCD_H_RES * 80 * sizeof(uint16_t),
    };
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));

    // 3. 绑定 CS 和 DC 引脚，设置 SPI 频率
    esp_lcd_panel_io_handle_t io_handle = NULL;
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
    ESP_ERROR_CHECK(esp_lcd_new_panel_ili9341(io_handle, &panel_config, &global_panel_handle)); 

    // 5. 解锁屏幕：复位 -> 初始化 -> 反转色彩 (解决发白问题) -> 开启显示
    ESP_ERROR_CHECK(esp_lcd_panel_reset(global_panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(global_panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(global_panel_handle, true)); 
    
    //ESP_ERROR_CHECK(esp_lcd_panel_mirror(global_panel_handle, true, true)); 

    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(global_panel_handle, true));

    // 1. 乐鑫官方的自动接驳库,帮你建好了负责刷新的后台 FreeRTOS 任务
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
        // 开启硬件 DMA，并要求底层自动处理字节交换 (解决彩色杂边)
        .flags = { .buff_dma = true} 
    };
    lv_disp_t * disp = lvgl_port_add_disp(&disp_cfg);

    // 强行拔掉乐鑫写好的标准发货员，换成我们上面写的那个自带镜像和色彩修复的软件外挂
    disp->driver->flush_cb = my_fast_mirror_cb;

    return disp;
}