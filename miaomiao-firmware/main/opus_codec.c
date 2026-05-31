/**
 * Opus 编解码封装 - 使用 esp_audio_codec 组件
 */

#include <string.h>
#include <stdlib.h>
#include <esp_log.h>
#include <esp_heap_caps.h>
#include "opus_codec.h"
#include "esp_opus_enc.h"
#include "esp_opus_dec.h"
#include "esp_audio_enc.h"
#include "esp_audio_dec.h"
#include "esp_audio_dec_reg.h"

static const char *TAG = "OPUS_CODEC";

struct opus_codec_s {
    void *handle;
    int sample_rate;
    int channels;
    int frame_duration_ms;
    bool is_encoder;
};

opus_codec_handle_t opus_enc_create(opus_config_t *config)
{
    if (config == NULL) {
        ESP_LOGE(TAG, "opus_enc_create: config is NULL");
        return NULL;
    }

    esp_opus_enc_config_t enc_cfg = {
        .sample_rate = config->sample_rate,
        .channel = config->channels,
        .bits_per_sample = 16,
        .bitrate = config->bitrate > 0 ? config->bitrate : 64000,
        .frame_duration = ESP_OPUS_ENC_FRAME_DURATION_60_MS,
        .application_mode = ESP_OPUS_ENC_APPLICATION_VOIP,
        .complexity = 0,
        .enable_fec = false,
        .enable_dtx = true,
        .enable_vbr = true,
    };

    void *enc_hd = NULL;
    esp_audio_err_t ret = esp_opus_enc_open(&enc_cfg, sizeof(enc_cfg), &enc_hd);
    if (ret != ESP_AUDIO_ERR_OK || enc_hd == NULL) {
        ESP_LOGE(TAG, "esp_opus_enc_open failed: %d", ret);
        return NULL;
    }

    struct opus_codec_s *codec = (struct opus_codec_s *)calloc(1, sizeof(struct opus_codec_s));
    if (codec == NULL) {
        esp_opus_enc_close(enc_hd);
        return NULL;
    }

    codec->handle = enc_hd;
    codec->sample_rate = config->sample_rate;
    codec->channels = config->channels;
    codec->frame_duration_ms = config->frame_duration_ms;
    codec->is_encoder = true;

    ESP_LOGI(TAG, "Opus encoder created: %dHz, %dch, %dms",
             config->sample_rate, config->channels, config->frame_duration_ms);
    return codec;
}

int opus_enc_process(opus_codec_handle_t handle, const int16_t *pcm_in, int pcm_samples,
                     uint8_t *opus_out, int opus_max_len)
{
    if (handle == NULL || !handle->is_encoder) {
        return -1;
    }

    esp_audio_enc_in_frame_t in_frame = {
        .buffer = (const uint8_t *)pcm_in,
        .len = pcm_samples * sizeof(int16_t),
    };
    esp_audio_enc_out_frame_t out_frame = {
        .buffer = opus_out,
        .len = opus_max_len,
    };

    esp_audio_err_t ret = esp_opus_enc_process(handle->handle, &in_frame, &out_frame);
    if (ret != ESP_AUDIO_ERR_OK) {
        if (ret != ESP_AUDIO_ERR_DATA_LACK) {
            ESP_LOGW(TAG, "esp_opus_enc_process err: %d", ret);
        }
        return -1;
    }

    return out_frame.encoded_bytes;
}

opus_codec_handle_t opus_dec_create(opus_config_t *config)
{
    if (config == NULL) {
        ESP_LOGE(TAG, "opus_dec_create: config is NULL");
        return NULL;
    }

    esp_opus_dec_cfg_t dec_cfg = {
        .sample_rate = config->sample_rate,
        .channel = config->channels,
        .frame_duration = ESP_OPUS_DEC_FRAME_DURATION_60_MS,
        .self_delimited = false,
    };

    void *dec_hd = NULL;
    esp_audio_err_t ret = esp_opus_dec_open(&dec_cfg, sizeof(dec_cfg), &dec_hd);
    if (ret != ESP_AUDIO_ERR_OK || dec_hd == NULL) {
        ESP_LOGE(TAG, "esp_opus_dec_open failed: %d", ret);
        return NULL;
    }

    struct opus_codec_s *codec = (struct opus_codec_s *)calloc(1, sizeof(struct opus_codec_s));
    if (codec == NULL) {
        esp_opus_dec_close(dec_hd);
        return NULL;
    }

    codec->handle = dec_hd;
    codec->sample_rate = config->sample_rate;
    codec->channels = config->channels;
    codec->frame_duration_ms = config->frame_duration_ms;
    codec->is_encoder = false;

    ESP_LOGI(TAG, "Opus decoder created: %dHz, %dch", config->sample_rate, config->channels);
    return codec;
}

int opus_dec_process(opus_codec_handle_t handle, const uint8_t *opus_in, int opus_len,
                     int16_t *pcm_out, int pcm_max_samples)
{
    if (handle == NULL || handle->is_encoder) {
        return -1;
    }

    esp_audio_dec_in_raw_t in_raw = {
        .buffer = (uint8_t *)opus_in,
        .len = opus_len,
    };
    esp_audio_dec_out_frame_t out_frame = {
        .buffer = (uint8_t *)pcm_out,
        .len = pcm_max_samples * sizeof(int16_t),
    };
    esp_audio_dec_info_t dec_info = {0};

    esp_audio_err_t ret = esp_opus_dec_decode(handle->handle, &in_raw, &out_frame, &dec_info);
    if (ret != ESP_AUDIO_ERR_OK) {
        ESP_LOGW(TAG, "esp_opus_dec_decode err: %d", ret);
        return -1;
    }

    return out_frame.decoded_size / sizeof(int16_t);
}

void opus_codec_destroy(opus_codec_handle_t handle)
{
    if (handle == NULL) return;

    if (handle->is_encoder) {
        esp_opus_enc_close(handle->handle);
    } else {
        esp_opus_dec_close(handle->handle);
    }

    free(handle);
    ESP_LOGI(TAG, "Opus codec destroyed");
}

int opus_get_sample_rate(opus_codec_handle_t handle)
{
    if (handle == NULL) return 0;
    return handle->sample_rate;
}
