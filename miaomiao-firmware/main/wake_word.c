/**
 * 独立唤醒词检测模块（xiaozhi esp_wn_iface 方案）
 *
 * 使用 esp_wn_iface 直接调用 WakeNet detect() 函数，
 * 不依赖 AFE fetch 的 wakeup_state。
 * audio_afe.c 的 feed_task 通过 wake_word_feed() 喂数据。
 */

#include <string.h>
#include <stdlib.h>
#include <esp_log.h>
#include <esp_err.h>
#include <esp_timer.h>

#include "esp_wn_iface.h"
#include "esp_wn_models.h"
#include "model_path.h"
#include "config.h"

static const char *TAG = "WAKE_WORD";

/* esp_wn_iface 接口和句柄 */
static const esp_wn_iface_t *wn_iface = NULL;
static model_iface_data_t *wn_data = NULL;

/* 唤醒回调 */
static void (*wake_word_callback)(const char *wake_word) = NULL;

/* cooldown */
static int64_t last_trigger_time_ms = 0;
#define TRIGGER_COOLDOWN_MS 2000

/* 外部调用的 feed 函数 - 由 audio_afe.c feed_task 调用 */
void wake_word_feed(const int16_t *samples, int count)
{
    if (wn_iface == NULL || wn_data == NULL) return;

    static int64_t last_log_ms = 0;
    int64_t now_ms = esp_timer_get_time() / 1000;

    // 直接调用 detect()，返回检测到的词索引（>0=检测到），0=未检测
    // 注意：返回类型是 wakenet_state_t，但 esp_wn_iface 实现返回的是词索引
    int detect_result = (int)wn_iface->detect(wn_data, (int16_t *)samples);

    if (detect_result > 0) {
        ESP_LOGI(TAG, "##### WakeNet DETECTED! word_idx=%d #####", detect_result);

        if (now_ms - last_trigger_time_ms >= TRIGGER_COOLDOWN_MS) {
            last_trigger_time_ms = now_ms;
            if (wake_word_callback != NULL) {
                const char *word = "miaomiao";
                ESP_LOGI(TAG, ">>> WakeWord callback: %s", word);
                wake_word_callback(word);
            }
        }
    }

    // 每 10 秒打印一次检测状态
    if (now_ms - last_log_ms > 10000) {
        ESP_LOGI(TAG, "[WakeNet] detect=%d, chunksize=%d", detect_result,
                 wn_iface->get_samp_chunksize(wn_data));
        last_log_ms = now_ms;
    }
}

esp_err_t wake_word_init(void)
{
    ESP_LOGI(TAG, "初始化唤醒词检测 (esp_wn_iface 方案)...");

    srmodel_list_t *models = esp_srmodel_init("model");
    if (models == NULL || models->num <= 0) {
        ESP_LOGE(TAG, "加载模型列表失败或模型为空");
        return ESP_FAIL;
    }

    char *wn_model_name = esp_srmodel_filter(models, ESP_WN_PREFIX, NULL);
    if (wn_model_name == NULL) {
        ESP_LOGE(TAG, "未找到 WakeNet 模型");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "找到唤醒词模型: %s", wn_model_name);

    // 使用 esp_wn_iface（esp_wake_word.cc 方案）
    wn_iface = esp_wn_handle_from_name(wn_model_name);
    if (wn_iface == NULL) {
        ESP_LOGE(TAG, "获取 WakeNet 接口失败");
        return ESP_FAIL;
    }

    wn_data = wn_iface->create(wn_model_name, DET_MODE_95);
    if (wn_data == NULL) {
        ESP_LOGE(TAG, "创建 WakeNet 数据失败");
        return ESP_FAIL;
    }

    int sample_rate = wn_iface->get_samp_rate(wn_data);
    int chunksize = wn_iface->get_samp_chunksize(wn_data);
    ESP_LOGI(TAG, "WakeNet 采样率: %d Hz, chunksize: %d samples", sample_rate, chunksize);

    ESP_LOGI(TAG, "WakeNet 检测已初始化");
    return ESP_OK;
}

void wake_word_set_callback(void (*callback)(const char *wake_word))
{
    wake_word_callback = callback;
}

void wake_word_reset(void)
{
    if (wn_iface != NULL && wn_data != NULL) {
        ESP_LOGI(TAG, "WakeNet 状态重置");
        wn_iface->reset_det_threshold(wn_data);
    }
    last_trigger_time_ms = 0;
}

int wake_word_get_chunksize(void)
{
    if (wn_iface == NULL || wn_data == NULL) {
        return 512;
    }
    return wn_iface->get_samp_chunksize(wn_data);
}

void wake_word_deinit(void)
{
    if (wn_iface != NULL && wn_data != NULL) {
        wn_iface->destroy(wn_data);
        wn_data = NULL;
        wn_iface = NULL;
    }

    ESP_LOGI(TAG, "唤醒词检测已关闭");
}