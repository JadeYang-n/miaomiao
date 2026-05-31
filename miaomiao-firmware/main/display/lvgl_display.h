/*
 * LVGL Display Driver for ST7789
 * 基于 xiaozhi-esp32 的 display 实现简化
 */

#ifndef LVGL_DISPLAY_H_
#define LVGL_DISPLAY_H_

#include <esp_err.h>
#include <esp_lcd_types.h>
#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化 LVGL 显示驱动
 *
 * @param panel_io OUT: panel IO handle
 * @param panel OUT: panel handle
 * @return esp_err_t
 */
esp_err_t lvgl_display_init(esp_lcd_panel_io_handle_t *panel_io, esp_lcd_panel_handle_t *panel);

/**
 * @brief 添加 LVGL 显示到端口
 *
 * @return lv_display_t* LVGL 显示句柄
 */
lv_display_t* lvgl_display_add(void);

/**
 * @brief 获取显示宽度
 */
int lvgl_display_get_width(void);

/**
 * @brief 获取显示高度
 */
int lvgl_display_get_height(void);

/* Display interface - compatible with existing code */
esp_err_t display_init(void);
void display_show_status(const char *line1, const char *line2);
void display_show_message(const char *message);

#ifdef __cplusplus
}
#endif

#endif /* LVGL_DISPLAY_H_ */