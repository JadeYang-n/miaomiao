#ifndef MQTT_PROTOCOL_H_
#define MQTT_PROTOCOL_H_

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

// Hello message structure
typedef struct {
    const char* type;
    int version;
    const char* transport;
    struct {
        bool aec;
        bool mcp;
    } features;
    struct {
        const char* format;
        int sample_rate;
        int channels;
        int frame_duration;
    } audio_params;
} mqtt_hello_msg_t;

// Server hello response
typedef struct {
    const char* type;
    const char* transport;
    const char* session_id;
    int server_sample_rate;
    int server_frame_duration;
    struct {
        const char* server;
        int port;
        const char* key;
        const char* nonce;
        const char* encryption;
    } udp;
} mqtt_server_hello_t;

// Audio packet callback
typedef void (*audio_packet_cb_t)(const uint8_t* data, int len, int timestamp, int sequence);
// Text message callback
typedef void (*text_message_cb_t)(const char* text);

typedef struct mqtt_protocol mqtt_protocol_t;

// Create and destroy
mqtt_protocol_t* mqtt_protocol_create(void);
void mqtt_protocol_destroy(mqtt_protocol_t* proto);

// Configuration
void mqtt_protocol_set_broker(mqtt_protocol_t* proto, const char* host, int port);
void mqtt_protocol_set_credentials(mqtt_protocol_t* proto, const char* client_id, const char* username, const char* password);
void mqtt_protocol_set_device_id(mqtt_protocol_t* proto, const char* device_id);

// Callbacks
void mqtt_protocol_on_audio(mqtt_protocol_t* proto, audio_packet_cb_t cb);
void mqtt_protocol_on_text(mqtt_protocol_t* proto, text_message_cb_t cb);

// Operations
bool mqtt_protocol_start(mqtt_protocol_t* proto);
bool mqtt_protocol_open_audio_channel(mqtt_protocol_t* proto);
void mqtt_protocol_close_audio_channel(mqtt_protocol_t* proto);
bool mqtt_protocol_is_connected(mqtt_protocol_t* proto);
bool mqtt_protocol_send_audio(mqtt_protocol_t* proto, const uint8_t* opus_data, int opus_len, int timestamp);
bool mqtt_protocol_send_text(mqtt_protocol_t* proto, const char* text);
bool mqtt_protocol_send_goodbye(mqtt_protocol_t* proto);

#ifdef __cplusplus
}
#endif

#endif // MQTT_PROTOCOL_H_