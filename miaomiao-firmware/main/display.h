/**
 * ST7789 TFT 显示屏头文件
 */

#ifndef DISPLAY_H
#define DISPLAY_H

#include <esp_err.h>

/**
 * 初始化 ST7789 TFT 显示屏 (SPI)
 */
esp_err_t display_init(void);

/**
 * 清空屏幕
 */
void display_clear(void);

/**
 * 显示苗苗 logo
 */
void display_show_logo(void);

/**
 * 显示状态信息
 * @param line1 第一行文本（最多 16 字符）
 * @param line2 第二行文本（可选，为 NULL 或空则不显示）
 */
void display_show_status(const char *line1, const char *line2);

/**
 * 显示对话消息
 * @param speaker 说话者（"老人" 或 "苗苗"）
 * @param message 消息内容
 */
void display_show_message(const char *speaker, const char *message);

/**
 * 设置苗苗表情
 * @param expression 0=正常, 1=眨眼, 2=说话中, 3=思考, 4=警告
 */
void display_set_expression(int expression);

/**
 * 设置警告表情（高风险诈骗检测时调用）
 * 显示红色感叹号 + 警告嘴巴
 */
void display_show_warning(void);

/**
 * 清除警告表情，恢复正常
 */
void display_clear_warning(void);

/**
 * 设置 WiFi 连接状态图标
 * @param connected true=已连接(绿色), false=未连接(红色)
 */
void display_set_wifi(bool connected);

/**
 * 设置音频活动状态（音频传输期间暂停动画更新）
 * @param active true=音频活动中, false=空闲
 */
void display_set_audio_active(bool active);

#endif // DISPLAY_H