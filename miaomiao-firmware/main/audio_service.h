/**
 * 音频服务头文件
 */

#ifndef AUDIO_SERVICE_H
#define AUDIO_SERVICE_H

#include <stdint.h>
#include <stdbool.h>
#include <esp_err.h>

/**
 * 设置音频采集完成回调
 * @param cb 回调函数（samples 为音频数据，count 为样本数）
 */
void audio_service_set_callback(void (*cb)(int16_t *samples, int count));

/**
 * 初始化音频服务（I2S + WakeWord + VAD）
 */
esp_err_t audio_service_init(void);

/**
 * 启动音频服务
 */
esp_err_t audio_service_start(void);

/**
 * 停止音频服务
 */
void audio_service_stop(void);

/**
 * 释放音频服务
 */
void audio_service_deinit(void);

#endif // AUDIO_SERVICE_H