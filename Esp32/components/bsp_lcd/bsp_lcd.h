#ifndef BSP_LCD_H
#define BSP_LCD_H

#include "lvgl.h"

// 对外暴露初始化函数，返回创建好的屏幕对象指针
lv_disp_t* bsp_lcd_init(void);

#endif