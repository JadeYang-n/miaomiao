package com.antifraud.gateway.audio;

import com.antifraud.gateway.config.AppConfig;
import com.fasterxml.jackson.databind.ObjectMapper;
import okhttp3.*;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.stereotype.Component;

import java.util.Base64;
import java.util.Map;
import java.util.concurrent.TimeUnit;

@Component
public class AudioForwarder {
    private static final Logger log = LoggerFactory.getLogger(AudioForwarder.class);

    private final AppConfig appConfig;
    private final ObjectMapper objectMapper;
    private final OkHttpClient httpClient;

    public AudioForwarder(AppConfig appConfig, ObjectMapper objectMapper) {
        this.appConfig = appConfig;
        this.objectMapper = objectMapper;
        this.httpClient = new OkHttpClient.Builder()
                .connectTimeout(10, TimeUnit.SECONDS)
                .writeTimeout(30, TimeUnit.SECONDS)
                .readTimeout(30, TimeUnit.SECONDS)
                .build();
    }

    public void forward(byte[] pcmData, String deviceId) {
        try {
            String base64Audio = Base64.getEncoder().encodeToString(pcmData);

            Map<String, Object> requestBody = Map.of(
                    "audio", base64Audio,
                    "format", "pcm",
                    "sampleRate", 16000
            );

            String json = objectMapper.writeValueAsString(requestBody);
            RequestBody body = RequestBody.create(json, MediaType.parse("application/json"));

            String url = appConfig.getSpringBoot().getUrl() + appConfig.getSpringBoot().getAudioEndpoint();
            Request request = new Request.Builder()
                    .url(url)
                    .post(body)
                    .build();

            try (Response response = httpClient.newCall(request).execute()) {
                if (response.isSuccessful()) {
                    String responseBody = response.body() != null ? response.body().string() : "";
                    log.info("Audio forwarded successfully, response: {}", responseBody);
                } else {
                    log.warn("Audio forward failed: {}", response.code());
                }
            }
        } catch (Exception e) {
            log.error("Failed to forward audio", e);
        }
    }

    public void sendTextResponse(String deviceId, String text) {
        log.info("Sending text response to device {}: {}", deviceId, text);
    }

    public void sendAudioResponse(String deviceId, byte[] mp3Audio) {
        String base64Audio = Base64.getEncoder().encodeToString(mp3Audio);
        log.info("Sending audio response to device {}, size: {} bytes", deviceId, mp3Audio.length);
    }
}