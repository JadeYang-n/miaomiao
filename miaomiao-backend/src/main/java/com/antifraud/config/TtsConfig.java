package com.antifraud.config;

import org.springframework.beans.factory.annotation.Value;
import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;
import org.springframework.http.codec.LoggingCodecSupport;
import org.springframework.web.reactive.function.client.ExchangeStrategies;
import org.springframework.web.reactive.function.client.WebClient;

/**
 * TTS 专用 WebClient 配置
 * 使用 MiMo-V2.5-TTS API
 */
@Configuration
public class TtsConfig {

    @Value("${tts.base-url}")
    private String ttsUrl;

    @Value("${tts.api-key}")
    private String apiKey;

    @Bean(name = "ttsWebClient")
    public WebClient ttsWebClient() {
        // 增加缓冲区大小到 16MB，支持大音频响应
        ExchangeStrategies strategies = ExchangeStrategies.builder()
                .codecs(configurer -> configurer.defaultCodecs().maxInMemorySize(16 * 1024 * 1024))
                .build();

        return WebClient.builder()
                .baseUrl(ttsUrl)
                .defaultHeader("Content-Type", "application/json")
                .defaultHeader("Authorization", "Bearer " + apiKey)
                .exchangeStrategies(strategies)
                .build();
    }
}