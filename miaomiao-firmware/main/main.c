/**
 * 苗苗 ESP32 固件 - 主程序入口
 *
 * 功能流程：
 * 1. 启动 → 连接 WiFi → 初始化硬件
 * 2. 等待唤醒词"喵喵" (WakeNet 独立检测)
 * 3. 唤醒后开始对话（VAD 检测语音活动）
 * 4. 发送音频到后端 → 接收 AI 回复 → 播放 TTS
 * 5. 30秒无说话 → 主动说"我去休息一下啦"
 * 6. 回到等待唤醒词状态
 *
 * 架构：参考 xiaozhi-esp32，使用独立 WakeNet + VAD
 */

#include <stdio.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_system.h>
#include <esp_log.h>
#include <esp_wifi.h>
#include <nvs_flash.h>
#include <driver/gpio.h>

#include "config.h"
#include "wifi.h"
#include "audio.h"
#include "audio_service.h"
#include "display.h"
#include "http_client.h"

static const char *TAG = "MIAOMIAO";

#define IDLE_TIMEOUT_MS    30000   // 30秒无说话则主动提示
#define PROACTIVE_MESSAGE  "我去休息一下啦，有事喊我哦~"

static int64_t get_time_ms(void) {
    return xTaskGetTickCount() * portTICK_PERIOD_MS;
}

/* 硬件初始化 */
static void init_hardware(void)
{
    ESP_LOGI(TAG, "初始化硬件...");

#ifdef ENABLE_SCREEN
    ESP_LOGI(TAG, "开始初始化屏幕...");
    esp_err_t disp_ret = display_init();
    if (disp_ret != ESP_OK) {
        ESP_LOGE(TAG, "屏幕初始化失败: %d", disp_ret);
    } else {
        ESP_LOGI(TAG, "屏幕初始化成功");
        display_show_status("系统启动中", "");
    }
#endif

#ifdef ENABLE_TEST_BLINK
    gpio_set_direction(38, GPIO_MODE_OUTPUT);
    ESP_LOGI(TAG, "开始测试闪烁...");
    for (int i = 0; i < 5; i++) {
        gpio_set_level(38, 1);
        vTaskDelay(pdMS_TO_TICKS(200));
        gpio_set_level(38, 0);
        vTaskDelay(pdMS_TO_TICKS(200));
    }
    gpio_set_level(38, 1);  // 测试结束，确保背光打开
    ESP_LOGI(TAG, "测试闪烁完成");
#endif

    ESP_LOGI(TAG, "开始初始化音频服务...");
    if (audio_service_init() != ESP_OK) {
        ESP_LOGE(TAG, "音频服务初始化失败");
    }

    ESP_LOGI(TAG, "硬件初始化完成");
}

/**
 * 音频采集完成回调
 * 当 WakeNet + VAD 检测到有效语音后调用此回调
 */
static void on_audio_captured(int16_t *samples, int count)
{
    ESP_LOGI(TAG, "===== 采集到音频: %d samples (%d ms) =====",
             count, count * 1000 / AUDIO_SAMPLE_RATE);

#ifdef ENABLE_SCREEN
    display_show_status("正在识别...", "");
#endif

    char response_text[512] = {0};
    uint8_t *audio_response = NULL;
    size_t audio_len = 0;

    // 发送到后端获取 AI 回复
    ESP_LOGI(TAG, "发送音频到后端: %d bytes", count * sizeof(int16_t));
    int ret = http_chat(samples, count * sizeof(int16_t),
                        response_text, &audio_response, &audio_len);

    ESP_LOGI(TAG, "后端响应: ret=%d, text='%s', audio_len=%d",
             ret, response_text, audio_len);

    if (ret == 0 && audio_response && audio_len > 0) {
        // 显示 AI 回复文字
#ifdef ENABLE_SCREEN
        display_show_message("苗苗:", response_text);
#endif

        // 播放 TTS 音频
#ifdef ENABLE_SCREEN
        display_show_status("正在播放...", "");
#endif
        audio_play(audio_response, audio_len);

        free(audio_response);
    } else if (ret == 0 && response_text[0] != '\0') {
        // 没有音频，只显示文字
#ifdef ENABLE_SCREEN
        display_show_message("苗苗:", response_text);
        display_show_status("按任意键继续", "");
#endif
    } else {
        ESP_LOGW(TAG, "后端响应异常");
#ifdef ENABLE_SCREEN
        display_show_status("连接异常", "返回待机");
#endif
    }
}

/* WiFi 连接失败处理 */
static void wifi_fail_recovery(void)
{
    ESP_LOGW(TAG, "WiFi 连接失败，10 秒后重试...");
#ifdef ENABLE_SCREEN
    display_show_status("WiFi 失败", "10秒后重试...");
#endif
    vTaskDelay(pdMS_TO_TICKS(10000));
    esp_restart();
}

/**
 * 主动提醒任务
 * 检查 30 秒无活动后发送 TTS 提示
 */
static void proactive_task(void *arg)
{
    int64_t last_talk_time = 0;

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));

        int64_t now = get_time_ms();
        if (last_talk_time > 0 && (now - last_talk_time) > IDLE_TIMEOUT_MS) {
            ESP_LOGI(TAG, "30秒无说话，主动提示");

#ifdef ENABLE_SCREEN
            display_show_message("苗苗:", PROACTIVE_MESSAGE);
#endif

            uint8_t *audio_response = NULL;
            size_t audio_len = 0;

            int ret = http_proactive_message(PROACTIVE_MESSAGE, &audio_response, &audio_len);
            if (ret == 0 && audio_response && audio_len > 0) {
                audio_play(audio_response, audio_len);
                free(audio_response);
            }

            last_talk_time = 0;  // 重置，避免重复发送
        }
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "========== 苗苗 ESP32 固件启动 ==========");
    ESP_LOGI(TAG, "硬件: ESP32-S3 + INMP441 + MAX98357A + ST7789");
    ESP_LOGI(TAG, "后端: %s:%d", BACKEND_HOST, BACKEND_PORT);
    ESP_LOGI(TAG, "架构: 独立 WakeNet + VAD (小智模式)");

    // 初始化 NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // 初始化硬件
    init_hardware();

    // 连接 WiFi（失败会一直重试）
    int retry_count = 0;
    while (wifi_connect(WIFI_SSID, WIFI_PASSWORD) != ESP_OK) {
        retry_count++;
        ESP_LOGE(TAG, "WiFi 连接失败 (尝试 %d)", retry_count);
        if (retry_count >= 5) {
            wifi_fail_recovery();
        }
        vTaskDelay(pdMS_TO_TICKS(2000));
    }

    ESP_LOGI(TAG, "WiFi 连接成功");

    vTaskDelay(pdMS_TO_TICKS(2000));

#ifdef ENABLE_SCREEN
    display_show_status("WiFi 已连接", "开始待机...");
    vTaskDelay(pdMS_TO_TICKS(1000));
#endif

    // 启动音频服务（开始监听唤醒词）
    // 音频服务在 PHASE_CAPTURING 状态自动发送音频到后端，无需回调
    if (audio_service_start() != ESP_OK) {
        ESP_LOGE(TAG, "音频服务启动失败");
    }

    // 创建主动提醒任务
    xTaskCreate(proactive_task, "proactive", 4096, NULL, 3, NULL);

    ESP_LOGI(TAG, "========== 苗苗运行中 ==========");
    ESP_LOGI(TAG, "等待唤醒词 '喵喵'...");
}