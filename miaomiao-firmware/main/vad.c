/**
 * 独立 VAD（语音活动检测）模块
 *
 * 使用 esp_vad.h 直接检测语音，无需通过 AFE
 * VAD API 来自 esp-sr 组件
 */

#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>

#include "esp_vad.h"
#include "config.h"

static const char *TAG = "VAD";

/* VAD 句柄 */
static vad_handle_t vad_handle = NULL;

/* VAD 状态 */
static bool vad_speech_detected = false;
static TaskHandle_t vad_task_handle = NULL;
static QueueHandle_t vad_audio_queue = NULL;

/* VAD 配置 */
static const int MY_VAD_FRAME_MS = 30;
static const int MY_VAD_SAMPLE_RATE = 16000;
static int vad_chunksize = 0;

/* 音频队列消息 */
typedef struct {
    int16_t samples[512];
    int count;
} vad_audio_msg_t;

/**
 * 初始化独立 VAD
 */
esp_err_t vad_init(void)
{
    ESP_LOGI(TAG, "初始化独立 VAD...");

    /* 创建 VAD 实例 - VAD_MODE_0 是最低灵敏度，避免环境音误触发 */
    vad_handle = vad_create(VAD_MODE_0);
    if (vad_handle == NULL) {
        ESP_LOGE(TAG, "创建 VAD 实例失败");
        return ESP_FAIL;
    }

    vad_chunksize = MY_VAD_SAMPLE_RATE * MY_VAD_FRAME_MS / 1000;
    ESP_LOGI(TAG, "VAD 初始化完成, 分块大小: %d samples", vad_chunksize);

    /* 创建音频队列 */
    vad_audio_queue = xQueueCreate(10, sizeof(vad_audio_msg_t));
    if (vad_audio_queue == NULL) {
        ESP_LOGE(TAG, "创建音频队列失败");
        return ESP_FAIL;
    }

    vad_speech_detected = false;
    return ESP_OK;
}

/**
 * 检查是否检测到语音
 */
bool vad_is_speech_detected(void)
{
    return vad_speech_detected;
}

/**
 * 重置 VAD 状态，唤醒检测任务准备下一轮
 */
void vad_reset(void)
{
    vad_speech_detected = false;

    /* 如果任务处于挂起状态，唤醒它 */
    if (vad_task_handle != NULL && eTaskGetState(vad_task_handle) == eSuspended) {
        vTaskResume(vad_task_handle);
    }
}

/**
 * 发送音频数据到 VAD
 */
void vad_feed(int16_t *samples, int count)
{
    if (vad_audio_queue == NULL || vad_speech_detected) {
        return;
    }

    vad_audio_msg_t msg;

    while (count >= vad_chunksize) {
        msg.count = vad_chunksize;
        memcpy(msg.samples, samples, vad_chunksize * sizeof(int16_t));

        if (xQueueSend(vad_audio_queue, &msg, 0) != pdTRUE) {
            break;
        }

        samples += vad_chunksize;
        count -= vad_chunksize;
    }
}

/**
 * VAD 检测任务 - 循环检测模式，不自删除
 */
static void vad_task(void *arg)
{
    ESP_LOGI(TAG, "VAD 任务启动");

    while (1) {
        vad_audio_msg_t msg;

        if (xQueueReceive(vad_audio_queue, &msg, pdMS_TO_TICKS(100)) == pdTRUE) {
            /* 检测语音活动 */
            vad_state_t state = vad_process(vad_handle, msg.samples, MY_VAD_SAMPLE_RATE, MY_VAD_FRAME_MS);

            if (state == VAD_SPEECH) {
                ESP_LOGI(TAG, "检测到语音活动");
                vad_speech_detected = true;

                /* 清空队列，准备下一轮检测 */
                xQueueReset(vad_audio_queue);

                /* 不自删除，挂起等待主循环重启 */
                vTaskSuspend(NULL);
                /* 这里不会执行到 */
            }
        }

        /* 检测到语音后挂起，不再循环 */
        if (vad_speech_detected) {
            vTaskSuspend(NULL);
        }
    }
}

/**
 * 启动 VAD 检测
 */
esp_err_t vad_start(void)
{
    if (vad_handle == NULL) {
        return ESP_FAIL;
    }

    if (vad_task_handle != NULL) {
        /* 任务已存在，检查是否处于挂起状态，如果是则唤醒 */
        if (eTaskGetState(vad_task_handle) == eSuspended) {
            ESP_LOGI(TAG, "唤醒已存在的 VAD 任务");
            vad_reset();
            return ESP_OK;
        }
        return ESP_OK;
    }

    vad_reset();

    BaseType_t ret = xTaskCreatePinnedToCore(
        vad_task,
        "vad",
        3072,
        NULL,
        5,
        &vad_task_handle,
        0
    );

    if (ret != pdPASS) {
        ESP_LOGE(TAG, "创建 VAD 任务失败");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "VAD 检测已启动");
    return ESP_OK;
}

/**
 * 停止 VAD 检测
 */
void vad_stop(void)
{
    if (vad_task_handle != NULL) {
        vTaskDelete(vad_task_handle);
        vad_task_handle = NULL;
    }

    if (vad_audio_queue != NULL) {
        xQueueReset(vad_audio_queue);
    }
}

/**
 * 释放 VAD 资源
 */
void vad_deinit(void)
{
    vad_stop();

    if (vad_handle != NULL) {
        vad_destroy(vad_handle);
        vad_handle = NULL;
    }

    if (vad_audio_queue != NULL) {
        vQueueDelete(vad_audio_queue);
        vad_audio_queue = NULL;
    }

    ESP_LOGI(TAG, "VAD 已关闭");
}