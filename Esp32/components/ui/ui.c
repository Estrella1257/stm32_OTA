#include "ui.h"
#include <stdio.h>

static lv_obj_t * speed_arc;
static lv_obj_t * speed_label;
static lv_obj_t * gear_label;
static lv_obj_t * battery_bar;
static lv_obj_t * battery_label;
static lv_obj_t * odo_label;
static lv_obj_t * wifi_icon;
static lv_obj_t * headlight_matrix_led;

LV_FONT_DECLARE(lv_font_montserrat_28);

// 独立心脏起搏器：什么都不干，只为了每 20ms 踢醒 LVGL 一次
static void ui_heartbeat_cb(lv_timer_t * timer) {
    (void)timer; 
}

void ui_init(lv_disp_t *disp)
{
    lv_obj_t * scr = lv_disp_get_scr_act(disp);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x050510), 0);

    /* --- 顶部控制台 (Top) --- */
    wifi_icon = lv_label_create(scr);
    lv_label_set_text(wifi_icon, LV_SYMBOL_WIFI);
    lv_obj_set_style_text_color(wifi_icon, lv_color_hex(0x444444), 0); 
    lv_obj_set_style_text_font(wifi_icon, &lv_font_montserrat_28, 0);
    lv_obj_align(wifi_icon, LV_ALIGN_TOP_LEFT, 20, 20);

    headlight_matrix_led = lv_obj_create(scr);
    lv_obj_set_size(headlight_matrix_led, 20, 20); 
    lv_obj_set_style_radius(headlight_matrix_led, 10, 0); 
    lv_obj_set_style_bg_color(headlight_matrix_led, lv_color_hex(0x333333), 0); 
    lv_obj_set_style_border_color(headlight_matrix_led, lv_color_hex(0x1A1D2E), 0); 
    lv_obj_set_style_border_width(headlight_matrix_led, 2, 0);
    lv_obj_align(headlight_matrix_led, LV_ALIGN_TOP_RIGHT, -20, 22);

    /* --- 核心动力源 (Center-Top) --- */
    // 竖屏宽度是 240，所以圆弧做 200，留出完美边距
    speed_arc = lv_arc_create(scr);
    lv_obj_set_size(speed_arc, 200, 200); 
    lv_arc_set_rotation(speed_arc, 135);
    lv_arc_set_bg_angles(speed_arc, 0, 270);
    lv_obj_align(speed_arc, LV_ALIGN_CENTER, 0, -30); // 整体稍微偏上一点
    lv_obj_remove_style(speed_arc, NULL, LV_PART_KNOB); 

    lv_obj_set_style_arc_color(speed_arc, lv_color_hex(0x1A1D2E), LV_PART_MAIN);
    lv_obj_set_style_arc_width(speed_arc, 16, LV_PART_MAIN);

    lv_obj_set_style_arc_color(speed_arc, lv_color_hex(0x00E5FF), LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(speed_arc, 16, LV_PART_INDICATOR);
    lv_obj_set_style_shadow_color(speed_arc, lv_color_hex(0x00E5FF), LV_PART_INDICATOR);
    lv_obj_set_style_shadow_width(speed_arc, 20, LV_PART_INDICATOR); 

    speed_label = lv_label_create(speed_arc); // 让数字挂载在圆弧内部
    lv_label_set_text(speed_label, "00");
    lv_obj_set_style_text_color(speed_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(speed_label, &lv_font_montserrat_28, 0); 
    lv_obj_align(speed_label, LV_ALIGN_CENTER, 0, -10);

    lv_obj_t * unit_label = lv_label_create(speed_arc);
    lv_label_set_text(unit_label, "km/h");
    lv_obj_set_style_text_color(unit_label, lv_color_hex(0x00E5FF), 0); 
    lv_obj_align_to(unit_label, speed_label, LV_ALIGN_OUT_BOTTOM_MID, 0, 5);

    gear_label = lv_label_create(scr);
    lv_label_set_text(gear_label, "P");
    lv_obj_set_style_text_color(gear_label, lv_color_hex(0xFF0055), 0); 
    lv_obj_set_style_text_font(gear_label, &lv_font_montserrat_28, 0);
    lv_obj_align_to(gear_label, speed_arc, LV_ALIGN_OUT_BOTTOM_MID, 0, -20); // 压在圆弧底部

    /* --- 底部装甲 (Bottom) --- */
    // 竖屏下，电池做成底部的横向能量条
    battery_bar = lv_bar_create(scr);
    lv_obj_set_size(battery_bar, 180, 14); 
    lv_obj_align(battery_bar, LV_ALIGN_BOTTOM_MID, 0, -50);
    lv_bar_set_range(battery_bar, 0, 100);
    
    lv_obj_set_style_bg_color(battery_bar, lv_color_hex(0x1A1D2E), LV_PART_MAIN);
    lv_obj_set_style_bg_color(battery_bar, lv_color_hex(0x39FF14), LV_PART_INDICATOR); 
    lv_obj_set_style_shadow_color(battery_bar, lv_color_hex(0x39FF14), LV_PART_INDICATOR);
    lv_obj_set_style_shadow_width(battery_bar, 15, LV_PART_INDICATOR);

    battery_label = lv_label_create(scr);
    lv_label_set_text(battery_label, "100%");
    lv_obj_set_style_text_color(battery_label, lv_color_hex(0x39FF14), 0);
    lv_obj_align_to(battery_label, battery_bar, LV_ALIGN_OUT_TOP_MID, 0, -5);

    odo_label = lv_label_create(scr);
    lv_label_set_text(odo_label, "ODO: 0.0 km");
    lv_obj_set_style_text_color(odo_label, lv_color_hex(0x888888), 0);
    lv_obj_align(odo_label, LV_ALIGN_BOTTOM_MID, 0, -10);

    lv_timer_create(ui_heartbeat_cb, 20, NULL);
}

// 这里的更新逻辑完全不用变！只需要接管 UI 就行！
void ui_update_dashboard(int speed, int battery, int gear, bool wifi_on, bool headlight_on, float odo)
{
    if(!speed_label) return; 

    lv_arc_set_value(speed_arc, speed * 100 / 120);
    lv_label_set_text_fmt(speed_label, "%02d", speed);

    lv_bar_set_value(battery_bar, battery, LV_ANIM_ON);
    lv_label_set_text_fmt(battery_label, "%d%%", battery);

    if(battery <= 20) {
        lv_obj_set_style_bg_color(battery_bar, lv_color_hex(0xFF0000), LV_PART_INDICATOR);
        lv_obj_set_style_shadow_color(battery_bar, lv_color_hex(0xFF0000), LV_PART_INDICATOR);
        lv_obj_set_style_text_color(battery_label, lv_color_hex(0xFF0000), 0);
    } else {
        lv_obj_set_style_bg_color(battery_bar, lv_color_hex(0x39FF14), LV_PART_INDICATOR);
        lv_obj_set_style_shadow_color(battery_bar, lv_color_hex(0x39FF14), LV_PART_INDICATOR);
        lv_obj_set_style_text_color(battery_label, lv_color_hex(0x39FF14), 0);
    }

    switch(gear) {
        case 0: lv_label_set_text(gear_label, "P"); lv_obj_set_style_text_color(gear_label, lv_color_hex(0xFF0055), 0); break;
        case 1: lv_label_set_text(gear_label, "ECO"); lv_obj_set_style_text_color(gear_label, lv_color_hex(0x39FF14), 0); break;
        case 2: lv_label_set_text(gear_label, "D"); lv_obj_set_style_text_color(gear_label, lv_color_hex(0x00E5FF), 0); break;
        case 3: lv_label_set_text(gear_label, "SPORT"); lv_obj_set_style_text_color(gear_label, lv_color_hex(0xFFB300), 0); break;
    }
    lv_obj_align_to(gear_label, speed_arc, LV_ALIGN_OUT_BOTTOM_MID, 0, -40);

    if (wifi_on) lv_obj_set_style_text_color(wifi_icon, lv_color_hex(0x00E5FF), 0);
    else lv_obj_set_style_text_color(wifi_icon, lv_color_hex(0x444444), 0);

    if (headlight_on) {
        lv_obj_set_style_bg_color(headlight_matrix_led, lv_color_hex(0xFFB300), 0);
        lv_obj_set_style_shadow_color(headlight_matrix_led, lv_color_hex(0xFFB300), 0);
        lv_obj_set_style_shadow_width(headlight_matrix_led, 20, 0); 
    } else {
        lv_obj_set_style_bg_color(headlight_matrix_led, lv_color_hex(0x333333), 0);
        lv_obj_set_style_shadow_width(headlight_matrix_led, 0, 0); 
    }
    int odo_int = (int)odo;
    int odo_dec = (int)(odo * 10) % 10;
    lv_label_set_text_fmt(odo_label, "ODO: %d.%d km", odo_int, odo_dec);
}