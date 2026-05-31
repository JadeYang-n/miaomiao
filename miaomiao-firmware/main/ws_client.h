/**
 * WebSocket 客户端 - 基于 esp_websocket_client
 *
 * 使用 ESP-IDF 官方的 esp_websocket_client 组件
 */

#ifndef _WS_CLIENT_H_
#define _WS_CLIENT_H_

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* WebSocket 事件类型 */
typedef enum {
    WS_EVENT_CONNECTED,
    WS_EVENT_DISCONNECTED,
    WS_EVENT_TEXT,
    WS_EVENT_BINARY,
    WS_EVENT_ERROR
} ws_event_type_t;

/* WebSocket 消息 */
typedef struct {
    ws_event_type_t type;
    uint8_t *data;
    size_t len;
    bool binary;
} ws_message_t;

/* WebSocket 回调 */
typedef void (*ws_on_message_t)(const ws_message_t *msg);
typedef void (*ws_on_connected_t)(void);
typedef void (*ws_on_disconnected_t)(void);

/* WebSocket 客户端句柄 */
typedef struct ws_client_s *ws_client_handle_t;

ws_client_handle_t ws_client_create(const char *url, const char *header_key, const char *header_value);

void ws_client_set_callbacks(ws_client_handle_t client,
                              ws_on_connected_t on_connected,
                              ws_on_disconnected_t on_disconnected,
                              ws_on_message_t on_message);

bool ws_client_connect(ws_client_handle_t client, uint32_t timeout_ms);

void ws_client_disconnect(ws_client_handle_t client);

void ws_client_destroy(ws_client_handle_t client);

bool ws_client_send_text(ws_client_handle_t client, const char *text);

bool ws_client_send_binary(ws_client_handle_t client, const uint8_t *data, size_t len);

bool ws_client_is_connected(ws_client_handle_t client);

bool ws_client_poll(ws_client_handle_t client);

#ifdef __cplusplus
}
#endif

#endif /* _WS_CLIENT_H_ */
