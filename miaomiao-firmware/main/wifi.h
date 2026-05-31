/**
 * WiFi 连接管理头文件
 */

#ifndef WIFI_H
#define WIFI_H

#include <esp_err.h>

/**
 * 初始化 WiFi
 */
esp_err_t wifi_init(void);

/**
 * 连接 WiFi（阻塞直到成功或失败）
 * @param ssid WiFi 名称
 * @param password WiFi 密码
 * @return ESP_OK 成功，其他失败
 */
esp_err_t wifi_connect(const char *ssid, const char *password);

/**
 * 获取 WiFi 信号强度
 * @return RSSI 值（负数，越大越好）
 */
int wifi_get_rssi(void);

/**
 * WiFi 连接状态
 */
typedef struct {
    bool connected;
    int rssi;
} wifi_status_t;

/**
 * 获取 WiFi 状态
 */
wifi_status_t wifi_get_status(void);

#endif // WIFI_H