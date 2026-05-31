/**
 * WiFi 连接管理
 */

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "config.h"
#include "wifi.h"
#include "display.h"

static const char *TAG = "WIFI";

#define WIFI_MAX_RETRY      10
#define WIFI_CONNECTED_BIT  BIT0

static int s_retry_count = 0;
static EventGroupHandle_t s_wifi_event_group = NULL;

/* WiFi 事件处理 */
static void event_handler(void* arg, esp_event_base_t event_base,
                          int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_count < WIFI_MAX_RETRY) {
            esp_wifi_connect();
            s_retry_count++;
            ESP_LOGI(TAG, "重试连接 WiFi... (%d/%d)", s_retry_count, WIFI_MAX_RETRY);
        } else {
            xEventGroupSetBits(s_wifi_event_group, BIT1);  // 失败标记
            display_set_wifi(false);
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "获取 IP 地址: " IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_count = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        display_set_wifi(true);
    }
}

/* 初始化 WiFi */
esp_err_t wifi_init(void)
{
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                ESP_EVENT_ANY_ID,
                &event_handler,
                NULL,
                &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                IP_EVENT_STA_GOT_IP,
                &event_handler,
                NULL,
                &instance_got_ip));

    return ESP_OK;
}

/* 连接 WiFi（阻塞直到成功或失败） */
esp_err_t wifi_connect(const char *ssid, const char *password)
{
    static bool initialized = false;
    if (!initialized) {
        ESP_ERROR_CHECK(wifi_init());
        initialized = true;
    }

    s_retry_count = 0;

    wifi_config_t wifi_config = {0};
    strncpy((char *)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid));
    strncpy((char *)wifi_config.sta.password, password, sizeof(wifi_config.sta.password));
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    ESP_LOGI(TAG, "连接到 WiFi: %s", ssid);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    // 等待连接结果
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | BIT1,
            pdFALSE,
            pdFALSE,
            pdMS_TO_TICKS(30000));  // 30秒超时

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "WiFi 连接成功");
        return ESP_OK;
    } else {
        ESP_LOGE(TAG, "WiFi 连接失败");
        return ESP_FAIL;
    }
}

/* 获取信号强度 */
int wifi_get_rssi(void)
{
    wifi_ap_record_t ap_info;
    if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
        return ap_info.rssi;
    }
    return -127;
}

/* 获取 WiFi 状态 */
wifi_status_t wifi_get_status(void)
{
    wifi_status_t st = {.connected = false, .rssi = -127};
    if (s_wifi_event_group != NULL) {
        EventBits_t bits = xEventGroupGetBits(s_wifi_event_group);
        st.connected = (bits & WIFI_CONNECTED_BIT) != 0;
    }
    if (st.connected) {
        st.rssi = wifi_get_rssi();
    }
    return st;
}