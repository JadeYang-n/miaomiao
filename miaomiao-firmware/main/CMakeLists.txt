idf_component_register(
    SRCS "main.c" "wifi.c" "audio.c" "audio_i2s.c" "vad.c" "wake_word.c" "audio_afe.c" "audio_service.c" "http_client.c" "display.c" "mp3_decoder.c" "ws_client.c" "opus_codec.c"
    INCLUDE_DIRS "." "display" "protocols"
    REQUIRES driver esp_http_client nvs_flash esp_netif esp_wifi json esp_lcd espressif__esp_lvgl_port espressif__esp-sr esp_timer mqtt chmorgan__esp-libhelix-mp3 espressif__esp_audio_codec tcp_transport
)