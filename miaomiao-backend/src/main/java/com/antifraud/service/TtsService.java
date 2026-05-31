package com.antifraud.service;

import com.fasterxml.jackson.databind.JsonNode;
import jakarta.annotation.Resource;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.beans.factory.annotation.Qualifier;
import org.springframework.beans.factory.annotation.Value;
import org.springframework.stereotype.Service;
import org.springframework.web.reactive.function.client.WebClient;
import reactor.core.scheduler.Schedulers;

import java.util.Base64;
import java.util.List;
import java.util.Map;

/**
 * TTS 语音合成服务
 * 使用 MiMo-V2.5-TTS API 生成语音
 */
@Service
public class TtsService {

    private static final Logger log = LoggerFactory.getLogger(TtsService.class);

    // 统计 TTS 调用次数
    private static int totalCalls = 0;
    private static final Object callCountLock = new Object();

    @Qualifier("ttsWebClient")
    @Resource
    private WebClient ttsWebClient;

    // TTS API Key
    @Value("${tts.api-key}")
    private String apiKey;

    // TTS model
    @Value("${tts.model:your-tts-model}")
    private String model;

    @Qualifier("llmWebClient")
    @Resource
    private WebClient llmWebClient;

    @Resource
    private com.fasterxml.jackson.databind.ObjectMapper objectMapper;

    /**
     * 合成语音
     * @param text 要转换为语音的文本
     * @return MP3 音频数据（字节数组），失败返回 null
     */
    public byte[] synthesize(String text) {
        if (text == null || text.isEmpty()) {
            log.warn("[TTS] 收到空文本，跳过合成");
            return null;
        }

        log.info("[TTS] >>> 调用合成，文本长度: {}，内容: {}", text.length(), text.substring(0, Math.min(100, text.length())));
            synchronized (callCountLock) {
                totalCalls++;
                log.info("[TTS] 调用统计: 本次为第 {} 次调用", totalCalls);
            }

        try {
            // 清理文本
            String cleanText = text
                    .replaceAll("[#*`_\\[\\]]", "")
                    .replaceAll("\\n+", "，")
                    .trim();
            if (cleanText.length() > 500) {
                cleanText = cleanText.substring(0, 500);
            }

            if (cleanText.isEmpty()) {
                return null;
            }

            log.info("[TTS] 合成文本: {}...", cleanText.substring(0, Math.min(50, cleanText.length())));

            // 构建 MiMo TTS 请求体（OpenAI 兼容格式）
            // user message: 风格指令
            // assistant message: 要合成的文本
            Map<String, Object> requestBody = Map.of(
                    "model", model,
                    "messages", List.of(
                            Map.of("role", "user", "content", "用温柔可爱的猫咪声音朗读"),
                            Map.of("role", "assistant", "content", cleanText)
                    ),
                    "stream", false,
                    "audio", Map.of(
                            "format", "mp3",
                            "sample_rate", 16000
                    )
            );

            // 使用 TTS 专用 WebClient
            JsonNode response = ttsWebClient
                    .post()
                    .bodyValue(requestBody)
                    .retrieve()
                    .bodyToMono(JsonNode.class)
                    .block();

            // 解析响应（MiMo 格式：choices[0].message.audio.data）
            JsonNode audioNode = response.path("choices").path(0).path("message").path("audio").path("data");
            if (audioNode.isMissingNode() || audioNode.isNull()) {
                log.error("[TTS] 响应中无音频数据: {}", response);
                return null;
            }

            // Base64 解码
            String audioBase64 = audioNode.asText();
            byte[] mp3Data = Base64.getDecoder().decode(audioBase64);

            log.info("[TTS] 合成成功，音频大小: {} 字节", mp3Data.length);
            return mp3Data;

        } catch (Exception e) {
            log.error("[TTS] 合成失败: {}", e.getMessage());
            return null;
        }
    }

    /**
     * 异步合成语音（用于流式场景）
     */
    public reactor.core.publisher.Mono<byte[]> synthesizeAsync(String text) {
        return reactor.core.publisher.Mono.fromCallable(() -> synthesize(text))
                .subscribeOn(Schedulers.boundedElastic());
    }
}