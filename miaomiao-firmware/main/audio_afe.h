/**
 * AFE 音频前端 - 基于 esp-sr 组件的统一音频管理
 *
 * 使用乐鑫 AFE (Audio Front-End) 方案统一管理：
 * - I2S 音频采集（DMA 模式）
 * - WakeWord 唤醒词检测
 * - VAD 语音活动检测
 *
 * 参考：xiaozhi-esp32/main/audio/processors/afe_audio_processor.cc
 */

#ifndef AUDIO_AFE_H
#define AUDIO_AFE_H

#include <stdint.h>
#include <stdbool.h>
#include <driver/i2s_std.h>
#include "esp_afe_sr_iface.h"
#include "esp_wn_iface.h"

// 音频缓冲区大小
#define AUDIO_AFE_FEED_SIZE 1280  // 每帧 1280 samples (80ms @ 16kHz)
#define AUDIO_AFE_FETCH_SIZE 1280 // fetch 返回大小

// AFE 事件回调
typedef void (*afe_wake_word_callback_t)(const char* wake_word, int wake_word_index);
typedef void (*afe_vad_callback_t)(bool speaking);
// AFE 输出音频回调（用于录音）
typedef void (*afe_output_callback_t)(const int16_t* data, int samples);

typedef struct {
    // 模型路径
    const char* model_path;
    // Wake word 模型名称
    const char* wake_word_model;
    // 是否启用 VAD
    bool vad_init;
    // VAD 模型名称（可为 NULL）
    const char* vad_model;
    // 是否启用降噪
    bool ns_init;
    // 降噪模型名称（可为 NULL）
    const char* ns_model;
    // AEC 模式（无回声消除时为 false）
    bool aec_init;
    // 内存分配模式
    int memory_alloc_mode;  // 1=internal, 2=balance, 3=psram
    // 采样率
    int sample_rate;
    // 回调
    afe_wake_word_callback_t on_wake_word;
    afe_vad_callback_t on_vad_change;
    // AFE 增强后音频输出回调（可用于录音）
    afe_output_callback_t on_output;
} audio_afe_config_t;

/**
 * AFE 音频前端句柄
 */
typedef struct audio_afe audio_afe_t;

/**
 * 创建 AFE 实例
 * @param config AFE 配置
 * @return AFE 句柄，NULL 表示失败
 */
audio_afe_t* audio_afe_create(audio_afe_config_t* config);

/**
 * 销毁 AFE 实例
 * @param afe AFE 句柄
 */
void audio_afe_destroy(audio_afe_t* afe);

/**
 * 启动 AFE
 * @param afe AFE 句柄
 * @return 0 成功，-1 失败
 */
int audio_afe_start(audio_afe_t* afe);

/**
 * 停止 AFE
 * @param afe AFE 句柄
 */
void audio_afe_stop(audio_afe_t* afe);

/**
 * 喂音频数据到 AFE
 * 调用方从 I2S 读取原始 PCM 数据后调用此函数
 * @param afe AFE 句柄
 * @param data 音频数据（16bit PCM）
 * @param samples 样本数量
 */
void audio_afe_feed(audio_afe_t* afe, const int16_t* data, int samples);

/**
 * 获取 AFE 配置的每帧样本数
 * @param afe AFE 句柄
 * @return 每帧样本数
 */
int audio_afe_get_feed_samples(audio_afe_t* afe);

/**
 * 设置 AFE 输出音频回调
 * @param afe AFE 句柄
 * @param callback 输出回调函数
 */
void audio_afe_set_output_callback(audio_afe_t* afe, afe_output_callback_t callback);

/**
 * 获取最近一帧的音频能量值（avg sample，绝对值）
 * 可用于能量检测替代 VAD
 * @return avg sample 值，越大表示声音越大
 */
int audio_afe_get_audio_level(void);

/**
 * 初始化 Standalone WakeNet（独立于 AFE）
 * 必须在 audio_afe_start() 之后调用
 * @param afe AFE 句柄
 * @param model_name WakeNet 模型名称（如 "wn9_himiaomiao_tts"）
 * @param threshold 检测阈值（0.4 是最低有效值）
 * @return 0 成功，-1 失败
 */
int audio_afe_init_wakenet(audio_afe_t* afe, const char* model_name, float threshold);

/**
 * 获取 AFE 的 I2S TX 句柄（用于音频播放）
 * TX 与 RX 共享 I2S 总线，由 init_i2s 同时创建
 * @param afe AFE 句柄
 * @return I2S TX 句柄，NULL 表示未初始化
 */
i2s_chan_handle_t audio_afe_get_tx_handle(audio_afe_t* afe);

/**
 * 暂停 AFE 的 RX 通道（播放时使用，避免 RX/TX 总线冲突）
 */
void audio_afe_pause_rx(audio_afe_t* afe);

/**
 * 恢复 AFE 的 RX 通道
 */
void audio_afe_resume_rx(audio_afe_t* afe);

#endif // AUDIO_AFE_H