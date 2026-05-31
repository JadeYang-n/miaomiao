/**
 * 音频服务 - 小智 audio_pipeline 架构
 *
 * 3-Task Pipeline：
 * - AudioInputTask (Core 0): I2S 麦克风录音，Opus 编码
 * - OpusCodecTask (Core 1): Opus 编码/解码
 * - AudioOutputTask (Core 1): 解码后 PCM → I2S 播放
 *
 * WebSocket：长连接传输 Opus 数据包
 */

#include <string.h>
#include <math.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <esp_random.h>
#include <driver/gpio.h>

#include "config.h"
#include "audio_afe.h"
#include "wake_word.h"
#include "ws_client.h"
#include "opus_codec.h"
#include "mp3dec.h"
#include "audio_i2s.h"
#include "display.h"
#include "audio.h"

static const char *TAG = "AUDIO_SERVICE";

/* 小智 pipeline 配置 */
#define OPUS_SAMPLE_RATE       16000
#define OPUS_FRAME_DURATION_MS 60
#define OPUS_FRAME_SIZE        ((OPUS_SAMPLE_RATE * OPUS_FRAME_DURATION_MS) / 1000) // 960 samples
#define OPUS_BITRATE           64000

/* 能量检测阈值（区分环境噪音和人声） */
// 改用峰值检测：环境噪音峰值 < 3000，人说话峰值 > 5000
#define SPEECH_PEAK_THRESHOLD   8000   // 峰值 > 8000 表示人在说话（静音约6000-8000）
#define SILENCE_DURATION_MS     3500   // 连续 3.5 秒峰值 < 3000 认为说话结束（用户停顿不算结束）
#define MIN_CAPTURE_TIME_MS     500   // 唤醒后至少采集 0.5 秒音频才允许发送

/* 队列最大深度 */
#define MAX_ENCODE_QUEUE   2
#define MAX_PLAYBACK_QUEUE 2
#define MAX_SEND_QUEUE     40
#define MAX_DECODE_QUEUE   40

/* 阶段状态机 */
typedef enum {
    PHASE_IDLE,          // 等待唤醒
    PHASE_WAKED,         // 已唤醒，等待语音开始
    PHASE_SPEAKING,      // 说话中，采集语音
    PHASE_CAPTURING,     // 正在发送音频
    PHASE_PLAYING        // 正在播放音频
} phase_state_t;

/* 音频包 */
typedef struct {
    uint8_t *data;
    size_t len;
    uint32_t timestamp;
} audio_packet_t;

/* 任务句柄 */
static TaskHandle_t audio_input_task_handle = NULL;
static TaskHandle_t opus_codec_task_handle = NULL;
static TaskHandle_t audio_output_task_handle = NULL;
static TaskHandle_t mp3_playback_task_handle = NULL;

/* 队列 */
static QueueHandle_t audio_encode_queue = NULL;
static QueueHandle_t audio_playback_queue = NULL;
static QueueHandle_t audio_send_queue = NULL;
static QueueHandle_t audio_decode_queue = NULL;
static QueueHandle_t mp3_playback_queue = NULL;  // MP3 播放队列（Base64 解码后的 MP3 数据）

/* 状态 */
static bool service_initialized = false;
static phase_state_t g_phase = PHASE_IDLE;

/* 智能休眠：对话结束后 30 秒无唤醒，自动说"我去休息啦" */
#define IDLE_SLEEP_TIMEOUT_MS  30000
static int64_t g_last_conversation_end_ms = 0;

/* 编解码器 */
static opus_codec_handle_t g_opus_enc = NULL;
static opus_codec_handle_t g_opus_dec = NULL;

/* WebSocket 客户端 */
static ws_client_handle_t g_ws_client = NULL;
static bool g_ws_connected = false;
static bool g_ws_auto_reconnect = false;

/* AFE 句柄（用于播放时暂停 RX） */
static audio_afe_t *g_afe = NULL;

/* 录音缓冲 */
static int16_t *g_audio_buffer = NULL;
static int g_audio_capture_pos = 0;
static bool g_recording = false;


/* AFE feed_samples 大小 */
static int g_feed_samples = 0;

/* 说话超时检测 */
static int64_t g_speaking_start_ms = 0;

/* 唤醒冷却时间（防止 WakeNet 重复触发） */
static int64_t g_wake_cooldown_end_ms = 0;
#define WAKE_COOLDOWN_MS 2000

/* 唤醒状态 */
static volatile bool g_wake_word_detected = false;
static volatile bool g_vad_speaking = false;
static volatile int g_wake_word_count = 0;  // 调试用

/* WebSocket hello 状态 */
static volatile bool g_ws_need_send_hello = false;

/* 能量检测（替代 VAD silence） */
static int64_t g_silence_start_ms = 0;  // 最近一次检测到安静的时刻
static bool g_energy_speaking = false;   // 当前是否在说话（基于能量）

/* 音频发送完成信号量 */
static SemaphoreHandle_t g_capture_done_sem = NULL;

/* MP3 播放完成信号量 */
static SemaphoreHandle_t g_mp3_play_done_sem = NULL;

/* MP3 播放中标志（抑制 VAD 和唤醒词检测） */
static volatile bool g_mp3_playing = false;

/* 前向声明 */
static void ws_reconnect_task(void *arg);

/* ==================== WebSocket 回调 ==================== */

static void on_ws_connected(void)
{
    ESP_LOGI(TAG, ">>> WebSocket 已连接");
    g_ws_connected = true;
    g_ws_need_send_hello = true;
}

static void on_ws_disconnected(void)
{
    ESP_LOGI(TAG, ">>> WebSocket 已断开");
    g_ws_connected = false;

    // 如果不是因为主动关闭，开始自动重连
    if (!g_ws_auto_reconnect) {
        g_ws_auto_reconnect = true;
        xTaskCreate(&ws_reconnect_task, "ws_reconnect", 4096, NULL, 3, NULL);
    }
}

static void on_ws_message(const ws_message_t *msg)
{
    if (msg->type == WS_EVENT_TEXT) {
        // 解析 JSON（确保 null 终止）
        size_t len = msg->len;
        char *data = (char *)malloc(len + 1);
        if (!data) return;
        memcpy(data, msg->data, len);
        data[len] = '\0';
        ESP_LOGI(TAG, "收到文本 len=%d: %.100s", (int)len, data);

        // 检查是否是 hello 响应
        if (strstr(data, "\"type\":\"hello\"") != NULL) {
            ESP_LOGI(TAG, "收到服务端 hello");
        }
        // 检查是否是 audio 类型（MP3 音频数据）→ 异步播放
        else if (strstr(data, "\"type\":\"audio\"") != NULL && strstr(data, "\"audio\":\"") != NULL) {
            ESP_LOGI(TAG, "检测到 audio 消息，开始解析 Base64");
            char *audio_start = strstr(data, "\"audio\":\"");
            ESP_LOGI(TAG, "audio_start=%p, data=%p", audio_start, data);
            if (audio_start) {
                audio_start += 9;
                char *audio_end = strchr(audio_start, '"');
                ESP_LOGI(TAG, "audio_end=%p, audio_start=%p, diff=%d", audio_end, audio_start, audio_end ? (int)(audio_end - audio_start) : -1);
                if (audio_end) {
                    size_t b64_len = audio_end - audio_start;
                    ESP_LOGI(TAG, "Base64 长度: %d bytes", b64_len);
                    if (b64_len > 0 && b64_len < 1000000) {
                        // Base64 解码
                        uint8_t *mp3_buf = (uint8_t *)malloc(b64_len);
                        if (mp3_buf) {
                            size_t mp3_len = 0;
                            for (size_t i = 0; i + 4 <= b64_len && (audio_start + i + 4) <= audio_end; i += 4) {
                                uint32_t n = 0;
                                int padding = 0;
                                for (int j = 0; j < 4; j++) {
                                    char c = audio_start[i + j];
                                    if (c == '=') { padding++; continue; }
                                    if (c >= 'A' && c <= 'Z') n = (n << 6) | (c - 'A');
                                    else if (c >= 'a' && c <= 'z') n = (n << 6) | (c - 'a' + 26);
                                    else if (c >= '0' && c <= '9') n = (n << 6) | (c - '0' + 52);
                                    else if (c == '+') n = (n << 6) | 62;
                                    else if (c == '/') n = (n << 6) | 63;
                                }
                                mp3_buf[mp3_len++] = (n >> 16) & 0xFF;
                                if (padding < 2) mp3_buf[mp3_len++] = (n >> 8) & 0xFF;
                                if (padding < 1) mp3_buf[mp3_len++] = n & 0xFF;
                            }
                            ESP_LOGI(TAG, "Base64 解码完成: %d bytes", mp3_len);
                            if (mp3_len > 0 && mp3_playback_queue) {
                                audio_packet_t *pkt = (audio_packet_t *)malloc(sizeof(audio_packet_t));
                                if (pkt) {
                                    pkt->data = mp3_buf;
                                    pkt->len = mp3_len;
                                    pkt->timestamp = 0;
                                    if (xQueueSend(mp3_playback_queue, &pkt, 0) != pdTRUE) {
                                        ESP_LOGW(TAG, "MP3 播放队列满，丢弃");
                                        free(mp3_buf);
                                        free(pkt);
                                    } else {
                                        ESP_LOGI(TAG, "MP3 数据已入队: %d bytes", mp3_len);
                                    }
                                } else {
                                    free(mp3_buf);
                                }
                            } else {
                                free(mp3_buf);
                            }
                        }
                    }
                }
            }
        }
        free(data);
    } else if (msg->type == WS_EVENT_BINARY) {
        // MP3 音频数据（后端用 binary frame 发送），直接放入 MP3 播放队列
        ESP_LOGI(TAG, "收到 MP3 binary 帧: %d bytes", msg->len);
        if (mp3_playback_queue && msg->len > 0) {
            audio_packet_t *pkt = (audio_packet_t *)malloc(sizeof(audio_packet_t));
            if (pkt) {
                pkt->data = (uint8_t *)malloc(msg->len);
                if (pkt->data) {
                    memcpy(pkt->data, msg->data, msg->len);
                    pkt->len = msg->len;
                    pkt->timestamp = 0;
                    if (xQueueSend(mp3_playback_queue, &pkt, 0) != pdTRUE) {
                        ESP_LOGW(TAG, "MP3 播放队列满，丢弃");
                        free(pkt->data);
                        free(pkt);
                    } else {
                        ESP_LOGI(TAG, "MP3 binary 数据已入队: %d bytes", msg->len);
                    }
                } else {
                    free(pkt);
                }
            }
        }
    }
}

/* ==================== WakeWord 回调 ==================== */

static void on_wake_word(const char *wake_word, int wake_word_index)
{
    if (g_mp3_playing) {
        ESP_LOGD(TAG, "WAKE_DETECTED 但 MP3 播放中，忽略");
        return;
    }
    ESP_LOGI(TAG, "##### WAKE_DETECTED: %s (index=%d) #####", wake_word, wake_word_index);
    g_wake_word_detected = true;
    g_wake_word_count++;
    ESP_LOGI(TAG, "##### WAKE_DETECTED: set true, count=%d #####", g_wake_word_count);
}

static void wake_word_callback_wrapper(const char *wake_word)
{
    static int64_t last_callback_ms = 0;
    static int callback_toggle = 0;
    int64_t now = esp_timer_get_time() / 1000;
    ESP_LOGI(TAG, "[CALLBACK] wake_word_callback_wrapper called: %s (delta=%lldms)",
             wake_word, now - last_callback_ms);
    last_callback_ms = now;
    g_wake_word_detected = true;
    g_wake_word_count++;
    // Debug LED toggle on GPIO 38 to visually confirm callback
    gpio_set_direction(38, GPIO_MODE_OUTPUT);
    gpio_set_level(38, callback_toggle ? 1 : 0);
    callback_toggle = !callback_toggle;
    ESP_LOGI(TAG, "[CALLBACK] g_wake_word_detected=%d, count=%d", g_wake_word_detected, g_wake_word_count);
}

/* ==================== VAD 回调 ==================== */

static void on_vad_change(bool speaking)
{
    if (g_mp3_playing) return;  // 播放中忽略 VAD
    g_vad_speaking = speaking;
}

/* ==================== AFE 音频回调 ==================== */

// 全局峰值（用于峰值检测）
static volatile int g_audio_peak = 0;

static void on_afe_output(const int16_t *data, int samples)
{
    // 计算这一帧的峰值
    int peak = 0;
    for (int i = 0; i < samples; i++) {
        int abs_val = data[i] >= 0 ? data[i] : -data[i];
        if (abs_val > peak) peak = abs_val;
    }
    g_audio_peak = peak;

    // 【Step 1 调试】每次回调都打印（不管 g_recording），确认 AFE 是否持续触发
    static int dbg_afe_count = 0;
    if (dbg_afe_count < 50) {
        ESP_LOGI(TAG, ">>> AFE_CB[%d]: samples=%d, peak=%d, rec=%d, pos=%d",
                 dbg_afe_count, samples, peak, g_recording, g_audio_capture_pos);
        dbg_afe_count++;
    }

    if (!g_recording) return;

    // 录音期间打印 buffer 状态（前10次）
    static int dbg_call_count = 0;
    if (g_recording && dbg_call_count < 10) {
        ESP_LOGI(TAG, ">>> on_afe_output: samples=%d, capture_pos=%d, recording=%d, peak=%d",
                 samples, g_audio_capture_pos, g_recording, peak);
        dbg_call_count++;
    } else if (!g_recording) {
        dbg_call_count = 0;  // 重置
    }

    if (!g_recording) return;

    // 调试日志
    static int dbg_enter_count = 0;
    if (dbg_enter_count < 3) {
        ESP_LOGI(TAG, ">>> on_afe: enter check: g_recording=%d, pos=%d, samples=%d, check=%d",
                 g_recording, g_audio_capture_pos, samples,
                 g_audio_capture_pos + samples < 8000);
        dbg_enter_count++;
    }

    if (g_audio_capture_pos + samples < 8000) {
        memcpy(&g_audio_buffer[g_audio_capture_pos], data, samples * sizeof(int16_t));
        g_audio_capture_pos += samples;
        // 日志打印buffer状态
        static int dbg_buf_count = 0;
        if (dbg_buf_count < 5) {
            ESP_LOGI(TAG, "BUFFER: pos=%d, samples=%d, free=%d",
                     g_audio_capture_pos, samples, 8000 - g_audio_capture_pos - samples);
            dbg_buf_count++;
        }
    } else {
        ESP_LOGW(TAG, "BUFFER FULL: pos=%d, samples=%d", g_audio_capture_pos, samples);
    }
}

/* ==================== WebSocket 重连任务 ==================== */

static void ws_reconnect_task(void *arg)
{
    (void)arg;
    ESP_LOGI(TAG, "WebSocket 重连任务启动，每 5 秒重试...");

    while (g_ws_auto_reconnect) {
        vTaskDelay(5000 / portTICK_PERIOD_MS);

        if (!g_ws_connected && g_ws_client) {
            ESP_LOGI(TAG, "尝试重新连接 WebSocket...");
            if (ws_client_connect(g_ws_client, 5000)) {
                ESP_LOGI(TAG, "WebSocket 重连成功!");
                g_ws_auto_reconnect = false;
            } else {
                ESP_LOGI(TAG, "WebSocket 重连失败，继续重试...");
            }
        }
    }

    vTaskDelete(NULL);
}

/* ==================== AudioInputTask ==================== */

static void audio_input_task(void *arg)
{
    (void)arg;
    ESP_LOGI(TAG, "AudioInputTask 启动 (Core %d)", xPortGetCoreID());

    int16_t pcm_buffer[OPUS_FRAME_SIZE];

    while (1) {
        // 调试日志
        static int64_t last_debug = 0;
        int64_t now = esp_timer_get_time() / 1000;
        if (now - last_debug > 2000) {
            ESP_LOGI(TAG, "AudioInput: ws=%d, rec=%d, pos=%d",
                     g_ws_connected, g_recording, g_audio_capture_pos);
            last_debug = now;
        }

        // 等待累积足够的样本（OPUS_FRAME_SIZE = 960 samples）
        if (g_ws_connected && g_audio_capture_pos >= OPUS_FRAME_SIZE) {
            ESP_LOGI(TAG, "AudioInputTask: 准备发送, pos=%d", g_audio_capture_pos);
            // 复制一帧数据（960 samples）
            memcpy(pcm_buffer, g_audio_buffer, OPUS_FRAME_SIZE * sizeof(int16_t));
            g_audio_capture_pos -= OPUS_FRAME_SIZE;
            memmove(g_audio_buffer, &g_audio_buffer[OPUS_FRAME_SIZE], g_audio_capture_pos * sizeof(int16_t));

            // 直接发送 PCM 数据（跳过 Opus 编码，用于调试）
            // 将 PCM 数据编码为 Base64
            int pcm_bytes = OPUS_FRAME_SIZE * sizeof(int16_t);  // 1920 bytes
            char b64[3000];  // 1920 * 4/3 + padding ≈ 2564
            static const char b64_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
            size_t j = 0;
            for (int i = 0; i < pcm_bytes; i += 3) {
                int a = ((uint8_t *)pcm_buffer)[i];
                int b = (i + 1 < pcm_bytes) ? ((uint8_t *)pcm_buffer)[i + 1] : 0;
                int c = (i + 2 < pcm_bytes) ? ((uint8_t *)pcm_buffer)[i + 2] : 0;
                b64[j++] = b64_chars[(a >> 2) & 0x3F];
                b64[j++] = b64_chars[((a << 4) | (b >> 4)) & 0x3F];
                b64[j++] = (i + 1 < pcm_bytes) ? b64_chars[((b << 2) | (c >> 6)) & 0x3F] : '=';
                b64[j++] = (i + 2 < pcm_bytes) ? b64_chars[c & 0x3F] : '=';
            }
            b64[j] = '\0';

            char msg[4096];
            snprintf(msg, sizeof(msg), "{\"type\":\"audio\",\"audio\":\"%s\",\"format\":\"pcm_s16le\",\"sample_rate\":16000,\"channels\":1}", b64);
            ESP_LOGI(TAG, "SEND_AUDIO: ws=%d, pcm_bytes=%d, msg_len=%d", g_ws_connected, pcm_bytes, strlen(msg));
            if (!g_ws_connected) {
                ESP_LOGW(TAG, "SEND_AUDIO: WebSocket not connected, skipping!");
                continue;
            }
            bool send_ok = ws_client_send_text(g_ws_client, msg);
            ESP_LOGI(TAG, "SEND_RESULT: %s", send_ok ? "OK" : "FAILED");
            vTaskDelay(20 / portTICK_PERIOD_MS);  // 减慢发送速度，让 WiFi 总线没那么忙
        } else if (!g_recording && g_audio_capture_pos > 0) {
            // recording=false 但 buffer 还有数据，强制发送剩余数据
            ESP_LOGI(TAG, "AudioInputTask: 强制发送剩余数据, pos=%d", g_audio_capture_pos);
            // 用零填充到 OPUS_FRAME_SIZE
            memset(pcm_buffer, 0, OPUS_FRAME_SIZE * sizeof(int16_t));
            int to_copy = (g_audio_capture_pos < OPUS_FRAME_SIZE) ? g_audio_capture_pos : OPUS_FRAME_SIZE;
            memcpy(pcm_buffer, g_audio_buffer, to_copy * sizeof(int16_t));
            g_audio_capture_pos = 0;

            // 直接发送 PCM 数据（跳过 Opus 编码，用于调试）
            int pcm_bytes = OPUS_FRAME_SIZE * sizeof(int16_t);  // 1920 bytes
            char b64[3000];
            static const char b64_chars2[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
            size_t j = 0;
            for (int i = 0; i < pcm_bytes; i += 3) {
                int a = ((uint8_t *)pcm_buffer)[i];
                int b = (i + 1 < pcm_bytes) ? ((uint8_t *)pcm_buffer)[i + 1] : 0;
                int c = (i + 2 < pcm_bytes) ? ((uint8_t *)pcm_buffer)[i + 2] : 0;
                b64[j++] = b64_chars2[(a >> 2) & 0x3F];
                b64[j++] = b64_chars2[((a << 4) | (b >> 4)) & 0x3F];
                b64[j++] = (i + 1 < pcm_bytes) ? b64_chars2[((b << 2) | (c >> 6)) & 0x3F] : '=';
                b64[j++] = (i + 2 < pcm_bytes) ? b64_chars2[c & 0x3F] : '=';
            }
            b64[j] = '\0';

            char msg[4096];
            snprintf(msg, sizeof(msg), "{\"type\":\"audio\",\"audio\":\"%s\",\"format\":\"pcm_s16le\",\"sample_rate\":16000,\"channels\":1}", b64);
            ESP_LOGI(TAG, "SEND_REMAINING: pcm_bytes=%d, msg_len=%d", pcm_bytes, strlen(msg));
            ws_client_send_text(g_ws_client, msg);
        } else if (!g_recording && g_audio_capture_pos == 0) {
            // g_recording=false 且缓冲区已空，说明数据全部发送完毕
            ESP_LOGI(TAG, "AudioInputTask: 数据发送完毕，给信号量");
            if (g_capture_done_sem != NULL) {
                xSemaphoreGive(g_capture_done_sem);
            }
            vTaskDelay(10 / portTICK_PERIOD_MS);
        } else if (!g_ws_connected) {
            ESP_LOGW(TAG, "AudioInputTask: WebSocket 未连接，等待...");
            vTaskDelay(100 / portTICK_PERIOD_MS);
        } else {
            vTaskDelay(10 / portTICK_PERIOD_MS);
        }
    }
}

/* ==================== OpusCodecTask ==================== */

static void opus_codec_task(void *arg)
{
    (void)arg;
    ESP_LOGI(TAG, "OpusCodecTask 启动 (Core %d)", xPortGetCoreID());

    audio_packet_t *pkt;

    while (1) {
        if (xQueueReceive(audio_decode_queue, &pkt, portMAX_DELAY) == pdTRUE) {
            // 解码 Opus → PCM
            int16_t pcm_out[OPUS_FRAME_SIZE * 2];
            int samples = opus_dec_process(g_opus_dec, pkt->data, pkt->len, pcm_out, OPUS_FRAME_SIZE * 2);

            if (samples > 0) {
                // 放入播放队列
                int16_t *playback_data = (int16_t *)malloc(samples * sizeof(int16_t));
                if (playback_data) {
                    memcpy(playback_data, pcm_out, samples * sizeof(int16_t));
                    audio_packet_t *playback_pkt = (audio_packet_t *)malloc(sizeof(audio_packet_t));
                    if (playback_pkt) {
                        playback_pkt->data = (uint8_t *)playback_data;
                        playback_pkt->len = samples * sizeof(int16_t);
                        playback_pkt->timestamp = 0;
                        if (xQueueSend(audio_playback_queue, &playback_pkt, 0) != pdTRUE) {
                            free(playback_data);
                            free(playback_pkt);
                        }
                    }
                }
            }

            free(pkt->data);
            free(pkt);
        }
    }
}

/* ==================== AudioOutputTask ==================== */

static void audio_output_task(void *arg)
{
    (void)arg;
    ESP_LOGI(TAG, "AudioOutputTask 启动 (Core %d)", xPortGetCoreID());

    audio_packet_t *pkt;

    while (1) {
        if (xQueueReceive(audio_playback_queue, &pkt, portMAX_DELAY) == pdTRUE) {
            // 播放 PCM
            audio_play(pkt->data, pkt->len);
            free(pkt->data);
            free(pkt);
        }
    }
}

/* ==================== MP3 播放任务（xiaozhi 方式：逐帧解码+直接写入） ==================== */

static void mp3_playback_task(void *arg)
{
    (void)arg;
    ESP_LOGI(TAG, "MP3PlaybackTask 启动 (Core %d)", xPortGetCoreID());

    audio_packet_t *pkt;

    while (1) {
        if (xQueueReceive(mp3_playback_queue, &pkt, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        ESP_LOGI(TAG, "[MP3 PLAY] 开始播放, %d bytes", pkt->len);

        /* 参考 xiaozhi AudioOutputTask：播放前确保 TX 启用 */
        audio_i2s_enable_tx();

        g_recording = false;
        g_mp3_playing = true;
        display_set_audio_active(true);  /* 暂停动画更新 */
        g_wake_word_detected = false;

        HMP3Decoder hDec = MP3InitDecoder();
        if (hDec == NULL) {
            ESP_LOGE(TAG, "[MP3 PLAY] 解码器创建失败");
            free(pkt->data);
            free(pkt);
            g_mp3_playing = false;
            display_set_audio_active(false);  /* 恢复动画更新 */
            continue;
        }

        unsigned char *read_ptr = pkt->data;
        int bytes_left = (int)pkt->len;
        int16_t pcm_frame[1152 * 2];
        int total_samples = 0;
        int frames = 0;

        /* 逐帧解码，直接写入 I2S（xiaozhi 方式） */
        while (bytes_left > 0) {
            int offset = MP3FindSyncWord(read_ptr, bytes_left);
            if (offset < 0) break;
            read_ptr += offset;
            bytes_left -= offset;

            int ret = MP3Decode(hDec, &read_ptr, &bytes_left, pcm_frame, 0);
            if (ret != 0) continue;

            MP3FrameInfo frameInfo;
            MP3GetLastFrameInfo(hDec, &frameInfo);
            int samples = frameInfo.outputSamps;

            /* 立体声 → 单声道 */
            if (frameInfo.nChans == 2) {
                for (int i = 0; i < samples / 2; i++) {
                    pcm_frame[i] = (int16_t)(((int)pcm_frame[i*2] + pcm_frame[i*2+1]) / 2);
                }
                samples /= 2;
            }

            /* 诊断：打印 MP3 信息（首帧）和多帧 PCM 峰值 */
            if (samples > 0) {
                int16_t peak = 0;
                for (int i = 0; i < samples; i++) {
                    int16_t abs_val = pcm_frame[i] < 0 ? -pcm_frame[i] : pcm_frame[i];
                    if (abs_val > peak) peak = abs_val;
                }

                if (frames == 0) {
                    ESP_LOGI(TAG, "[MP3 DIAG] samprate=%d, nChans=%d, bits=%d, samples=%d",
                             frameInfo.samprate, frameInfo.nChans, frameInfo.bitsPerSample, samples);
                }
                /* 每 50 帧打印一次峰值，加上第 1、2、5 帧 */
                if (frames == 1 || frames == 2 || frames == 5 || frames % 50 == 0) {
                    ESP_LOGI(TAG, "[MP3 DIAG] frame=%d, peak=%d, pcm[0..3]: %d %d %d %d",
                             frames, peak, pcm_frame[0], pcm_frame[1], pcm_frame[2], pcm_frame[3]);
                }

                audio_i2s_write(pcm_frame, samples, portMAX_DELAY);
                total_samples += samples;
            }
            frames++;
        }

        ESP_LOGI(TAG, "[MP3 PLAY] 完成: %d 帧, %d samples (%d ms @%dHz)",
                 frames, total_samples, total_samples * 1000 / 24000, 24000);

        MP3FreeDecoder(hDec);
        free(pkt->data);
        free(pkt);

        g_mp3_playing = false;
        display_set_audio_active(false);  /* 恢复动画更新 */

        if (g_mp3_play_done_sem) {
            xSemaphoreGive(g_mp3_play_done_sem);
        }
    }
}

/* ==================== 状态机主循环 ==================== */

static void audio_service_task(void *arg)
{
    (void)arg;
    ESP_LOGI(TAG, "音频服务任务启动");

    /* 使用 VAD AFE 内置的 WakeNet 检测 */
    audio_afe_config_t afe_config = {
        .wake_word_model = "wn9_himiaomiao_tts",
        .vad_init = true,
        .vad_model = NULL,
        .ns_init = false,
        .aec_init = false,
        .memory_alloc_mode = 3,
        .sample_rate = OPUS_SAMPLE_RATE,
        .on_wake_word = on_wake_word,  // VAD AFE 内置 WakeNet 回调
        .on_vad_change = on_vad_change,
    };

    audio_afe_t *afe = audio_afe_create(&afe_config);
    if (afe == NULL) {
        ESP_LOGE(TAG, "AFE 创建失败");
        vTaskDelete(NULL);
        return;
    }
    g_afe = afe;

    audio_afe_set_output_callback(afe, on_afe_output);
    g_feed_samples = audio_afe_get_feed_samples(afe);

    if (audio_afe_start(afe) != 0) {
        ESP_LOGE(TAG, "AFE 启动失败");
        audio_afe_destroy(afe);
        vTaskDelete(NULL);
        return;
    }

    /* 从 AFE 获取 TX 句柄（duplex 模式，与 RX 共享总线） */
    i2s_chan_handle_t tx_handle = audio_afe_get_tx_handle(afe);
    if (tx_handle != NULL) {
        audio_i2s_set_tx_handle(tx_handle);
        ESP_LOGI(TAG, "I2S TX handle 已从 AFE 注入");
    } else {
        ESP_LOGE(TAG, "AFE TX handle 为空，播放不可用");
    }

    /* 初始化独立 WakeNet */
    ESP_LOGI(TAG, "初始化 Standalone WakeNet...");
    if (wake_word_init() != ESP_OK) {
        ESP_LOGE(TAG, "WakeNet 初始化失败");
    } else {
        ESP_LOGI(TAG, "WakeNet 初始化成功");
    }
    wake_word_set_callback(wake_word_callback_wrapper);
    ESP_LOGI(TAG, "Standalone WakeNet 初始化完成");

    /* 初始化 Opus 编解码器 */
    opus_config_t enc_config = {
        .sample_rate = OPUS_SAMPLE_RATE,
        .channels = 1,
        .bitrate = OPUS_BITRATE,
        .frame_duration_ms = OPUS_FRAME_DURATION_MS,
    };
    g_opus_enc = opus_enc_create(&enc_config);

    opus_config_t dec_config = {
        .sample_rate = OPUS_SAMPLE_RATE,
        .channels = 1,
        .frame_duration_ms = OPUS_FRAME_DURATION_MS,
    };
    g_opus_dec = opus_dec_create(&dec_config);

    /* 创建 WebSocket 客户端 */
    char ws_url[128];
    snprintf(ws_url, sizeof(ws_url), "ws://%s:%d/ws/audio", BACKEND_HOST, BACKEND_PORT);
    g_ws_client = ws_client_create(ws_url, NULL, NULL);
    if (g_ws_client) {
        ws_client_set_callbacks(g_ws_client, on_ws_connected, on_ws_disconnected, on_ws_message);
        if (ws_client_connect(g_ws_client, 10000)) {
            ESP_LOGI(TAG, "WebSocket 连接成功");
        } else {
            ESP_LOGE(TAG, "WebSocket 连接失败，启动自动重连...");
            g_ws_auto_reconnect = true;
            xTaskCreate(&ws_reconnect_task, "ws_reconnect", 4096, NULL, 3, NULL);
        }
    }

    ESP_LOGI(TAG, "===== 小智架构已启动，请说'喵喵' =====");

    /* 主循环 */
    int loop_count = 0;
    while (1) {
        loop_count++;
        if (loop_count <= 20 || loop_count % 200 == 0) {
            ESP_LOGI(TAG, "[LOOP %d] g_phase=%d, wake=%d, ws=%d, rec=%d",
                     loop_count, g_phase, g_wake_word_detected, g_ws_connected, g_recording);
        }
        switch (g_phase) {
            case PHASE_IDLE:
                // 发送 hello 消息（连接后立即发送）
                if (g_ws_need_send_hello && g_ws_connected) {
                    g_ws_need_send_hello = false;
                    const char *hello = "{\"type\":\"hello\",\"version\":3,\"transport\":\"websocket\","
                                        "\"features\":{\"aec\":false,\"mcp\":true},"
                                        "\"audio_params\":{\"format\":\"opus\",\"sample_rate\":16000,\"channels\":1,\"frame_duration\":60}}";
                    ESP_LOGI(TAG, "发送客户端 hello");
                    ws_client_send_text(g_ws_client, hello);
                }

                // 调试：打印检测状态（每100次循环）
                static int idle_debug_count = 0;
                if (idle_debug_count < 5) {
                    ESP_LOGI(TAG, "[IDLE] g_wake_word_detected=%d, ws=%d, count=%d",
                             g_wake_word_detected, g_ws_connected, idle_debug_count);
                    idle_debug_count++;
                }
                if (g_wake_word_detected) {
                    int64_t now_ms = esp_timer_get_time() / 1000;
                    // 检查唤醒冷却时间，防止 WakeNet 重复触发
                    if (now_ms < g_wake_cooldown_end_ms) {
                        ESP_LOGW(TAG, "唤醒冷却中 (%lldms remaining)，忽略此次检测",
                                 g_wake_cooldown_end_ms - now_ms);
                        g_wake_word_detected = false;
                        vTaskDelay(50 / portTICK_PERIOD_MS);
                        break;
                    }
                    ESP_LOGI(TAG, "[%lldms] 唤醒成功!", now_ms);
                    g_wake_word_detected = false;
                    g_wake_cooldown_end_ms = now_ms + WAKE_COOLDOWN_MS;
                    g_phase = PHASE_WAKED;
                    g_vad_speaking = false;
                    g_speaking_start_ms = now_ms;
                    ESP_LOGI(TAG, "[%lldms] display_set_expression 调用前", now_ms);
#ifdef ENABLE_SCREEN
                    display_set_expression(2);
#endif
                    ESP_LOGI(TAG, "[%lldms] display_set_expression 调用后，准备 vTaskDelay", now_ms);
                    vTaskDelay(100 / portTICK_PERIOD_MS);
                    ESP_LOGI(TAG, "[%lldms] vTaskDelay 完成，g_phase=%d", esp_timer_get_time() / 1000, g_phase);
                } else {
                    /* 智能休眠：对话结束 30 秒后自动说"我去休息啦" */
                    if (g_last_conversation_end_ms > 0) {
                        int64_t now_ms2 = esp_timer_get_time() / 1000;
                        if ((now_ms2 - g_last_conversation_end_ms) > IDLE_SLEEP_TIMEOUT_MS) {
                            ESP_LOGI(TAG, "30秒无唤醒，自动休眠");
                            g_last_conversation_end_ms = 0;
                            /* 发送休眠消息给后端，请求 TTS */
                            const char *sleep_msg = "{\"type\":\"sleep\"}";
                            if (g_ws_client && ws_client_is_connected(g_ws_client)) {
                                ws_client_send_text(g_ws_client, sleep_msg);
                                ESP_LOGI(TAG, "已发送休眠消息，等待 TTS 回复");
                                /* 等待 MP3 播放完成（最多 15 秒） */
                                if (g_mp3_play_done_sem != NULL) {
                                    while (xSemaphoreTake(g_mp3_play_done_sem, 0) == pdTRUE) {}
                                    xSemaphoreTake(g_mp3_play_done_sem, pdMS_TO_TICKS(15000));
                                }
                            }
                        }
                    }
                    vTaskDelay(50 / portTICK_PERIOD_MS);
                }
                break;

            case PHASE_WAKED:
                // 唤醒后直接开始录音，不再等 VAD（VAD 会被环境噪音骗）
                ESP_LOGI(TAG, "==== PHASE_WAKED: 唤醒成功，直接开始录音 ====");
                g_phase = PHASE_SPEAKING;
                g_audio_capture_pos = 0;
                // 必须在 vTaskDelay 之前设置 g_recording，避免被 on_afe_output 忽略
                g_recording = true;
                display_set_audio_active(true);  /* 暂停动画更新 */
                ESP_LOGI(TAG, "==== PHASE_SPEAKING: 开始录音, recording=%d ====", g_recording);
                g_speaking_start_ms = esp_timer_get_time() / 1000;
                g_silence_start_ms = 0;
                g_energy_speaking = true;
                ESP_LOGI(TAG, "==== PHASE_WAKED 完成，g_phase=%d, recording=%d ====", g_phase, g_recording);
                vTaskDelay(20 / portTICK_PERIOD_MS);
                ESP_LOGI(TAG, "==== PHASE_WAKED vTaskDelay 完成 ====");
                break;

            case PHASE_SPEAKING: {
                /* 用 AFE 的 VAD 信号判断说话是否结束 */
                bool vad_speaking = g_vad_speaking;
                int64_t now_ms = esp_timer_get_time() / 1000;
                int64_t capture_duration = now_ms - g_speaking_start_ms;

                // 每秒打印一次状态
                static int64_t last_debug_ms = 0;
                if (now_ms - last_debug_ms > 1000) {
                    int64_t silence_ms = (g_silence_start_ms == 0) ? 0 : (now_ms - g_silence_start_ms);
                    ESP_LOGI(TAG, "VAD检测: vad=%d, silence=%lldms, duration=%lldms",
                             vad_speaking, silence_ms, capture_duration);
                    last_debug_ms = now_ms;
                }

                if (vad_speaking) {
                    // VAD 检测到语音，重置安静计时
                    g_silence_start_ms = now_ms;
                } else {
                    // VAD 报告安静
                    if (g_silence_start_ms == 0) {
                        g_silence_start_ms = now_ms;
                    } else if ((now_ms - g_silence_start_ms) > SILENCE_DURATION_MS) {
                        // 连续安静超过阈值，说话结束
                        if (capture_duration >= MIN_CAPTURE_TIME_MS) {
                            ESP_LOGI(TAG, "VAD检测：说话结束 (silence=%lldms, duration=%lldms)",
                                     now_ms - g_silence_start_ms, capture_duration);
                            g_recording = false;
                            display_set_audio_active(false);  /* 恢复动画更新 */
                            g_phase = PHASE_CAPTURING;
                            vTaskDelay(200 / portTICK_PERIOD_MS);
                            break;
                        }
                    }
                }

                // 15秒硬超时保护
                if (capture_duration > 15000) {
                    ESP_LOGW(TAG, "说话超时15秒，强制结束");
                    g_recording = false;
                    display_set_audio_active(false);  /* 恢复动画更新 */
                    g_phase = PHASE_CAPTURING;
                    vTaskDelay(200 / portTICK_PERIOD_MS);
                } else {
                    vTaskDelay(20 / portTICK_PERIOD_MS);
                }
                break;
            }

            case PHASE_CAPTURING: {
                // 发完音频后等待后端回复（ASR+AI+TTS 需要 ~13 秒）
                ESP_LOGI(TAG, "===== PHASE_CAPTURING: 等待信号量 =====");
                if (g_capture_done_sem != NULL) {
                    xSemaphoreTake(g_capture_done_sem, pdMS_TO_TICKS(5000));
                }

                // 等待后端 MP3 响应播放完成（最多 30 秒）
                ESP_LOGI(TAG, "===== 等待后端 MP3 响应播放... =====");
                if (g_mp3_play_done_sem != NULL) {
                    // 先清空旧信号量（上一轮残留）
                    while (xSemaphoreTake(g_mp3_play_done_sem, 0) == pdTRUE) {}
                    if (xSemaphoreTake(g_mp3_play_done_sem, pdMS_TO_TICKS(60000)) == pdTRUE) {
                        ESP_LOGI(TAG, "===== MP3 播放完成 =====");
                    } else {
                        ESP_LOGW(TAG, "===== 等待 MP3 播放超时 =====");
                    }
                }

                ESP_LOGI(TAG, "===== 回到等待唤醒词状态 =====");
                g_audio_capture_pos = 0;
                g_last_conversation_end_ms = esp_timer_get_time() / 1000;
                g_phase = PHASE_IDLE;
                wake_word_reset();
#ifdef ENABLE_SCREEN
                display_set_expression(0);  /* 重置表情为正常状态 */
#endif
                break;
            }

            case PHASE_PLAYING:
                vTaskDelay(20 / portTICK_PERIOD_MS);
                break;
        }
    }
}

/* ==================== 初始化 ==================== */

esp_err_t audio_service_init(void)
{
    if (service_initialized) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "初始化音频服务 (小智架构)...");

    /* 分配录音缓冲区 */
    g_audio_buffer = (int16_t *)calloc(8000, sizeof(int16_t));
    if (g_audio_buffer == NULL) {
        ESP_LOGE(TAG, "分配录音缓冲区失败");
        return ESP_FAIL;
    }

    /* 创建队列 */
    audio_encode_queue = xQueueCreate(MAX_ENCODE_QUEUE, sizeof(void *));
    audio_playback_queue = xQueueCreate(MAX_PLAYBACK_QUEUE, sizeof(void *));
    audio_send_queue = xQueueCreate(MAX_SEND_QUEUE, sizeof(void *));
    audio_decode_queue = xQueueCreate(MAX_DECODE_QUEUE, sizeof(void *));
    mp3_playback_queue = xQueueCreate(2, sizeof(void *));  // MP3 播放队列，最多缓存 2 个

    /* 创建信号量（用于同步 AudioInputTask 发完数据） */
    g_capture_done_sem = xSemaphoreCreateBinary();
    if (g_capture_done_sem == NULL) {
        ESP_LOGE(TAG, "创建信号量失败");
        return ESP_FAIL;
    }

    /* 创建信号量（用于同步 MP3 播放完成） */
    g_mp3_play_done_sem = xSemaphoreCreateBinary();
    if (g_mp3_play_done_sem == NULL) {
        ESP_LOGE(TAG, "创建 MP3 播放信号量失败");
        return ESP_FAIL;
    }

    service_initialized = true;
    ESP_LOGI(TAG, "音频服务初始化完成");
    return ESP_OK;
}

esp_err_t audio_service_start(void)
{
    if (!service_initialized) {
        return ESP_FAIL;
    }

    /* I2S TX 初始化移到 audio_service_task 中，在 AFE 之后 */

    /* 创建主状态机任务 */
    TaskHandle_t main_task;
    BaseType_t ret = xTaskCreatePinnedToCore(
        audio_service_task,
        "audio_service",
        8192,
        NULL,
        5,
        &main_task,
        0
    );

    if (ret != pdPASS) {
        ESP_LOGE(TAG, "创建服务任务失败");
        return ESP_FAIL;
    }

    /* 创建 AudioInputTask */
    ret = xTaskCreatePinnedToCore(
        audio_input_task,
        "audio_input",
        12288,
        NULL,
        8,
        &audio_input_task_handle,
        0
    );

    /* 创建 OpusCodecTask */
    ret = xTaskCreatePinnedToCore(
        opus_codec_task,
        "opus_codec",
        12288,
        NULL,
        5,
        &opus_codec_task_handle,
        1
    );

    /* 创建 AudioOutputTask */
    ret = xTaskCreatePinnedToCore(
        audio_output_task,
        "audio_output",
        12288,
        NULL,
        4,
        &audio_output_task_handle,
        1
    );

    /* 创建 MP3 播放任务 */
    ret = xTaskCreatePinnedToCore(
        mp3_playback_task,
        "mp3_playback",
        12288,  // 栈大小：逐帧解码需要 ~6KB pcm_frame buffer
        NULL,
        3,      // 优先级：低于音频输入(8)和输出(4)
        &mp3_playback_task_handle,
        1       // Core 1
    );

    ESP_LOGI(TAG, "小智 audio_pipeline 已启动");
    return ESP_OK;
}

void audio_service_stop(void)
{
    if (audio_input_task_handle) {
        vTaskDelete(audio_input_task_handle);
        audio_input_task_handle = NULL;
    }
    if (opus_codec_task_handle) {
        vTaskDelete(opus_codec_task_handle);
        opus_codec_task_handle = NULL;
    }
    if (audio_output_task_handle) {
        vTaskDelete(audio_output_task_handle);
        audio_output_task_handle = NULL;
    }
    if (mp3_playback_task_handle) {
        vTaskDelete(mp3_playback_task_handle);
        mp3_playback_task_handle = NULL;
    }
    if (g_ws_client) {
        ws_client_disconnect(g_ws_client);
    }
}

void audio_service_deinit(void)
{
    audio_service_stop();

    if (g_audio_buffer) {
        free(g_audio_buffer);
        g_audio_buffer = NULL;
    }

    if (g_opus_enc) {
        opus_codec_destroy(g_opus_enc);
        g_opus_enc = NULL;
    }

    if (g_opus_dec) {
        opus_codec_destroy(g_opus_dec);
        g_opus_dec = NULL;
    }

    if (g_ws_client) {
        ws_client_destroy(g_ws_client);
        g_ws_client = NULL;
    }

    if (audio_encode_queue) {
        vQueueDelete(audio_encode_queue);
    }
    if (audio_playback_queue) {
        vQueueDelete(audio_playback_queue);
    }
    if (audio_send_queue) {
        vQueueDelete(audio_send_queue);
    }
    if (audio_decode_queue) {
        vQueueDelete(audio_decode_queue);
    }
    if (mp3_playback_queue) {
        vQueueDelete(mp3_playback_queue);
    }

    wake_word_deinit();
    service_initialized = false;
}

void audio_service_set_callback(void (*cb)(int16_t *samples, int count))
{
    (void)cb;
}