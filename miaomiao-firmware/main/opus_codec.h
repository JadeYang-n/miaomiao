/**
 * Opus 编解码封装 - 使用 esp_audio_codec 组件
 */

#ifndef _OPUS_CODEC_H_
#define _OPUS_CODEC_H_

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Opus 句柄 */
typedef struct opus_codec_s *opus_codec_handle_t;

/* Opus 配置 */
typedef struct {
    int sample_rate;        // 采样率 (默认 16000)
    int channels;          // 通道数 (默认 1)
    int bitrate;           // 码率 (默认 64000)
    int frame_duration_ms; // 帧时长 (默认 60)
} opus_config_t;

/**
 * 创建 Opus 编码器
 */
opus_codec_handle_t opus_enc_create(opus_config_t *config);

/**
 * 编码 PCM → Opus
 */
int opus_enc_process(opus_codec_handle_t handle, const int16_t *pcm_in, int pcm_samples,
                     uint8_t *opus_out, int opus_max_len);

/**
 * 创建 Opus 解码器
 */
opus_codec_handle_t opus_dec_create(opus_config_t *config);

/**
 * 解码 Opus → PCM
 */
int opus_dec_process(opus_codec_handle_t handle, const uint8_t *opus_in, int opus_len,
                     int16_t *pcm_out, int pcm_max_samples);

/**
 * 销毁编解码器
 */
void opus_codec_destroy(opus_codec_handle_t handle);

/**
 * 获取采样率
 */
int opus_get_sample_rate(opus_codec_handle_t handle);

#ifdef __cplusplus
}
#endif

#endif /* _OPUS_CODEC_H_ */
