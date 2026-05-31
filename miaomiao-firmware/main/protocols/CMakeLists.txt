idf_component_register(
    SRCS "mqtt_protocol.cc"
    INCLUDE_DIRS "."
    REQUIRES esp_timer esp_netif mqtt nvs_flash mbedtls lwip
)