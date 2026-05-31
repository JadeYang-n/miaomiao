/**
 * 独立 VAD（语音活动检测）头文件
 */

#ifndef VAD_H
#define VAD_H

#include <stdint.h>
#include <stdbool.h>
#include <esp_err.h>

/**
 * 初始化独立 VAD
 */
esp_err_t vad_init(void);

/**
 * 启动 VAD 检测
 */
esp_err_t vad_start(void);

/**
 * 停止 VAD 检测
 */
void vad_stop(void);

/**
 * 释放 VAD 资源
 */
void vad_deinit(void);

/**
 * 检查是否检测到语音
 */
bool vad_is_speech_detected(void);

/**
 * 重置 VAD 状态
 */
void vad_reset(void);

/**
 * 发送音频数据到 VAD
 * @param samples 音频样本
 * @param count 样本数量
 */
void vad_feed(int16_t *samples, int count);

#endif // VAD_H