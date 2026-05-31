/**
 * MP3 解码器封装 - 基于 HELIX MP3 解码器
 *
 * 提供简单的 API：输入 MP3 数据 → 输出 PCM 数据
 */

#ifndef MP3_DECODER_H
#define MP3_DECODER_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * MP3 解码器句柄
 */
typedef struct mp3_decoder_s* mp3_decoder_handle_t;

/**
 * 创建 MP3 解码器
 * @return 解码器句柄，NULL 表示失败
 */
mp3_decoder_handle_t mp3_decoder_create(void);

/**
 * 解码 MP3 数据（内存到内存）
 * @param handle 解码器句柄
 * @param mp3_data 输入 MP3 数据
 * @param mp3_len 输入数据长度
 * @param pcm_out 输出 PCM 缓冲区
 * @param pcm_max_samples 输出缓冲区最大采样点数
 * @return 实际解码的采样点数，<= 0 表示错误或需要更多数据
 */
int mp3_decoder_decode(mp3_decoder_handle_t handle,
                       const uint8_t* mp3_data,
                       size_t mp3_len,
                       int16_t* pcm_out,
                       size_t pcm_max_samples);

/**
 * 获取解码器采样率
 * @param handle 解码器句柄
 * @return 采样率（Hz），0 表示未知
 */
int mp3_decoder_get_sample_rate(mp3_decoder_handle_t handle);

/**
 * 获取解码器通道数
 * @param handle 解码器句柄
 * @return 通道数（1=单声道，2=立体声），0 表示未知
 */
int mp3_decoder_get_channels(mp3_decoder_handle_t handle);

/**
 * 销毁解码器
 * @param handle 解码器句柄
 */
void mp3_decoder_destroy(mp3_decoder_handle_t handle);

#ifdef __cplusplus
}
#endif

#endif // MP3_DECODER_H