/**
 * HTTP 客户端 - 与后端通信
 *
 * 功能：
 * - 发送音频到后端进行 ASR
 * - 接收 AI 回复（文字 + TTS 音频）
 * - 健康检查
 */

#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <esp_http_client.h>
#include <cJSON.h>

#include "config.h"

static const char *TAG = "HTTP_CLIENT";

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
              char *text_response, uint8_t **audio_response, size_t *audio_len_out)
{
    *text_response = '\0';
    *audio_response = NULL;
    *audio_len_out = 0;

    /* 构建后端 URL */
    char url[128];
    snprintf(url, sizeof(url), "http://%s:%d%s", BACKEND_HOST, BACKEND_PORT, API_CHAT_SSE);

    ESP_LOGI(TAG, "发送请求到: %s", url);

    /* 简单实现：发送 HTTP POST 请求
     * 实际项目中应该用分块传输或 websocket
     * 这里用最简化的方式：Base64 编码音频后发送
     */

    /* 计算 Base64 编码后的长度 */
    size_t b64_len = ((audio_len + 2) / 3) * 4 + 1;
    char *b64_data = (char *)malloc(b64_len);
    if (b64_data == NULL) {
        ESP_LOGE(TAG, "分配 Base64 缓冲区失败");
        return -1;
    }

    /* 简单的 Base64 编码（不依赖外部库） */
    static const char b64_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    size_t j = 0;
    for (size_t i = 0; i < audio_len; i += 3) {
        int a = audio_data[i];
        int b = (i + 1 < audio_len) ? audio_data[i + 1] : 0;
        int c = (i + 2 < audio_len) ? audio_data[i + 2] : 0;

        b64_data[j++] = b64_chars[(a >> 2) & 0x3F];
        b64_data[j++] = b64_chars[((a << 4) | (b >> 4)) & 0x3F];
        b64_data[j++] = (i + 1 < audio_len) ? b64_chars[((b << 2) | (c >> 6)) & 0x3F] : '=';
        b64_data[j++] = (i + 2 < audio_len) ? b64_chars[c & 0x3F] : '=';
    }
    b64_data[j] = '\0';

    /* 构建 JSON 请求体 */
    char *json_body = (char *)malloc(1024 + b64_len);
    if (json_body == NULL) {
        free(b64_data);
        return -1;
    }

    snprintf(json_body, 1024 + b64_len,
             "{\"audio\":\"%s\",\"format\":\"pcm\",\"sampleRate\":16000}",
             b64_data);

    free(b64_data);

    /* 发送 HTTP 请求 */
    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_POST,
        .timeout_ms = 15000,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        ESP_LOGE(TAG, "创建 HTTP 客户端失败");
        free(json_body);
        return -1;
    }

    /* 设置请求头 */
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_header(client, "Accept", "application/json");

    /* 设置请求体 */
    esp_http_client_set_post_field(client, json_body, strlen(json_body));

    /* 执行请求 */
    esp_err_t err = esp_http_client_perform(client);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "HTTP 请求失败: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        free(json_body);
        return -1;
    }

    int status_code = esp_http_client_get_status_code(client);
    ESP_LOGI(TAG, "HTTP 响应状态码: %d", status_code);

    if (status_code != 200) {
        esp_http_client_cleanup(client);
        free(json_body);
        return -1;
    }

    /* 读取响应 - 分配 512KB 缓冲区以支持大 TTS 音频响应 */
    int content_length = esp_http_client_get_content_length(client);
    ESP_LOGI(TAG, "Content-Length: %d", content_length);

    // 如果没有 Content-Length（chunked transfer），则用 512KB 限制
    if (content_length <= 0 || content_length > 512 * 1024) {
        content_length = 512 * 1024;
    }

    char *response_buf = (char *)malloc(content_length + 1);
    if (response_buf == NULL) {
        esp_http_client_cleanup(client);
        free(json_body);
        return -1;
    }

    int read_len = esp_http_client_read(client, response_buf, content_length);
    if (read_len <= 0) {
        free(response_buf);
        esp_http_client_cleanup(client);
        free(json_body);
        return -1;
    }
    response_buf[read_len] = '\0';

    ESP_LOGI(TAG, "收到响应 %d 字节", read_len);

    /* 调试：打印响应开头 */
    ESP_LOGI(TAG, "响应开头: %.100s", response_buf);

    /* 简单解析 JSON 响应
     * 期望格式：{"text":"回复文字","audio":"base64编码的mp3"} */
    char *text_start = strstr(response_buf, "\"text\":\"");
    if (text_start) {
        text_start += 7;
        char *text_end = strstr(text_start, "\"");
        if (text_end) {
            int text_len = text_end - text_start;
            ESP_LOGI(TAG, "找到 text 字段，长度: %d", text_len);
            if (text_len > 511) text_len = 511;
            strncpy(text_response, text_start, text_len);
            text_response[text_len] = '\0';
        } else {
            ESP_LOGW(TAG, "找到 text 字段但没有结束引号");
        }
    } else {
        ESP_LOGW(TAG, "未找到 text 字段");
    }

    /* 解析 audio 字段（Base64 解码） */
    char *audio_start = strstr(response_buf, "\"audio\":\"");
    if (audio_start) {
        audio_start += 9;
        char *audio_end = strstr(audio_start, "\"");
        if (audio_end) {
            int audio_b64_len = audio_end - audio_start;
            ESP_LOGI(TAG, "找到 audio 字段，Base64 长度: %d", audio_b64_len);

            /* 解码 Base64（简化版） */
            size_t decoded_len = audio_b64_len * 3 / 4 + 1;
            uint8_t *decoded = (uint8_t *)malloc(decoded_len);
            if (decoded) {
                size_t actual_len = 0;
                for (int i = 0; i < audio_b64_len; i += 4) {
                    int a = audio_start[i];
                    int b = (i + 1 < audio_b64_len) ? audio_start[i + 1] : 'A';
                    int c = (i + 2 < audio_b64_len) ? audio_start[i + 2] : 'A';
                    int d = (i + 3 < audio_b64_len) ? audio_start[i + 3] : 'A';

                    /* Base64 解码 */
                    int values[4] = {0};
                    for (int k = 0; k < 64; k++) {
                        if (b64_chars[k] == a) values[0] = k;
                        if (b64_chars[k] == b) values[1] = k;
                        if (c != '=' && b64_chars[k] == c) values[2] = k;
                        if (d != '=' && b64_chars[k] == d) values[3] = k;
                    }

                    decoded[actual_len++] = (values[0] << 2) | (values[1] >> 4);
                    if (c != '=' && actual_len < decoded_len - 1) {
                        decoded[actual_len++] = ((values[1] & 0x0F) << 4) | (values[2] >> 2);
                    }
                    if (d != '=' && actual_len < decoded_len - 1) {
                        decoded[actual_len++] = ((values[2] & 0x03) << 6) | values[3];
                    }
                }

                *audio_response = decoded;
                *audio_len_out = actual_len;
            }
        }
    }

    /* 清理 */
    free(response_buf);
    esp_http_client_cleanup(client);
    free(json_body);

    ESP_LOGI(TAG, "解析完成: text=%s, audio_len=%d", text_response, *audio_len_out);
    return 0;
}

/**
 * 发送主动消息（超时提示）
 * @param message 要说的话
 * @param audio_response 输出：TTS 音频数据（由调用者释放）
 * @param audio_len_out 输出：TTS 音频长度
 * @return 0 成功，-1 失败
 */
int http_proactive_message(const char *message, uint8_t **audio_response, size_t *audio_len_out)
{
    *audio_response = NULL;
    *audio_len_out = 0;

    char url[128];
    snprintf(url, sizeof(url), "http://%s:%d%s", BACKEND_HOST, BACKEND_PORT, API_TTS);

    ESP_LOGI(TAG, "发送主动消息到: %s", url);

    /* 构建 JSON 请求体 */
    char *json_body = (char *)malloc(1024);
    if (json_body == NULL) {
        return -1;
    }

    snprintf(json_body, 1024, "{\"text\":\"%s\"}", message);

    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_POST,
        .timeout_ms = 10000,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        free(json_body);
        return -1;
    }

    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, json_body, strlen(json_body));

    esp_err_t err = esp_http_client_perform(client);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "HTTP 请求失败: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        free(json_body);
        return -1;
    }

    int status_code = esp_http_client_get_status_code(client);
    if (status_code != 200) {
        esp_http_client_cleanup(client);
        free(json_body);
        return -1;
    }

    char *response_buf = (char *)malloc(512 * 1024);
    if (response_buf == NULL) {
        esp_http_client_cleanup(client);
        free(json_body);
        return -1;
    }

    int read_len = esp_http_client_read(client, response_buf, 512 * 1024 - 1);
    if (read_len <= 0) {
        free(response_buf);
        esp_http_client_cleanup(client);
        free(json_body);
        return -1;
    }
    response_buf[read_len] = '\0';

    /* 解析 audio 字段（Base64 解码） */
    char *audio_start = strstr(response_buf, "\"audio\":\"");
    if (audio_start) {
        audio_start += 9;
        char *audio_end = strstr(audio_start, "\"");
        if (audio_end) {
            int audio_b64_len = audio_end - audio_start;
            size_t decoded_len = audio_b64_len * 3 / 4 + 1;
            uint8_t *decoded = (uint8_t *)malloc(decoded_len);
            if (decoded) {
                size_t actual_len = 0;
                static const char b64_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
                for (int i = 0; i < audio_b64_len; i += 4) {
                    int a = audio_start[i];
                    int b = (i + 1 < audio_b64_len) ? audio_start[i + 1] : 'A';
                    int c = (i + 2 < audio_b64_len) ? audio_start[i + 2] : 'A';
                    int d = (i + 3 < audio_b64_len) ? audio_start[i + 3] : 'A';

                    int values[4] = {0};
                    for (int k = 0; k < 64; k++) {
                        if (b64_chars[k] == a) values[0] = k;
                        if (b64_chars[k] == b) values[1] = k;
                        if (c != '=' && b64_chars[k] == c) values[2] = k;
                        if (d != '=' && b64_chars[k] == d) values[3] = k;
                    }

                    decoded[actual_len++] = (values[0] << 2) | (values[1] >> 4);
                    if (c != '=' && actual_len < decoded_len - 1) {
                        decoded[actual_len++] = ((values[1] & 0x0F) << 4) | (values[2] >> 2);
                    }
                    if (d != '=' && actual_len < decoded_len - 1) {
                        decoded[actual_len++] = ((values[2] & 0x03) << 6) | values[3];
                    }
                }

                *audio_response = decoded;
                *audio_len_out = actual_len;
            }
        }
    }

    free(response_buf);
    esp_http_client_cleanup(client);
    free(json_body);

    return 0;
}

/**
 * 健康检查
 * @return true 后端正常，false 后端异常
 */
bool http_health_check(void)
{
    char url[128];
    snprintf(url, sizeof(url), "http://%s:%d%s", BACKEND_HOST, BACKEND_PORT, API_HEALTH);

    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_GET,
        .timeout_ms = 5000,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) return false;

    esp_err_t err = esp_http_client_perform(client);
    int status = esp_http_client_get_status_code(client);

    esp_http_client_cleanup(client);

    return err == ESP_OK && status == 200;
}