/*
 * Stub Display Driver - provides no-op implementations when LVGL is disabled
 */

#include "display_stub.h"
#include <esp_log.h>

static const char *TAG = "DisplayStub";

esp_err_t display_init(void) {
    ESP_LOGI(TAG, "Display stub - LVGL disabled");
    return ESP_OK;
}

void display_clear(void) {
}

void display_show_logo(void) {
}

void display_show_status(const char *line1, const char *line2) {
    ESP_LOGI(TAG, "[%s] [%s]", line1 ? line1 : "", line2 ? line2 : "");
}

void display_show_message(const char *speaker, const char *message) {
    ESP_LOGI(TAG, "[%s] %s", speaker ? speaker : "", message ? message : "");
}

void display_set_expression(int expression) {
}

void display_show_warning(void) {
}

void display_clear_warning(void) {
}