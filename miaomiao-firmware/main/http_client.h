/**
 * HTTP 客户端头文件
 */

#ifndef HTTP_CLIENT_H
#define HTTP_CLIENT_H

#include <stdint.h>
#include <stdbool.h>

/**
 * 发送音频到后端，获取 AI 回复
 *
 * @param audio_data 音频数据（PCM 16bit mono）
 * @param audio_len 音频数据长度（字节）
 * @param text_response 输出：AI 回复文字（需预分配 512 字节）
 * @param audio_response 输出：TTS 音频数据（由调用者释放）
 * @param audio_len_out 输出：TTS 音频长度
 * @return 0 成功，-1 失败
 */
int http_chat(int16_t *audio_data, size_t audio_len,
              char *text_response, uint8_t **audio_response, size_t *audio_len_out);

/**
 * 发送主动消息（超时提示）
 * @param message 要说的话
 * @param audio_response 输出：TTS 音频数据（由调用者释放）
 * @param audio_len_out 输出：TTS 音频长度
 * @return 0 成功，-1 失败
 */
int http_proactive_message(const char *message, uint8_t **audio_response, size_t *audio_len_out);

/**
 * 健康检查
 * @return true 后端正常，false 后端异常
 */
bool http_health_check(void);

#endif // HTTP_CLIENT_H