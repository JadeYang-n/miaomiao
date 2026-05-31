/**
 * WebSocket 客户端 - 基于 esp_websocket_client
 *
 * 使用 ESP-IDF 官方的 esp_websocket_client 组件：
 * - 正确处理 TEXT/BINARY 帧
 * - 内置自动重连
 * - 线程安全的发送/接收
 */

#include <string.h>
#include <stdlib.h>
#include <esp_log.h>
#include <esp_websocket_client.h>

#include "ws_client.h"

static const char *TAG = "WS_CLIENT";

/* 内部状态 */
struct ws_client_s {
    esp_websocket_client_handle_t ws_client;
    char url[256];

    ws_on_message_t on_message;
    ws_on_connected_t on_connected;
    ws_on_disconnected_t on_disconnected;

    bool connected;

    /* TEXT 消息拼接缓冲（处理大消息分片） */
    char *text_accum;
    int text_accum_len;

    /* BINARY 消息拼接缓冲 */
    uint8_t *bin_accum;
    int bin_accum_len;
};

/* 事件处理 */
static void websocket_event_handler(void *arg, esp_event_base_t event_base,
                                     int32_t event_id, void *event_data)
{
    ws_client_handle_t client = (ws_client_handle_t)arg;
    esp_websocket_event_data_t *data = (esp_websocket_event_data_t *)event_data;

    switch (event_id) {
        case WEBSOCKET_EVENT_CONNECTED:
            ESP_LOGI(TAG, "WebSocket 已连接");
            client->connected = true;
            if (client->on_connected) {
                client->on_connected();
            }
            break;

        case WEBSOCKET_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "WebSocket 已断开");
            client->connected = false;
            /* 清理拼接缓冲 */
            if (client->text_accum) {
                free(client->text_accum);
                client->text_accum = NULL;
                client->text_accum_len = 0;
            }
            if (client->bin_accum) {
                free(client->bin_accum);
                client->bin_accum = NULL;
                client->bin_accum_len = 0;
            }
            if (client->on_disconnected) {
                client->on_disconnected();
            }
            break;

        case WEBSOCKET_EVENT_DATA:
            if (data->op_code == 0x01) {
                /* TEXT 帧 — 大消息可能分片，需要拼接 */
                if (data->payload_len > data->data_len) {
                    /* 分片消息：累积数据 */
                    if (client->text_accum == NULL) {
                        client->text_accum = (char *)malloc(data->payload_len + 1);
                        client->text_accum_len = 0;
                    }
                    if (client->text_accum && client->text_accum_len + data->data_len <= data->payload_len) {
                        memcpy(client->text_accum + client->text_accum_len, data->data_ptr, data->data_len);
                        client->text_accum_len += data->data_len;
                    }
                    /* 检查是否收齐 */
                    if (client->text_accum_len >= data->payload_len) {
                        client->text_accum[data->payload_len] = '\0';
                        ESP_LOGI(TAG, "TEXT 消息拼接完成: %d bytes", data->payload_len);
                        if (client->on_message) {
                            ws_message_t msg;
                            msg.type = WS_EVENT_TEXT;
                            msg.data = (uint8_t *)client->text_accum;
                            msg.len = data->payload_len;
                            msg.binary = false;
                            client->on_message(&msg);
                        }
                        free(client->text_accum);
                        client->text_accum = NULL;
                        client->text_accum_len = 0;
                    }
                } else {
                    /* 完整消息，需要确保 null 终止 */
                    ESP_LOGI(TAG, "收到 TEXT 消息，长度=%d", data->data_len);
                    if (client->on_message && data->data_len > 0) {
                        char *buf = (char *)malloc(data->data_len + 1);
                        if (buf) {
                            memcpy(buf, data->data_ptr, data->data_len);
                            buf[data->data_len] = '\0';
                            ws_message_t msg;
                            msg.type = WS_EVENT_TEXT;
                            msg.data = (uint8_t *)buf;
                            msg.len = data->data_len;
                            msg.binary = false;
                            client->on_message(&msg);
                            free(buf);
                        }
                    }
                }
            } else if (data->op_code == 0x02) {
                /* BINARY 帧 — 大消息可能分片 */
                if (data->payload_len > data->data_len) {
                    /* 分片：累积数据 */
                    if (client->bin_accum == NULL) {
                        client->bin_accum = (uint8_t *)malloc(data->payload_len);
                        client->bin_accum_len = 0;
                    }
                    if (client->bin_accum && client->bin_accum_len + data->data_len <= data->payload_len) {
                        memcpy(client->bin_accum + client->bin_accum_len, data->data_ptr, data->data_len);
                        client->bin_accum_len += data->data_len;
                    }
                    if (client->bin_accum_len >= data->payload_len) {
                        ESP_LOGI(TAG, "BINARY 消息拼接完成: %d bytes", data->payload_len);
                        if (client->on_message) {
                            ws_message_t msg;
                            msg.type = WS_EVENT_BINARY;
                            msg.data = client->bin_accum;
                            msg.len = data->payload_len;
                            msg.binary = true;
                            client->on_message(&msg);
                        }
                        free(client->bin_accum);
                        client->bin_accum = NULL;
                        client->bin_accum_len = 0;
                    }
                } else {
                    /* 完整消息 */
                    ESP_LOGI(TAG, "收到 BINARY 消息，长度=%d", data->data_len);
                    if (client->on_message && data->data_len > 0) {
                        ws_message_t msg;
                        msg.type = WS_EVENT_BINARY;
                        msg.data = (uint8_t *)data->data_ptr;
                        msg.len = data->data_len;
                        msg.binary = true;
                        client->on_message(&msg);
                    }
                }
            } else if (data->op_code == 0x08) {
                /* CLOSE 帧 */
                ESP_LOGI(TAG, "收到 CLOSE 帧");
            } else if (data->op_code == 0x0A) {
                /* PONG 帧 */
                ESP_LOGD(TAG, "收到 PONG");
            }
            break;

        case WEBSOCKET_EVENT_ERROR:
            ESP_LOGE(TAG, "WebSocket 错误");
            break;

        default:
            break;
    }
}

ws_client_handle_t ws_client_create(const char *url, const char *header_key, const char *header_value)
{
    ws_client_handle_t client = (ws_client_handle_t)calloc(1, sizeof(struct ws_client_s));
    if (client == NULL) {
        ESP_LOGE(TAG, "分配客户端失败");
        return NULL;
    }

    strncpy(client->url, url, sizeof(client->url) - 1);
    client->connected = false;

    /* 配置 WebSocket 客户端 */
    esp_websocket_client_config_t ws_cfg = {
        .uri = url,
        .buffer_size = 131072,  // 128KB：MP3 binary 可达 ~83KB
        .reconnect_timeout_ms = 5000,
        .network_timeout_ms = 10000,
    };

    client->ws_client = esp_websocket_client_init(&ws_cfg);
    if (client->ws_client == NULL) {
        ESP_LOGE(TAG, "初始化 WebSocket 客户端失败");
        free(client);
        return NULL;
    }

    /* 添加自定义请求头 */
    if (header_key && header_value) {
        esp_websocket_client_append_header(client->ws_client, header_key, header_value);
    }

    /* 注册事件处理 */
    esp_websocket_register_events(client->ws_client, WEBSOCKET_EVENT_ANY,
                                   websocket_event_handler, client);

    return client;
}

void ws_client_set_callbacks(ws_client_handle_t client,
                              ws_on_connected_t on_connected,
                              ws_on_disconnected_t on_disconnected,
                              ws_on_message_t on_message)
{
    client->on_connected = on_connected;
    client->on_disconnected = on_disconnected;
    client->on_message = on_message;
}

bool ws_client_connect(ws_client_handle_t client, uint32_t timeout_ms)
{
    ESP_LOGI(TAG, "连接到: %s", client->url);

    esp_err_t err = esp_websocket_client_start(client->ws_client);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "WebSocket 启动失败: %s", esp_err_to_name(err));
        return false;
    }

    /* 等待连接建立 */
    int wait_count = 0;
    int max_wait = timeout_ms / 100;
    while (!client->connected && wait_count < max_wait) {
        vTaskDelay(pdMS_TO_TICKS(100));
        wait_count++;
    }

    if (!client->connected) {
        ESP_LOGE(TAG, "WebSocket 连接超时");
        esp_websocket_client_stop(client->ws_client);
        return false;
    }

    return true;
}

void ws_client_disconnect(ws_client_handle_t client)
{
    if (client->ws_client) {
        esp_websocket_client_close(client->ws_client, pdMS_TO_TICKS(1000));
        esp_websocket_client_stop(client->ws_client);
    }
    client->connected = false;
}

void ws_client_destroy(ws_client_handle_t client)
{
    if (client == NULL) return;

    ws_client_disconnect(client);

    if (client->text_accum) {
        free(client->text_accum);
    }

    if (client->ws_client) {
        esp_websocket_client_destroy(client->ws_client);
        client->ws_client = NULL;
    }

    free(client);
}

bool ws_client_send_text(ws_client_handle_t client, const char *text)
{
    if (!client->connected || client->ws_client == NULL) {
        ESP_LOGW(TAG, "ws_client_send_text: NOT CONNECTED");
        return false;
    }

    int ret = esp_websocket_client_send_text(client->ws_client, text, strlen(text), pdMS_TO_TICKS(2000));
    if (ret < 0) {
        ESP_LOGW(TAG, "ws_client_send_text: SEND FAILED ret=%d", ret);
        return false;
    }

    ESP_LOGI(TAG, "ws_client_send_text: sent %d bytes OK", (int)strlen(text));
    return true;
}

bool ws_client_send_binary(ws_client_handle_t client, const uint8_t *data, size_t len)
{
    if (!client->connected || client->ws_client == NULL) {
        return false;
    }

    int ret = esp_websocket_client_send_bin(client->ws_client, (const char *)data, len, pdMS_TO_TICKS(2000));
    return ret >= 0;
}

bool ws_client_is_connected(ws_client_handle_t client)
{
    return client->connected && esp_websocket_client_is_connected(client->ws_client);
}

bool ws_client_poll(ws_client_handle_t client)
{
    return ws_client_is_connected(client);
}
