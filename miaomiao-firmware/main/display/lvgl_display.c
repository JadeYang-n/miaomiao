/*
 * LVGL Display Driver for ST7789
 * 简化自 xiaozhi-esp32 display 实现
 */

#include "lvgl_display.h"
#include "config.h"
#include <esp_log.h>
#include <esp_lcd_types.h>
#include <esp_lcd_io_spi.h>
#include <esp_lcd_panel_st7789.h>
#include <esp_lvgl_port.h>
#include <driver/spi_master.h>
#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *TAG = "LVGLDisplay";

#define LVGL_DISPLAY_WIDTH   240
#define LVGL_DISPLAY_HEIGHT  240

static esp_lcd_panel_io_handle_t s_panel_io = NULL;
static esp_lcd_panel_handle_t s_panel = NULL;
static lv_display_t *s_disp = NULL;

esp_err_t lvgl_display_init(esp_lcd_panel_io_handle_t *panel_io, esp_lcd_panel_handle_t *panel)
{
    ESP_LOGI(TAG, "Initialize SPI bus");
    spi_bus_config_t buscfg = {
        .mosi_io_num = OLED_SDA_PIN,
        .miso_io_num = -1,
        .sclk_io_num = OLED_SCL_PIN,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = LVGL_DISPLAY_WIDTH * LVGL_DISPLAY_HEIGHT * sizeof(uint16_t),
    };
    ESP_ERROR_CHECK(spi_bus_initialize(OLED_SPI_HOST, &buscfg, SPI_DMA_CH_AUTO));

    ESP_LOGI(TAG, "Install panel IO");
    esp_lcd_panel_io_spi_config_t io_config = {
        .cs_gpio_num = OLED_CS_PIN,
        .dc_gpio_num = OLED_DC_PIN,
        .spi_mode = 3,
        .pclk_hz = 40 * 1000 * 1000,
        .trans_queue_depth = 10,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(OLED_SPI_HOST, &io_config, &s_panel_io));

    ESP_LOGI(TAG, "Install ST7789 driver");
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = OLED_RST_PIN,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .data_endian = LCD_RGB_DATA_ENDIAN_LITTLE,  /* 关键：匹配原始工作代码的字节序 */
        .bits_per_pixel = 16,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(s_panel_io, &panel_config, &s_panel));

    ESP_ERROR_CHECK(esp_lcd_panel_reset(s_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(s_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(s_panel, true));
    esp_lcd_panel_swap_xy(s_panel, false);
    esp_lcd_panel_mirror(s_panel, false, false);

    if (panel_io) *panel_io = s_panel_io;
    if (panel) *panel = s_panel;

    return ESP_OK;
}

static bool s_backlight_enabled = false;

lv_display_t* lvgl_display_add(void)
{
    ESP_LOGI(TAG, "Initialize LVGL library");
    lv_init();

    ESP_LOGI(TAG, "Initialize LVGL port");
    lvgl_port_cfg_t port_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    port_cfg.task_priority = 2;
    lvgl_port_init(&port_cfg);

    ESP_LOGI(TAG, "Add LCD display to LVGL");
    const lvgl_port_display_cfg_t display_cfg = {
        .io_handle = s_panel_io,
        .panel_handle = s_panel,
        .buffer_size = LVGL_DISPLAY_WIDTH * 20,
        .double_buffer = false,
        .hres = LVGL_DISPLAY_WIDTH,
        .vres = LVGL_DISPLAY_HEIGHT,
        .color_format = LV_COLOR_FORMAT_RGB565,
        .flags = {
            .buff_dma = 1,
            .buff_spiram = 0,
            .swap_bytes = 1,
        },
    };

    s_disp = lvgl_port_add_disp(&display_cfg);

    /* 不做任何额外操作，让 LVGL port 自动处理一切 */
    /* 开背光 */
    if (OLED_BL_PIN >= 0 && !s_backlight_enabled) {
        vTaskDelay(pdMS_TO_TICKS(500));  /* 等待 LVGL 任务稳定 */
        gpio_set_level(OLED_BL_PIN, 1);
        s_backlight_enabled = true;
        ESP_LOGI(TAG, "Backlight enabled (GPIO %d)", OLED_BL_PIN);
    }

    return s_disp;
}

int lvgl_display_get_width(void)
{
    return LVGL_DISPLAY_WIDTH;
}

int lvgl_display_get_height(void)
{
    return LVGL_DISPLAY_HEIGHT;
}

static lv_obj_t *s_status_label1 = NULL;
static lv_obj_t *s_status_label2 = NULL;

/* 使用 LVGL 显示状态信息 */
esp_err_t display_init(void)
{
    ESP_LOGI(TAG, "display_init called");

    /* 初始化 LVGL 和显示驱动 */
    esp_err_t ret = lvgl_display_init(NULL, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "lvgl_display_init failed: %d", ret);
        return ret;
    }

    lv_display_t *disp = lvgl_display_add();
    if (!disp) {
        ESP_LOGE(TAG, "lvgl_display_add failed");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "LVGL display added successfully");
    ESP_LOGI(TAG, "display_init done");
    return ESP_OK;
}

void display_show_status(const char *line1, const char *line2)
{
    if (s_status_label1 && line1) {
        lv_label_set_text(s_status_label1, line1);
    }
    if (s_status_label2 && line2) {
        lv_label_set_text(s_status_label2, line2);
    }
}

void display_show_message(const char *message)
{
    display_show_status(message, "");
}