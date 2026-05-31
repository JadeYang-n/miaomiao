/**
 * I2S 音频播放（TX）模块
 *
 * TX 句柄由 AFE 的 init_i2s 同时创建（duplex 模式），
 * 通过 audio_i2s_set_tx_handle() 注入。
 */

#ifndef AUDIO_I2S_H
#define AUDIO_I2S_H

#include <stdint.h>
#include <stdbool.h>
#include <driver/i2s_std.h>

/**
 * 设置 I2S TX 句柄（由 AFE 创建并注入）
 * 必须在 audio_i2s_write 之前调用
 */
void audio_i2s_set_tx_handle(i2s_chan_handle_t tx);

/**
 * 初始化 I2S 音频（旧接口，已弃用）
 */
bool audio_i2s_init(void);

/**
 * 读取麦克风音频数据
 * @param samples 输出 buffer
 * @param max_samples 最大样本数
 * @param timeout_ms 超时时间（毫秒）
 * @return 实际读取的样本数
 */
int audio_i2s_read(int16_t *samples, int max_samples, int timeout_ms);

/**
 * 写入音频数据到功放播放
 * @param samples 音频数据
 * @param samples_count 样本数
 * @param timeout_ms 超时时间（毫秒）
 */
void audio_i2s_write(int16_t *samples, int samples_count, int timeout_ms);

/**
 * 启用录音
 */
void audio_i2s_start_rx(void);

/**
 * 停止录音
 */
void audio_i2s_stop_rx(void);

/**
 * 启用播放
 */
void audio_i2s_start_tx(void);

/**
 * 停止播放
 */
void audio_i2s_stop_tx(void);

/**
 * 确保 TX channel 处于启用状态（播放前调用）
 */
void audio_i2s_enable_tx(void);

#endif // AUDIO_I2S_H
