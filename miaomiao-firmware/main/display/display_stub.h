/*
 * Stub Display Driver - provides no-op implementations when LVGL is disabled
 */

#ifndef DISPLAY_STUB_H_
#define DISPLAY_STUB_H_

#include <esp_err.h>

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t display_init(void);
void display_clear(void);
void display_show_logo(void);
void display_show_status(const char *line1, const char *line2);
void display_show_message(const char *speaker, const char *message);
void display_set_expression(int expression);
void display_show_warning(void);
void display_clear_warning(void);

#ifdef __cplusplus
}
#endif

#endif /* DISPLAY_STUB_H_ */