/**
 * I2S 音频播放（TX）模块
 *
 * TX 句柄由 AFE 的 init_i2s 创建，通过 audio_i2s_set_tx_handle() 注入。
 * RX 由 AFE 管理，此模块只负责播放。
 *
 * TX 采样率 24kHz（匹配 MiMo TTS MP3），RX 采样率 16kHz（独立 Simplex）。
 */

#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <driver/i2s_std.h>
#include <esp_log.h>

#include "config.h"

static const char *TAG = "AUDIO_I2S";

static i2s_chan_handle_t tx_handle = NULL;
static SemaphoreHandle_t write_mutex = NULL;
static bool first_write_logged = false;

void audio_i2s_set_tx_handle(i2s_chan_handle_t tx)
{
    tx_handle = tx;
    ESP_LOGI(TAG, "TX handle 已设置: %p", tx);
}

bool audio_i2s_init(void)
{
    if (tx_handle != NULL) {
        if (write_mutex == NULL) {
            write_mutex = xSemaphoreCreateMutex();
        }
        ESP_LOGI(TAG, "I2S TX 已就绪（由 AFE 注入）");
        return true;
    }
    ESP_LOGW(TAG, "TX handle 未设置");
    return false;
}

int audio_i2s_read(int16_t *samples, int max_samples, int timeout_ms)
{
    (void)samples; (void)max_samples; (void)timeout_ms;
    return -1;
}

/**
 * 写入音频数据到功放播放
 * 输入：16-bit PCM，24kHz（与 MiMo TTS 匹配）
 * 输出：32-bit I2S，24kHz（与 TX 采样率一致，无需重采样）
 */
void audio_i2s_write(int16_t *samples, int samples_count, int timeout_ms)
{
    if (tx_handle == NULL || samples == NULL || samples_count <= 0) return;

    if (write_mutex) xSemaphoreTake(write_mutex, portMAX_DELAY);

    /* 16-bit PCM → 32-bit I2S（参考 xiaozhi NoAudioCodec::Write） */
    int32_t *buf32 = (int32_t *)malloc(samples_count * sizeof(int32_t));
    if (buf32 == NULL) {
        if (write_mutex) xSemaphoreGive(write_mutex);
        return;
    }

    /* volume=65 → factor = pow(0.65, 2) * 65536 ≈ 27690，平衡音量和音质 */
    int32_t volume_factor = 28000;
    for (int i = 0; i < samples_count; i++) {
        int64_t temp = (int64_t)samples[i] * volume_factor;
        if (temp > INT32_MAX) temp = INT32_MAX;
        else if (temp < INT32_MIN) temp = INT32_MIN;
        buf32[i] = (int32_t)temp;
    }

    size_t bytes_written = 0;
    esp_err_t ret = i2s_channel_write(tx_handle, buf32, samples_count * sizeof(int32_t),
                      &bytes_written, portMAX_DELAY);

    if (!first_write_logged) {
        first_write_logged = true;
        ESP_LOGI(TAG, "[I2S DIAG] ret=%s, samples=%d, written=%d, buf32[0..3]: %ld %ld %ld %ld",
                 esp_err_to_name(ret), samples_count, (int)bytes_written,
                 (long)buf32[0], (long)buf32[1], (long)buf32[2], (long)buf32[3]);
    }

    free(buf32);
    if (write_mutex) xSemaphoreGive(write_mutex);
}

void audio_i2s_start_rx(void) { }
void audio_i2s_stop_rx(void) { }

void audio_i2s_start_tx(void)
{
    if (tx_handle) {
        ESP_LOGI(TAG, "TX 已就绪 handle=%p（常开模式）", tx_handle);
    }
}

void audio_i2s_enable_tx(void)
{
    if (tx_handle) {
        i2s_channel_enable(tx_handle);
    }
}

void audio_i2s_stop_tx(void) { }
