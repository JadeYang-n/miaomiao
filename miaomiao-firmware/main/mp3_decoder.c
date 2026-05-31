/**
 * MP3 解码器封装 - 基于 HELIX MP3 解码器
 */

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "mp3_decoder.h"
#include "esp_log.h"
#include "mp3dec.h"

static const char *TAG = "MP3_DECODER";

/* 内部状态 */
struct mp3_decoder_s {
    HMP3Decoder hMP3Decoder;      // HELIX 解码器句柄
    int sample_rate;               // 采样率
    int channels;                  // 通道数
    bool initialized;              // 是否已初始化
};

/* 内部缓冲区大小（必须是整数帧大小） */
#define MP3_DECODER_BUF_SIZE (8192)

mp3_decoder_handle_t mp3_decoder_create(void)
{
    mp3_decoder_handle_t handle = (mp3_decoder_handle_t)calloc(1, sizeof(struct mp3_decoder_s));
    if (handle == NULL) {
        ESP_LOGE(TAG, "分配解码器失败");
        return NULL;
    }

    handle->hMP3Decoder = MP3InitDecoder();
    if (handle->hMP3Decoder == NULL) {
        ESP_LOGE(TAG, "创建 HELIX 解码器失败");
        free(handle);
        return NULL;
    }

    handle->sample_rate = 0;
    handle->channels = 0;
    handle->initialized = true;

    ESP_LOGI(TAG, "MP3 解码器创建成功");
    return handle;
}

int mp3_decoder_decode(mp3_decoder_handle_t handle,
                       const uint8_t* mp3_data,
                       size_t mp3_len,
                       int16_t* pcm_out,
                       size_t pcm_max_samples)
{
    if (!handle || !handle->initialized || !mp3_data || !pcm_out) {
        ESP_LOGE(TAG, "参数无效");
        return -1;
    }

    if (mp3_len == 0) {
        return 0;
    }

    /* 指向输入数据的可修改指针 */
    unsigned char* inbuf = (unsigned char*)mp3_data;
    int bytesLeft = (int)mp3_len;

    /* 跳过 ID3 标签（如果有） */
    int offset = MP3FindSyncWord(inbuf, bytesLeft);
    if (offset < 0) {
        ESP_LOGW(TAG, "找不到 MP3 同步字");
        return 0;
    }
    inbuf += offset;
    bytesLeft -= offset;

    /* 解码一帧 */
    int outputSamples = MP3Decode(handle->hMP3Decoder, &inbuf, &bytesLeft, pcm_out, (int)pcm_max_samples);

    if (outputSamples < 0) {
        ESP_LOGW(TAG, "解码失败: %d", outputSamples);
        return 0;
    }

    /* 获取帧信息 */
    MP3FrameInfo frameInfo;
    MP3GetLastFrameInfo(handle->hMP3Decoder, &frameInfo);
    handle->sample_rate = frameInfo.samprate;
    handle->channels = frameInfo.nChans;

    return outputSamples;
}

int mp3_decoder_get_sample_rate(mp3_decoder_handle_t handle)
{
    if (!handle || !handle->initialized) {
        return 0;
    }
    return handle->sample_rate;
}

int mp3_decoder_get_channels(mp3_decoder_handle_t handle)
{
    if (!handle || !handle->initialized) {
        return 0;
    }
    return handle->channels;
}

void mp3_decoder_destroy(mp3_decoder_handle_t handle)
{
    if (handle) {
        if (handle->hMP3Decoder) {
            MP3FreeDecoder(handle->hMP3Decoder);
        }
        free(handle);
        ESP_LOGI(TAG, "MP3 解码器已销毁");
    }
}