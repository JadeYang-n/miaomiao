/**
 * ST7789 TFT LCD 显示屏驱动 (SPI)
 *
 * 显示苗苗头像和对话内容
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <esp_system.h>
#include <driver/gpio.h>
#include <driver/spi_master.h>
#include <esp_lcd_types.h>
#include <esp_lcd_io_spi.h>
#include <esp_lcd_panel_st7789.h>
#include <esp_lcd_panel_ops.h>
#include <esp_err.h>
#include <esp_random.h>

#include "config.h"
#include "cat_sprites.h"

/* 前向声明 */
static void display_init_animation(void);

/* 表情状态枚举 */
typedef enum {
    EXPRESSION_NORMAL = 0,
    EXPRESSION_BLINKING,
    EXPRESSION_TALKING,
    EXPRESSION_THINKING,
    EXPRESSION_ALERT,      /* 中风险 - 警惕 */
    EXPRESSION_WARNING,    /* 高风险 - 警告 */
} miao_expression_t;

/* 精灵姿态枚举 */
typedef enum {
    SPRITE_STAND = 0,
    SPRITE_SIT,
    SPRITE_SIT_HAPPY,
    SPRITE_SIT_THINK,
    SPRITE_SIT_BLINK,
    SPRITE_SIT_ALERT,
    SPRITE_WARNING,
    SPRITE_SLEEP,
    SPRITE_BELLY,
} miao_sprite_t;

static const char *TAG = "DISPLAY";

/* ST7789 屏幕分辨率 */
#define ST7789_WIDTH   240
#define ST7789_HEIGHT  240

/* 动画参数 */
#define BLINK_INTERVAL_MIN_MS  3000   /* 眨眼间隔最小 3 秒 */
#define BLINK_INTERVAL_MAX_MS  5000   /* 眨眼间隔最大 5 秒 */
#define BLINK_DURATION_MS      150    /* 眨眼持续 150ms */

/* 颜色定义 (RGB565) - invert_color(true) 会自动反转 */
#define COLOR_BLACK   0x0000
#define COLOR_WHITE   0xFFFF
#define COLOR_RED     0xF800
#define COLOR_GREEN   0x07E0
#define COLOR_BLUE    0x001F
#define COLOR_YELLOW  0xFFE0
#define COLOR_CYAN    0x07FF
#define COLOR_MAGENTA 0xF81F

/* 字体定义（8x16 像素 ASCII） */
static const uint8_t font_data[95][16] = {
    /* 空格 */ {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    /* ! */ {0x04,0x04,0x04,0x04,0x04,0x04,0x00,0x00,0x00,0x04,0x00,0x00,0x00,0x00,0x00,0x00},
    /* " */ {0x00,0x00,0x14,0x14,0x08,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    /* # */ {0x00,0x00,0x14,0x14,0x3E,0x14,0x1C,0x00,0x00,0x00,0x1C,0x3E,0x14,0x14,0x00,0x00},
    /* $ */ {0x04,0x1F,0x24,0x44,0x3E,0x0A,0x72,0x00,0x00,0x08,0x1C,0x3E,0x08,0x08,0x00,0x00},
    /* % */ {0x00,0x62,0x94,0x48,0x32,0x4A,0x80,0x00,0x00,0x02,0x31,0x12,0x4C,0x90,0x60,0x00},
    /* & */ {0x00,0x30,0x48,0x30,0x56,0x88,0x76,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    /* ' */ {0x00,0x04,0x08,0x10,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    /* ( */ {0x00,0x08,0x10,0x20,0x20,0x10,0x08,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    /* ) */ {0x00,0x20,0x10,0x08,0x08,0x10,0x20,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    /* * */ {0x00,0x00,0x24,0x18,0x18,0x24,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    /* + */ {0x00,0x00,0x04,0x04,0x3E,0x04,0x04,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    /* , */ {0x00,0x00,0x00,0x00,0x00,0x00,0x08,0x10,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    /* - */ {0x00,0x00,0x00,0x00,0x3E,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    /* . */ {0x00,0x00,0x00,0x00,0x00,0x00,0x04,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    /* / */ {0x00,0x02,0x04,0x08,0x10,0x20,0x40,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    /* 0 */ {0x00,0x3C,0x42,0x42,0x42,0x42,0x3C,0x00,0x00,0x3C,0x42,0x42,0x42,0x42,0x3C,0x00},
    /* 1 */ {0x00,0x08,0x18,0x08,0x08,0x08,0x1C,0x00,0x00,0x08,0x08,0x08,0x08,0x08,0x1C,0x00},
    /* 2 */ {0x00,0x3C,0x42,0x02,0x0C,0x30,0x7E,0x00,0x00,0x3C,0x42,0x02,0x0C,0x30,0x7E,0x00},
    /* 3 */ {0x00,0x3C,0x42,0x02,0x1C,0x02,0x7E,0x00,0x00,0x3C,0x42,0x02,0x1C,0x02,0x7E,0x00},
    /* 4 */ {0x00,0x0C,0x14,0x24,0x7E,0x04,0x04,0x00,0x00,0x04,0x04,0x04,0x3E,0x04,0x04,0x00},
    /* 5 */ {0x00,0x7E,0x40,0x7C,0x02,0x42,0x3C,0x00,0x00,0x3C,0x42,0x40,0x7C,0x02,0x7E,0x00},
    /* 6 */ {0x00,0x1C,0x20,0x40,0x7C,0x42,0x3C,0x00,0x00,0x3C,0x42,0x40,0x7C,0x42,0x3C,0x00},
    /* 7 */ {0x00,0x7E,0x02,0x04,0x08,0x10,0x10,0x00,0x00,0x08,0x10,0x10,0x10,0x10,0x10,0x00},
    /* 8 */ {0x00,0x3C,0x42,0x3C,0x42,0x42,0x3C,0x00,0x00,0x3C,0x42,0x42,0x3C,0x42,0x3C,0x00},
    /* 9 */ {0x00,0x3C,0x42,0x42,0x3E,0x02,0x3C,0x00,0x00,0x3C,0x42,0x42,0x3E,0x02,0x3C,0x00},
    /* : */ {0x00,0x00,0x04,0x00,0x00,0x04,0x00,0x00,0x00,0x00,0x04,0x00,0x00,0x04,0x00,0x00},
    /* ; */ {0x00,0x00,0x04,0x00,0x00,0x04,0x08,0x10,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    /* < */ {0x00,0x04,0x08,0x10,0x08,0x04,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    /* = */ {0x00,0x00,0x00,0x3E,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x3E,0x00,0x00,0x00,0x00},
    /* > */ {0x00,0x10,0x08,0x04,0x08,0x10,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    /* ? */ {0x00,0x3C,0x42,0x02,0x04,0x00,0x04,0x00,0x00,0x00,0x04,0x00,0x00,0x00,0x00,0x00},
    /* @ */ {0x00,0x3C,0x42,0x5E,0x52,0x5E,0x40,0x3C,0x00,0x3C,0x40,0x52,0x5E,0x42,0x3C,0x00},
    /* A */ {0x00,0x1C,0x22,0x22,0x3E,0x22,0x22,0x00,0x00,0x3C,0x42,0x42,0x7E,0x42,0x42,0x00},
    /* B */ {0x00,0x7C,0x42,0x7C,0x42,0x42,0x7C,0x00,0x00,0x7C,0x42,0x42,0x7C,0x42,0x7C,0x00},
    /* C */ {0x00,0x3C,0x42,0x40,0x40,0x42,0x3C,0x00,0x00,0x3C,0x42,0x40,0x40,0x42,0x3C,0x00},
    /* D */ {0x00,0x78,0x44,0x42,0x42,0x44,0x78,0x00,0x00,0x78,0x44,0x42,0x42,0x44,0x78,0x00},
    /* E */ {0x00,0x7E,0x40,0x7C,0x40,0x40,0x7E,0x00,0x00,0x7E,0x40,0x7C,0x40,0x40,0x7E,0x00},
    /* F */ {0x00,0x7E,0x40,0x7C,0x40,0x40,0x40,0x00,0x00,0x7E,0x40,0x7C,0x40,0x40,0x40,0x00},
    /* G */ {0x00,0x3C,0x42,0x40,0x4E,0x42,0x3C,0x00,0x00,0x3C,0x42,0x40,0x4E,0x42,0x3C,0x00},
    /* H */ {0x00,0x22,0x22,0x3E,0x22,0x22,0x22,0x00,0x00,0x42,0x42,0x7E,0x42,0x42,0x42,0x00},
    /* I */ {0x00,0x1C,0x04,0x04,0x04,0x04,0x1C,0x00,0x00,0x1C,0x04,0x04,0x04,0x04,0x1C,0x00},
    /* J */ {0x00,0x0E,0x04,0x04,0x04,0x44,0x38,0x00,0x00,0x0E,0x04,0x04,0x04,0x44,0x38,0x00},
    /* K */ {0x00,0x42,0x44,0x78,0x48,0x44,0x42,0x00,0x00,0x42,0x44,0x78,0x48,0x44,0x42,0x00},
    /* L */ {0x00,0x40,0x40,0x40,0x40,0x40,0x7E,0x00,0x00,0x7E,0x40,0x40,0x40,0x40,0x40,0x00},
    /* M */ {0x00,0x62,0x56,0x4A,0x42,0x42,0x42,0x00,0x00,0x42,0x42,0x42,0x4A,0x56,0x62,0x00},
    /* N */ {0x00,0x22,0x62,0x52,0x4A,0x46,0x42,0x00,0x00,0x42,0x42,0x4A,0x52,0x62,0x22,0x00},
    /* O */ {0x00,0x3C,0x42,0x42,0x42,0x42,0x3C,0x00,0x00,0x3C,0x42,0x42,0x42,0x42,0x3C,0x00},
    /* P */ {0x00,0x7C,0x42,0x7C,0x40,0x40,0x40,0x00,0x00,0x7C,0x42,0x7C,0x40,0x40,0x40,0x00},
    /* Q */ {0x00,0x3C,0x42,0x42,0x4A,0x44,0x3A,0x00,0x00,0x3C,0x42,0x42,0x4A,0x44,0x3A,0x00},
    /* R */ {0x00,0x7C,0x42,0x7C,0x48,0x44,0x42,0x00,0x00,0x7C,0x42,0x7C,0x48,0x44,0x42,0x00},
    /* S */ {0x00,0x3C,0x40,0x3C,0x02,0x42,0x3C,0x00,0x00,0x3C,0x40,0x3C,0x02,0x42,0x3C,0x00},
    /* T */ {0x00,0x7E,0x04,0x04,0x04,0x04,0x04,0x00,0x00,0x04,0x04,0x04,0x04,0x04,0x7E,0x00},
    /* U */ {0x00,0x22,0x22,0x22,0x22,0x22,0x3C,0x00,0x00,0x3C,0x22,0x22,0x22,0x22,0x22,0x00},
    /* V */ {0x00,0x11,0x11,0x22,0x22,0x44,0x44,0x00,0x00,0x44,0x44,0x22,0x22,0x11,0x11,0x00},
    /* W */ {0x00,0x41,0x41,0x49,0x49,0x55,0xAA,0x00,0x00,0xAA,0x55,0x49,0x49,0x41,0x41,0x00},
    /* X */ {0x00,0x42,0x24,0x18,0x18,0x24,0x42,0x00,0x00,0x42,0x24,0x18,0x18,0x24,0x42,0x00},
    /* Y */ {0x00,0x08,0x08,0x10,0x10,0x10,0x7C,0x00,0x00,0x7C,0x10,0x10,0x10,0x08,0x08,0x00},
    /* Z */ {0x00,0x7E,0x02,0x04,0x08,0x10,0x7E,0x00,0x00,0x7E,0x10,0x08,0x04,0x02,0x7E,0x00},
    /* [ */ {0x00,0x18,0x10,0x10,0x10,0x10,0x18,0x00,0x00,0x18,0x10,0x10,0x10,0x10,0x18,0x00},
    /* \ */ {0x00,0x80,0x40,0x20,0x10,0x08,0x04,0x00,0x00,0x04,0x08,0x10,0x20,0x40,0x80,0x00},
    /* ] */ {0x00,0x06,0x02,0x02,0x02,0x02,0x06,0x00,0x00,0x06,0x02,0x02,0x02,0x02,0x06,0x00},
    /* ^ */ {0x00,0x04,0x08,0x10,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    /* _ */ {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x7E,0x00,0x7E,0x00,0x00,0x00,0x00,0x00,0x00},
    /* ` */ {0x00,0x08,0x10,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    /* a */ {0x00,0x00,0x00,0x3C,0x02,0x3E,0x42,0x00,0x00,0x3C,0x02,0x3E,0x42,0x42,0x3C,0x00},
    /* b */ {0x00,0x40,0x40,0x5C,0x62,0x42,0x7C,0x00,0x00,0x7C,0x42,0x62,0x5C,0x40,0x40,0x00},
    /* c */ {0x00,0x00,0x00,0x3C,0x40,0x40,0x3C,0x00,0x00,0x3C,0x40,0x40,0x40,0x00,0x00,0x00},
    /* d */ {0x00,0x02,0x02,0x3A,0x42,0x42,0x3E,0x00,0x00,0x3E,0x42,0x42,0x3A,0x02,0x02,0x00},
    /* e */ {0x00,0x00,0x00,0x3C,0x42,0x7C,0x40,0x00,0x00,0x3C,0x42,0x7C,0x40,0x00,0x00,0x00},
    /* f */ {0x00,0x0C,0x12,0x10,0x3C,0x10,0x10,0x00,0x00,0x10,0x10,0x3C,0x10,0x12,0x0C,0x00},
    /* g */ {0x00,0x00,0x3E,0x42,0x3E,0x02,0x3C,0x00,0x00,0x3C,0x02,0x3E,0x42,0x3E,0x00,0x00},
    /* h */ {0x00,0x40,0x40,0x5C,0x42,0x42,0x42,0x00,0x00,0x42,0x42,0x5C,0x40,0x40,0x40,0x00},
    /* i */ {0x00,0x04,0x00,0x0C,0x04,0x04,0x0C,0x00,0x00,0x0C,0x04,0x04,0x04,0x04,0x0C,0x00},
    /* j */ {0x00,0x08,0x00,0x18,0x08,0x08,0x70,0x00,0x00,0x70,0x08,0x08,0x08,0x08,0x18,0x00},
    /* k */ {0x00,0x40,0x44,0x48,0x70,0x48,0x44,0x00,0x00,0x44,0x48,0x70,0x48,0x44,0x40,0x00},
    /* l */ {0x00,0x0C,0x04,0x04,0x04,0x04,0x0C,0x00,0x00,0x0C,0x04,0x04,0x04,0x04,0x0C,0x00},
    /* m */ {0x00,0x00,0x00,0x6C,0x52,0x52,0x4C,0x00,0x00,0x4C,0x52,0x52,0x6C,0x00,0x00,0x00},
    /* n */ {0x00,0x00,0x00,0x5C,0x62,0x42,0x42,0x00,0x00,0x42,0x42,0x5C,0x00,0x00,0x00,0x00},
    /* o */ {0x00,0x00,0x00,0x3C,0x42,0x42,0x3C,0x00,0x00,0x3C,0x42,0x42,0x3C,0x00,0x00,0x00},
    /* p */ {0x00,0x00,0x00,0x7C,0x42,0x7C,0x40,0x40,0x00,0x40,0x40,0x7C,0x42,0x7C,0x00,0x00},
    /* q */ {0x00,0x00,0x00,0x3E,0x42,0x3E,0x02,0x02,0x00,0x02,0x02,0x3E,0x42,0x3E,0x00,0x00},
    /* r */ {0x00,0x00,0x00,0x5C,0x60,0x40,0x40,0x00,0x00,0x40,0x40,0x5C,0x00,0x00,0x00,0x00},
    /* s */ {0x00,0x00,0x00,0x3E,0x40,0x3C,0x02,0x3C,0x00,0x3C,0x02,0x3C,0x40,0x3E,0x00,0x00},
    /* t */ {0x00,0x00,0x10,0x3C,0x10,0x12,0x0C,0x00,0x00,0x0C,0x12,0x10,0x3C,0x10,0x00,0x00},
    /* u */ {0x00,0x00,0x00,0x22,0x22,0x26,0x1A,0x00,0x00,0x1A,0x26,0x22,0x22,0x00,0x00,0x00},
    /* v */ {0x00,0x00,0x00,0x11,0x22,0x44,0x44,0x00,0x00,0x44,0x44,0x22,0x11,0x00,0x00,0x00},
    /* w */ {0x00,0x00,0x00,0x49,0x49,0x55,0xAA,0x00,0x00,0xAA,0x55,0x49,0x49,0x00,0x00,0x00},
    /* x */ {0x00,0x00,0x00,0x42,0x24,0x18,0x24,0x42,0x00,0x42,0x24,0x18,0x24,0x42,0x00,0x00},
    /* y */ {0x00,0x00,0x00,0x22,0x26,0x1A,0x22,0x1C,0x00,0x1C,0x22,0x1A,0x26,0x22,0x00,0x00},
    /* z */ {0x00,0x00,0x00,0x7E,0x04,0x18,0x7E,0x00,0x00,0x7E,0x18,0x04,0x7E,0x00,0x00,0x00},
    /* { */ {0x00,0x18,0x10,0x10,0x1C,0x10,0x18,0x00,0x00,0x18,0x10,0x10,0x1C,0x10,0x18,0x00},
    /* | */ {0x00,0x04,0x04,0x04,0x04,0x04,0x04,0x04,0x00,0x04,0x04,0x04,0x04,0x04,0x04,0x04},
    /* } */ {0x00,0x60,0x20,0x20,0x38,0x20,0x60,0x00,0x00,0x60,0x20,0x20,0x38,0x20,0x60,0x00},
    /* ~ */ {0x00,0x00,0x00,0x00,0x18,0x06,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
};

/* 16x16 中文字体 (简化版，仅苗苗两个字) */
static const uint8_t miao_char[] = {
    /* 苗 - 16x16 bitmap */
    0x00,0x00,0x07,0xF8,0x08,0x20,0x08,0x20,0x08,0x20,0x08,0x20,0x0F,0xF8,0x00,0x00,
    0x00,0x00,0x00,0x00,0x1F,0xF0,0x10,0x10,0x10,0x10,0x1F,0xF0,0x10,0x10,0x10,0x10,
    /* 苗 */
    0x00,0x00,0x07,0xF8,0x08,0x20,0x08,0x20,0x08,0x20,0x08,0x20,0x0F,0xF8,0x00,0x00,
    0x00,0x00,0x00,0x00,0x1F,0xF0,0x10,0x10,0x10,0x10,0x1F,0xF0,0x10,0x10,0x10,0x10,
};

static bool display_initialized = false;
static esp_lcd_panel_handle_t panel_handle = NULL;
static bool backlight_on = false;
static char last_status_line1[32] = {0};
static char last_status_line2[32] = {0};
static uint32_t last_draw_time = 0;
static bool content_drawn = false;

/* 上次绘制内容的哈希值，用于快速比较 */
static uint32_t last_content_hash = 0;

/* 动画状态 */
static miao_expression_t current_expression = EXPRESSION_NORMAL;
static miao_expression_t target_expression = EXPRESSION_NORMAL;
static bool is_blinking = false;
static int64_t next_blink_time_ms = 0;
static TaskHandle_t animation_task_handle = NULL;
static bool cat_face_drawn = false;
static int cat_face_x = 104;
static int cat_face_y = 80;

/* 精灵状态 */
static bool sprite_drawn = false;
static int sprite_x = 45;  /* 居中: (240-150)/2 = 45 */
static int sprite_y = 45;

/* 空闲状态跟踪 */
static bool showing_idle = false;
static int64_t last_interaction_time = 0;
static miao_sprite_t current_idle_sprite = SPRITE_SLEEP;

/* 显示更新锁 - 防止动画任务和主任务同时更新显示 */
static SemaphoreHandle_t display_update_lock = NULL;

/* 初始化显示锁 */
static void display_lock_init(void) {
    if (display_update_lock == NULL) {
        display_update_lock = xSemaphoreCreateMutex();
    }
}

/* 获取显示锁 */
static bool display_try_lock(void) {
    display_lock_init();
    return xSemaphoreTake(display_update_lock, pdMS_TO_TICKS(100)) == pdTRUE;
}

/* 释放显示锁 */
static void display_unlock(void) {
    if (display_update_lock != NULL) {
        xSemaphoreGive(display_update_lock);
    }
}

static uint32_t hash_string(const char *s)
{
    uint32_t h = 0;
    while (*s) {
        h = h * 31 + (uint8_t)(*s);
        s++;
    }
    return h;
}

/* 防抖：避免短时间内多次刷新屏幕 */
static bool should_redraw(void)
{
    uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
    if (now - last_draw_time < 500) {  /* 恢复500ms，正常更新 */
        return false;  /* 距离上次绘制不足 500ms，不重绘 */
    }
    return true;
}

/* 确保背光打开 - 只设置一次方向，后续只设置电平 */
static void ensure_backlight(void)
{
    if (OLED_BL_PIN >= 0) {
        if (!backlight_on) {
            gpio_set_direction(OLED_BL_PIN, GPIO_MODE_OUTPUT);
            backlight_on = true;
        }
        gpio_set_level(OLED_BL_PIN, 1);
    }
}

/* 测试用：画纯色诊断 */
static void draw_color_test(void)
{
    if (!display_initialized) return;

    /* 打开显示 */
    esp_lcd_panel_disp_on_off(panel_handle, true);
    vTaskDelay(pdMS_TO_TICKS(100));

    // 先画白屏测试背光和基本显示
    uint16_t white = 0xFFFF;
    uint16_t line_buf[ST7789_WIDTH];
    for (int i = 0; i < ST7789_WIDTH; i++) line_buf[i] = white;
    for (int y = 0; y < ST7789_HEIGHT; y++) {
        esp_lcd_panel_draw_bitmap(panel_handle, 0, y, ST7789_WIDTH, y + 1, line_buf);
    }
    ESP_LOGI(TAG, "白屏已绘制");
}

/* 保持屏幕常亮 - 定期重绘防止进入睡眠 */
static void keep_display_alive(void)
{
    if (!display_initialized) return;
    /* 强制刷新显示，防止 ST7789 进入睡眠模式 */
    esp_lcd_panel_disp_on_off(panel_handle, true);
    if (OLED_BL_PIN >= 0 && !backlight_on) {
        gpio_set_level(OLED_BL_PIN, 1);
        backlight_on = true;
    }
}

/* 填充矩形 */
static void fill_rect(int x, int y, int w, int h, uint16_t color)
{
    if (!display_initialized || panel_handle == NULL) return;
    uint16_t line_buf[ST7789_WIDTH];
    for (int i = 0; i < w; i++) {
        line_buf[i] = color;
    }
    for (int row = y; row < y + h; row++) {
        esp_lcd_panel_draw_bitmap(panel_handle, x, row, x + w, row + 1, line_buf);
    }
}

/* 绘制精灵图 */
static void draw_sprite(miao_sprite_t sprite, int x, int y)
{
    if (!display_initialized || panel_handle == NULL) return;

    const uint16_t *data = NULL;
    uint16_t width = 0;
    uint16_t height = 0;

    switch (sprite) {
        case SPRITE_STAND:
            data = CAT_STAND_DATA;
            width = CAT_STAND_WIDTH;
            height = CAT_STAND_HEIGHT;
            break;
        case SPRITE_SIT:
            data = CAT_SIT_DATA;
            width = CAT_SIT_WIDTH;
            height = CAT_SIT_HEIGHT;
            break;
        case SPRITE_SIT_HAPPY:
            data = CAT_SIT_HAPPY_DATA;
            width = CAT_SIT_HAPPY_WIDTH;
            height = CAT_SIT_HAPPY_HEIGHT;
            break;
        case SPRITE_SIT_THINK:
            data = CAT_SIT_THINK_DATA;
            width = CAT_SIT_THINK_WIDTH;
            height = CAT_SIT_THINK_HEIGHT;
            break;
        case SPRITE_SIT_BLINK:
            data = CAT_SIT_BLINK_DATA;
            width = CAT_SIT_BLINK_WIDTH;
            height = CAT_SIT_BLINK_HEIGHT;
            break;
        case SPRITE_SIT_ALERT:
            data = CAT_SIT_ALERT_DATA;
            width = CAT_SIT_ALERT_WIDTH;
            height = CAT_SIT_ALERT_HEIGHT;
            break;
        case SPRITE_WARNING:
            data = CAT_WARNING_DATA;
            width = CAT_WARNING_WIDTH;
            height = CAT_WARNING_HEIGHT;
            break;
        case SPRITE_SLEEP:
            data = CAT_SLEEP_DATA;
            width = CAT_SLEEP_WIDTH;
            height = CAT_SLEEP_HEIGHT;
            break;
        case SPRITE_BELLY:
            data = CAT_BELLY_DATA;
            width = CAT_BELLY_WIDTH;
            height = CAT_BELLY_HEIGHT;
            break;
        default:
            data = CAT_SIT_HAPPY_DATA;
            width = CAT_SIT_HAPPY_WIDTH;
            height = CAT_SIT_HAPPY_HEIGHT;
            break;
    }

    /* 逐行绘制精灵 */
    for (int row = 0; row < height; row++) {
        esp_lcd_panel_draw_bitmap(panel_handle, x, y + row, x + width, y + row + 1, &data[row * width]);
    }
}

/* 根据表情选择精灵姿态 */
static miao_sprite_t expression_to_sprite(miao_expression_t expr)
{
    switch (expr) {
        case EXPRESSION_BLINKING:
            return SPRITE_SIT_BLINK;
        case EXPRESSION_TALKING:
            return SPRITE_SIT_HAPPY;
        case EXPRESSION_THINKING:
            return SPRITE_SIT_THINK;
        case EXPRESSION_ALERT:
            return SPRITE_SIT_ALERT;
        case EXPRESSION_WARNING:
            return SPRITE_WARNING;
        case EXPRESSION_NORMAL:
        default:
            return SPRITE_SIT_HAPPY;
    }
}

/* 空闲时随机姿态（睡觉、站立、翻肚皮） */
static miao_sprite_t get_idle_sprite(void)
{
    uint32_t rand = esp_random();
    uint32_t choice = rand % 3;
    if (choice == 0) {
        return SPRITE_SLEEP;
    } else if (choice == 1) {
        return SPRITE_STAND;
    } else {
        return SPRITE_BELLY;
    }
}

/* 重绘苗苗精灵区域 */
static void redraw_sprite(void)
{
    if (!display_initialized || !sprite_drawn) return;

    miao_sprite_t sprite;
    if (showing_idle) {
        sprite = current_idle_sprite;
    } else {
        sprite = expression_to_sprite(current_expression);
    }
    draw_sprite(sprite, sprite_x, sprite_y);
}

/* 绘制 ASCII 字符 */
static void draw_char(int x, int y, char c, uint16_t fg, uint16_t bg)
{
    if (c < ' ' || c > '~') return;
    int index = c - ' ';
    const uint8_t *glyph = font_data[index];

    for (int row = 0; row < 16; row++) {
        uint16_t col_buf[8];
        uint8_t mask = glyph[row];
        for (int col = 0; col < 8; col++) {
            bool on = (mask >> (7 - col)) & 1;
            col_buf[col] = on ? fg : bg;
        }
        esp_lcd_panel_draw_bitmap(panel_handle, x, y + row, x + 8, y + row + 1, col_buf);
    }
}

/* 绘制中文字符 */
static void draw_miao_char(int x, int y, uint16_t fg, uint16_t bg)
{
    for (int row = 0; row < 16; row++) {
        uint16_t col_buf[16];
        for (int col = 0; col < 16; col++) {
            uint8_t byte = miao_char[row * 2 + (col / 8)];
            bool on = (byte >> (7 - (col % 8))) & 1;
            col_buf[col] = on ? fg : bg;
        }
        esp_lcd_panel_draw_bitmap(panel_handle, x, y + row, x + 16, y + row + 1, col_buf);
    }
}

/* 绘制字符串 - 支持 UTF-8 中文 */
static void draw_string(int x, int y, const char *str, uint16_t fg, uint16_t bg)
{
    int cursor_x = x;
    while (*str) {
        if (cursor_x + 8 > ST7789_WIDTH) {
            cursor_x = x;
            y += 18;
            if (y + 16 > ST7789_HEIGHT) break;
        }
        /* 检测 UTF-8 中文字符 (3字节) */
        if ((uint8_t)*str >= 0x80) {
            /* 是中文，跳过 3 字节，使用 draw_miao_char 显示 */
            if (str[0] == 0xE8 && str[1] == 0x8B && str[2] == 0x97) {
                /* 苗字 0xE8 0x8B 0x97 */
                draw_miao_char(cursor_x, y, fg, bg);
                cursor_x += 16;
                str += 3;
            } else {
                /* 跳过非苗字的中文字符（简化处理） */
                str += 3;
            }
        } else {
            /* ASCII 字符 */
            draw_char(cursor_x, y, *str, fg, bg);
            cursor_x += 8;
            str++;
        }
    }
}

/* 绘制眼睛 - 睁开状态 */
static void draw_eyes_open(int x, int y, uint16_t color)
{
    fill_rect(x + 6, y + 12, 5, 5, color);
    fill_rect(x + 21, y + 12, 5, 5, color);
}

/* 绘制眼睛 - 闭眼状态（横线） */
static void draw_eyes_closed(int x, int y, uint16_t color)
{
    fill_rect(x + 5, y + 14, 7, 2, color);
    fill_rect(x + 20, y + 14, 7, 2, color);
}

/* 绘制嘴巴 - 正常状态 */
static void draw_mouth_normal(int x, int y, uint16_t color)
{
    fill_rect(x + 10, y + 25, 4, 2, color);
    fill_rect(x + 18, y + 25, 4, 2, color);
}

/* 绘制嘴巴 - 开心状态（上扬弧度） */
static void draw_mouth_happy(int x, int y, uint16_t color)
{
    /* 左上扬 */
    fill_rect(x + 8, y + 23, 3, 2, color);
    fill_rect(x + 11, y + 25, 2, 2, color);
    /* 右上扬 */
    fill_rect(x + 17, y + 25, 2, 2, color);
    fill_rect(x + 19, y + 23, 3, 2, color);
}

/* 绘制简化猫脸 - 支持表情参数 */
static void draw_cat_face_with_expression(int x, int y, uint16_t fg, uint16_t bg, miao_expression_t expr)
{
    /* 圆脸 - 黄色填充 */
    fill_rect(x, y, 32, 32, bg);
    /* 头顶三角耳朵 - 左耳 */
    fill_rect(x, y, 6, 10, bg);
    /* 头顶三角耳朵 - 右耳 */
    fill_rect(x + 26, y, 6, 10, bg);
    /* 脸的外框 - 黑色描边 */
    for (int i = 0; i < 32; i++) {
        esp_lcd_panel_draw_bitmap(panel_handle, x + i, y, x + i + 1, y + 1, &fg);
        esp_lcd_panel_draw_bitmap(panel_handle, x + i, y + 31, x + i + 1, y + 32, &fg);
    }
    for (int i = 1; i < 31; i++) {
        esp_lcd_panel_draw_bitmap(panel_handle, x, y + i, x + 1, y + i + 1, &fg);
        esp_lcd_panel_draw_bitmap(panel_handle, x + 31, y + i, x + 32, y + i + 1, &fg);
    }

    /* 根据表情绘制眼睛 */
    if (expr == EXPRESSION_BLINKING) {
        draw_eyes_closed(x, y, fg);
    } else {
        draw_eyes_open(x, y, fg);
    }

    /* 根据表情绘制嘴巴 */
    if (expr == EXPRESSION_WARNING) {
        /* 警告嘴巴 - 三角形状 */
        fill_rect(x + 12, y + 24, 8, 1, fg);
        fill_rect(x + 13, y + 25, 6, 1, fg);
        fill_rect(x + 14, y + 26, 4, 1, fg);
        fill_rect(x + 15, y + 27, 2, 1, fg);
    } else if (expr == EXPRESSION_TALKING || expr == EXPRESSION_THINKING) {
        draw_mouth_happy(x, y, fg);
    } else {
        draw_mouth_normal(x, y, fg);
    }

    /* 胡须 - 左 */
    fill_rect(x - 8, y + 18, 10, 1, fg);
    fill_rect(x - 8, y + 22, 10, 1, fg);
    /* 胡须 - 右 */
    fill_rect(x + 30, y + 18, 10, 1, fg);
    fill_rect(x + 30, y + 22, 10, 1, fg);
}

/* 绘制感叹号（警告表情） */
static void draw_exclamation(int x, int y, uint16_t color)
{
    /* 7x13 像素的感叹号 */
    fill_rect(x + 2, y, 3, 2, color);      /* 圆点部分 */
    fill_rect(x + 2, y + 3, 3, 7, color); /* 竖线部分 */
}

/* 重绘苗苗头像区域（精灵重绘） */
static void redraw_cat_expression(void)
{
    if (!display_initialized) return;

    if (sprite_drawn) {
        /* 精灵模式：重绘精灵 */
        redraw_sprite();
    } else if (cat_face_drawn) {
        /* 旧版程序绘制猫脸模式 */
        uint16_t face_color = COLOR_YELLOW;
        uint16_t feature_color = COLOR_BLACK;

        /* 重绘黄色脸底（覆盖旧眼睛） */
        fill_rect(cat_face_x, cat_face_y, 32, 32, face_color);

        /* 根据表情重绘 */
        if (current_expression == EXPRESSION_WARNING && target_expression == EXPRESSION_WARNING) {
            /* 显示警告表情 */
            draw_cat_face_with_expression(cat_face_x, cat_face_y, feature_color, face_color, EXPRESSION_WARNING);
            /* 画感叹号 */
            draw_exclamation(cat_face_x + 12, cat_face_y - 15, COLOR_RED);
        } else {
            draw_cat_face_with_expression(cat_face_x, cat_face_y, feature_color, face_color, current_expression);
        }
    }
}

/* 简化猫脸兼容函数（调用带表情的版本） */
static void draw_cat_face(int x, int y, uint16_t fg, uint16_t bg)
{
    draw_cat_face_with_expression(x, y, fg, bg, current_expression);
}

/**
 * 初始化 ST7789 LCD 显示屏 (SPI)
 */
esp_err_t display_init(void)
{
    ESP_LOGI(TAG, "初始化 ST7789 TFT SPI...");

    /* 初始化 SPI 总线 */
    spi_bus_config_t bus_cfg = {
        .sclk_io_num = OLED_SCL_PIN,
        .mosi_io_num = OLED_SDA_PIN,
        .miso_io_num = -1,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = ST7789_WIDTH * ST7789_HEIGHT * sizeof(uint16_t),
    };
    esp_err_t ret = spi_bus_initialize(OLED_SPI_HOST, &bus_cfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPI 总线初始化失败: %d", ret);
        return ret;
    }

    /* 初始化 panel IO */
    esp_lcd_panel_io_spi_config_t io_cfg = {
        .cs_gpio_num = OLED_CS_PIN,
        .dc_gpio_num = OLED_DC_PIN,
        .spi_mode = 3,
        .pclk_hz = 40 * 1000 * 1000,  /* 40MHz，elecfans 建议 ≤ 40MHz */
        .trans_queue_depth = 10,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
    };
    esp_lcd_panel_io_handle_t io_handle;
    ret = esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)OLED_SPI_HOST, &io_cfg, &io_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Panel IO 初始化失败: %d", ret);
        return ret;
    }

    /* 初始化 ST7789 面板 - 修复颜色字节序 */
    esp_lcd_panel_dev_config_t panel_cfg = {
        .reset_gpio_num = OLED_RST_PIN,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,  // MADCTL = 0x00
        .data_endian = LCD_RGB_DATA_ENDIAN_LITTLE,   // RAMCTRL = 0xF8 (反转字节序)
        .bits_per_pixel = 16,
    };
    ret = esp_lcd_new_panel_st7789(io_handle, &panel_cfg, &panel_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "ST7789 面板初始化失败: %d", ret);
        return ret;
    }

    /* 复位面板 */
    ret = esp_lcd_panel_reset(panel_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "面板复位失败: %d", ret);
        return ret;
    }

    /* 初始化面板驱动 */
    ret = esp_lcd_panel_init(panel_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "面板初始化失败: %d", ret);
        return ret;
    }

    /* 颜色反转 - 测试 RGB + invert=true */
    esp_lcd_panel_invert_color(panel_handle, true);

    /* 设置显示方向 */
    esp_lcd_panel_swap_xy(panel_handle, false);  // 不交换 XY
    esp_lcd_panel_mirror(panel_handle, false, false);

    /* 不再画白屏，由诊断函数 draw_color_test() 接管 */

    /* 等待一下让显示稳定 */
    vTaskDelay(pdMS_TO_TICKS(100));

    /* 开背光 */
    if (OLED_BL_PIN >= 0) {
        gpio_set_direction(OLED_BL_PIN, GPIO_MODE_OUTPUT);
        gpio_set_level(OLED_BL_PIN, 1);
        backlight_on = true;
    }

    display_initialized = true;

    // 调用诊断：显示白屏
    draw_color_test();

    // 初始化动画系统
    display_init_animation();

    ESP_LOGI(TAG, "ST7789 TFT SPI 初始化完成");
    return ESP_OK;
}

/**
 * 清空屏幕
 */
void display_clear(void)
{
    if (!display_initialized) return;
    fill_rect(0, 0, ST7789_WIDTH, ST7789_HEIGHT, COLOR_BLACK);
}

/**
 * 显示苗苗 logo
 */
void display_show_logo(void)
{
    if (!display_initialized) return;
    ensure_backlight();
    /* 画白屏作为启动画面 */
    fill_rect(0, 0, ST7789_WIDTH, ST7789_HEIGHT, COLOR_WHITE);
}

/**
 * 获取当前时间（毫秒）
 */
static int64_t get_time_ms(void)
{
    return xTaskGetTickCount() * portTICK_PERIOD_MS;
}

/**
 * 生成随机数在 min-max 之间
 */
static int random_in_range(int min_val, int max_val)
{
    return min_val + (esp_random() % (max_val - min_val + 1));
}

/**
 * 动画任务：处理眨眼、表情切换、空闲姿态
 */
static void animation_task(void *arg)
{
    ESP_LOGI(TAG, "动画任务启动");

    /* 初始化眨眼计时 */
    next_blink_time_ms = get_time_ms() + random_in_range(BLINK_INTERVAL_MIN_MS, BLINK_INTERVAL_MAX_MS);
    last_interaction_time = get_time_ms();
    int64_t idle_check_interval_ms = 30000;  /* 30秒无互动后切换空闲姿态 */
    showing_idle = false;

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(100));  /* 100ms 检查一次 */

        int64_t now = get_time_ms();

        /* 每500ms拉高背光GPIO防止黑屏（不涉及SPI总线） */
        static int64_t last_keepalive = 0;
        if (now - last_keepalive > 500) {
            if (OLED_BL_PIN >= 0) {
                gpio_set_level(OLED_BL_PIN, 1);
            }
            last_keepalive = now;
        }

        /* 高优先级：警告/警惕表情直接显示，不眨眼 */
        if (target_expression == EXPRESSION_WARNING || target_expression == EXPRESSION_ALERT) {
            if (current_expression != target_expression) {
                current_expression = target_expression;
                showing_idle = false;
                redraw_sprite();
                last_interaction_time = now;
            }
            continue;
        }

        /* 检查空闲超时（30秒无互动） */
        if (!showing_idle && target_expression == EXPRESSION_NORMAL &&
            (now - last_interaction_time) > idle_check_interval_ms) {
            /* 切换到随机空闲姿态 */
            showing_idle = true;
            current_idle_sprite = get_idle_sprite();
            fill_rect(sprite_x, sprite_y, 150, 150, COLOR_WHITE);
            draw_sprite(current_idle_sprite, sprite_x, sprite_y);
            ESP_LOGI(TAG, "切换到空闲姿态: %d", current_idle_sprite);
        }

        /* 检查是否到了眨眼时间（只有在非空闲、非常告警状态时） */
        if (!is_blinking && now >= next_blink_time_ms && !showing_idle) {
            /* 开始眨眼 */
            is_blinking = true;
            current_expression = EXPRESSION_BLINKING;
            redraw_sprite();
            ESP_LOGD(TAG, "眨眼开始");

            /* 150ms 后恢复 */
            vTaskDelay(pdMS_TO_TICKS(BLINK_DURATION_MS));

            /* 恢复常态 */
            is_blinking = false;
            if (target_expression == EXPRESSION_NORMAL || target_expression == EXPRESSION_BLINKING) {
                showing_idle = false;
                current_expression = EXPRESSION_NORMAL;
                redraw_sprite();
            }

            /* 计算下次眨眼时间 */
            next_blink_time_ms = now + random_in_range(BLINK_INTERVAL_MIN_MS, BLINK_INTERVAL_MAX_MS);
            ESP_LOGD(TAG, "眨眼结束，下次眨眼在 %lld ms 后", next_blink_time_ms - now);
            last_interaction_time = now;
        }
    }
}

/**
 * 初始化动画系统
 */
static void display_init_animation(void)
{
    if (animation_task_handle != NULL) {
        return;  /* 已初始化 */
    }

    BaseType_t ret = xTaskCreatePinnedToCore(
        animation_task,
        "display_anim",
        4096,
        NULL,
        2,  /* 低优先级，不阻塞主流程 */
        &animation_task_handle,
        0
    );

    if (ret != pdPASS) {
        ESP_LOGE(TAG, "创建动画任务失败");
    } else {
        ESP_LOGI(TAG, "动画任务创建成功");
    }
}

/**
 * 显示状态信息
 */
void display_show_status(const char *line1, const char *line2)
{
    if (!display_initialized) return;

    /* 内容变化检测 */
    uint32_t content_hash = hash_string(line1) ^ (hash_string(line2 ? line2 : "") << 1);
    if (content_hash == last_content_hash && content_drawn) {
        return;  /* 内容没变，跳过重绘 */
    }
    if (!should_redraw()) {
        return;  /* 时间太短，跳过重绘 */
    }

    /* 直接更新显示，不使用锁 - 防止阻塞音频采集 */
    last_content_hash = content_hash;

    strncpy(last_status_line1, line1, sizeof(last_status_line1) - 1);
    strncpy(last_status_line2, line2 ? line2 : "", sizeof(last_status_line2) - 1);
    last_status_line1[sizeof(last_status_line1) - 1] = 0;
    last_status_line2[sizeof(last_status_line2) - 1] = 0;

    /* 重绘屏幕 - 使用精灵图 */
    fill_rect(0, 0, ST7789_WIDTH, ST7789_HEIGHT, COLOR_WHITE);  // 白色背景
    miao_sprite_t sprite = expression_to_sprite(current_expression);
    draw_sprite(sprite, sprite_x, sprite_y);
    sprite_drawn = true;

    /* 文本移到下方 */
    draw_string(20, 155, line1, COLOR_BLUE, COLOR_WHITE);
    if (line2 && line2[0]) {
        draw_string(20, 180, line2, COLOR_BLUE, COLOR_WHITE);
    }

    /* 记录绘制时间 */
    last_draw_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
    content_drawn = true;
    cat_face_drawn = false;  /* 不再使用程序绘制的猫脸 */
}

/**
 * 显示对话消息
 */
void display_show_message(const char *speaker, const char *message)
{
    if (!display_initialized) return;

    /* 不再使用锁，避免死锁问题 */

    /* 清除状态缓存，下次 display_show_status 会重绘 */
    last_status_line1[0] = '\0';
    last_status_line2[0] = '\0';

    fill_rect(0, 0, ST7789_WIDTH, ST7789_HEIGHT, COLOR_WHITE);  // 白色背景

    /* 精灵居中显示 */
    miao_sprite_t sprite = expression_to_sprite(current_expression);
    draw_sprite(sprite, sprite_x, sprite_y);
    sprite_drawn = true;

    /* 文本显示在猫下方 */
    int x = 20;
    int y = sprite_y + 35;
    int max_width = ST7789_WIDTH - 20;

    /* 说话者名字 */
    draw_string(x, sprite_y - 15, speaker, COLOR_RED, COLOR_WHITE);

    while (*message && y < ST7789_HEIGHT - 20) {
        int chars_to_draw = max_width / 8;
        if (chars_to_draw > 20) chars_to_draw = 20;

        char line[21] = {0};
        int len = 0;
        while (len < chars_to_draw && message[len]) len++;
        strncpy(line, message, len);

        draw_string(x, y, line, COLOR_BLACK, COLOR_WHITE);
        y += 18;
        message += len;
    }
}

/**
 * 设置苗苗表情
 */
void display_set_expression(int expression)
{
    if (!display_initialized) return;

    target_expression = expression;
    showing_idle = false;  /* 任何交互都退出空闲状态 */
    last_interaction_time = get_time_ms();

    if (!is_blinking) {
        current_expression = expression;
        if (sprite_drawn) {
            redraw_sprite();
        } else if (cat_face_drawn) {
            redraw_cat_expression();
        }
    }
}

/**
 * 设置警告表情
 */
void display_show_warning(void)
{
    display_set_expression(EXPRESSION_WARNING);
}

/**
 * 清除警告表情
 */
void display_clear_warning(void)
{
    display_set_expression(EXPRESSION_NORMAL);
}