#ifndef UI_H
#define UI_H

#include "lvgl.h"

// 暴露给外部的函数：初始化并画出静态的仪表盘
void ui_init(lv_disp_t *disp);

// 暴露给外部的函数：更新仪表盘的速度指针和数字
//void ui_update_speed(int speed);
void ui_update_dashboard(int speed, int battery, int gear, bool wifi_on, bool headlight_on, float odo);

#endif // DASHBOARD_UI_H