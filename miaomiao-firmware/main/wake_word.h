/**
 * 独立唤醒词检测模块（xiaozhi esp_wn_iface 方案）
 *
 * 使用 esp_wn_iface 直接检测，不需要 AFE fetch。
 * audio_afe.c feed_task 调用 wake_word_feed() 喂数据。
 */

#ifndef WAKE_WORD_H
#define WAKE_WORD_H

#include <stdint.h>
#include <esp_err.h>

/**
 * 初始化唤醒词检测
 */
esp_err_t wake_word_init(void);

/**
 * 设置唤醒回调
 */
void wake_word_set_callback(void (*callback)(const char *wake_word));

/**
 * 重置检测状态
 */
void wake_word_reset(void);

/**
 * 获取 WakeNet chunk size
 */
int wake_word_get_chunksize(void);

/**
 * 释放资源
 */
void wake_word_deinit(void);

/**
 * 喂音频数据给 WakeNet 检测（由 audio_afe.c 调用）
 */
void wake_word_feed(const int16_t *samples, int count);

#endif /* WAKE_WORD_H */
