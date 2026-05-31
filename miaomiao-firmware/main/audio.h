/**
 * 音频采集与播放头文件
 */

#ifndef AUDIO_H
#define AUDIO_H

#include <stdint.h>
#include <stdbool.h>
#include <esp_err.h>

/**
 * 初始化音频硬件（I2S 麦克风和功放）
 */
esp_err_t audio_init(void);

/**
 * 采集音频（阻塞直到采集完成或超时）
 * @param buffer 输出参数，指向接收音频数据的 buffer（调用者负责释放）
 * @param max_samples 最大采样点数
 * @return 实际采样点数，<= 0 表示超时或错误
 */
int audio_capture(int16_t **buffer, int max_samples);

/**
 * 使用 AFE 采集音频（支持唤醒词检测）
 * @param buffer 输出参数，指向接收音频数据的 buffer（调用者负责释放）
 * @param max_samples 最大采样点数
 * @return 实际采样点数，<= 0 表示超时或错误
 */
int audio_capture_with_wakeword(int16_t **buffer, int max_samples);

/**
 * 检查是否检测到唤醒词
 */
bool audio_is_wake_word_detected(void);

/**
 * 重置唤醒词检测状态
 */
void audio_reset_wake_word_detection(void);

/**
 * 播放音频
 * @param data 音频数据（PCM 16bit mono）
 * @param len 数据长度（字节）
 */
void audio_play(uint8_t *data, size_t len);

/**
 * 播放提示音（简单的 beep）
 */
void audio_play_beep(void);

#endif // AUDIO_H