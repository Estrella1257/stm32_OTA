#ifndef UI_H
#define UI_H

#include "lvgl.h"

void ui_init(lv_disp_t *disp);
void ui_update_dashboard(int speed, int battery, int gear, bool wifi_on, bool headlight_on, float odo);

#endif 