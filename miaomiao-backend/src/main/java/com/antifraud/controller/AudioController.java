package com.antifraud.controller;

import com.antifraud.service.AiService;
import com.antifraud.service.TtsService;
import com.fasterxml.jackson.databind.JsonNode;
import com.fasterxml.jackson.databind.ObjectMapper;
import jakarta.annotation.Resource;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.beans.factory.annotation.Qualifier;
import org.springframework.web.bind.annotation.*;
import org.springframework.web.reactive.function.client.WebClient;

import java.util.Base64;
import java.util.Map;

/**
 * ESP32 音频接口
 *
 * ESP32 发送 Base64 编码的 PCM 音频，后端：
 * 1. ASR 识别语音 -> 文字（调用 Python ASR 服务）
 * 2. AI 处理 -> 回复文字
 * 3. TTS 合成 -> MP3 音频
 * 4. 返回 文字 + 音频
 */
@RestController
@RequestMapping("/api")
public class AudioController {

    private static final Logger log = LoggerFactory.getLogger(AudioController.class);

    // Python ASR 服务地址
    private static final String ASR_SERVICE_URL = "http://127.0.0.1:8090/asr";
    private static final int OPUS_SAMPLE_RATE = 16000;
    private static final int OPUS_FRAME_DURATION_MS = 60;

    @Resource
    private AiService aiService;

    @Resource
    private TtsService ttsService;

    @Resource
    private ObjectMapper objectMapper;

    @Qualifier("asrWebClient")
    @Resource
    private WebClient asrWebClient;

    /**
     * 音频对话接口
     *
     * POST /api/audio
     * Body: {"audio":"base64编码的pcm","format":"pcm","sampleRate":16000}
     *
     * Response: {"text":"回复文字","audio":"base64编码的mp3"}
     */
    @PostMapping("/audio")
    public Map<String, String> chat(@RequestBody Map<String, String> request) {
        log.info("[Audio] >>>>> 收到音频请求，audio字段长度: {}", request.get("audio") != null ? request.get("audio").length() : 0);

        String audioBase64 = request.get("audio");
        String format = request.getOrDefault("format", "pcm");
        int sampleRate = Integer.parseInt(request.getOrDefault("sampleRate", "16000"));

        String userText = "";
        String textResponse = "喵～主人你好呀！";

        try {
            // 解码音频
            if (audioBase64 != null && !audioBase64.isEmpty()) {
                byte[] pcmData = Base64.getDecoder().decode(audioBase64);

                // 调用 Python ASR 服务
                userText = callAsrService(pcmData, sampleRate);
                log.info("[Audio] ASR 识别结果: {}", userText);

                if (userText.isEmpty()) {
                    userText = "[听不清，请再说一次]";
                }

                // 调用 AI 处理
                textResponse = aiService.chatSync(100, userText);
                log.info("[Audio] AI 回复: {}", textResponse);
            }
        } catch (Exception e) {
            log.error("[Audio] 处理失败: {}", e.getMessage());
            textResponse = "喵～主人刚才走神了，再试一次好不好？";
        }

        // TTS 合成
        String audioResponse = "";
        try {
            byte[] mp3Data = ttsService.synthesize(textResponse);
            if (mp3Data != null && mp3Data.length > 0) {
                audioResponse = Base64.getEncoder().encodeToString(mp3Data);
            }
        } catch (Exception e) {
            log.error("[Audio] TTS 失败: {}", e.getMessage());
            audioResponse = "";
        }

        return Map.of(
                "text", textResponse,
                "audio", audioResponse
        );
    }

    /**
     * 调用 Python ASR 服务
     * @param audioData 原始音频数据
     * @param sampleRate 采样率
     * @return 识别出的文字
     */
    private String callAsrService(byte[] audioData, int sampleRate) {
        try {
            // 构造请求
            Map<String, Object> requestBody = Map.of(
                    "audio", Base64.getEncoder().encodeToString(audioData),
                    "sample_rate", sampleRate
            );

            // 调用 ASR 服务
            JsonNode response = asrWebClient.post()
                    .uri(ASR_SERVICE_URL)
                    .bodyValue(requestBody)
                    .retrieve()
                    .bodyToMono(JsonNode.class)
                    .block();

            if (response != null && response.has("text")) {
                return response.get("text").asText();
            }
        } catch (Exception e) {
            log.error("[Audio] ASR 服务调用失败: {}", e.getMessage());
        }
        return "";
    }

    /**
     * 健康检查
     */
    @GetMapping("/health")
    public Map<String, Object> health() {
        return Map.of(
                "status", "ok",
                "service", "miaomiao-backend",
                "version", "1.0.0"
        );
    }
}