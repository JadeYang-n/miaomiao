package com.antifraud.websocket;

import com.antifraud.service.AiService;
import com.antifraud.service.TtsService;
import com.fasterxml.jackson.databind.JsonNode;
import com.fasterxml.jackson.databind.ObjectMapper;
import jakarta.annotation.Resource;
import jakarta.websocket.*;
import jakarta.websocket.server.ServerEndpoint;
import jakarta.websocket.server.ServerEndpointConfig;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.beans.factory.annotation.Qualifier;
import org.springframework.stereotype.Component;
import org.springframework.web.reactive.function.client.WebClient;

import java.io.*;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.Base64;
import java.util.Map;
import java.util.concurrent.CopyOnWriteArrayList;
import java.util.concurrent.Executors;
import java.util.concurrent.ScheduledExecutorService;
import java.util.concurrent.ScheduledFuture;
import java.util.concurrent.TimeUnit;

/**
 * ESP32 WebSocket 音频端点
 * 简化版：ESP32 发送 PCM，服务器返回 MP3（由 ESP32 的 MP3 解码器播放）
 */
@Component
@ServerEndpoint(value = "/ws/audio", configurator = AudioWebSocket.AudioWebSocketConfigurator.class)
public class AudioWebSocket {

    private static final Logger log = LoggerFactory.getLogger(AudioWebSocket.class);

    private static final String ASR_SERVICE_URL = "http://127.0.0.1:8090/asr";
    private static final int SAMPLE_RATE = 16000;
    private static final int FRAME_DURATION_MS = 60;
    private static final int FRAME_SIZE = (SAMPLE_RATE * FRAME_DURATION_MS) / 1000; // 960 samples

    // Spring beans (static because @ServerEndpoint creates instances, not Spring)
    private static AiService aiService;
    private static TtsService ttsService;
    private static ObjectMapper objectMapper;
    private static WebClient asrWebClient;

    @Resource
    public void setAiService(AiService aiService) {
        AudioWebSocket.aiService = aiService;
    }

    @Resource
    public void setTtsService(TtsService ttsService) {
        AudioWebSocket.ttsService = ttsService;
    }

    @Resource
    public void setObjectMapper(ObjectMapper objectMapper) {
        AudioWebSocket.objectMapper = objectMapper;
    }

    @Qualifier("asrWebClient")
    @Resource
    public void setAsrWebClient(WebClient asrWebClient) {
        AudioWebSocket.asrWebClient = asrWebClient;
    }

    // Session 相关
    private Session session;
    private String sessionId;
    private final CopyOnWriteArrayList<byte[]> audioBuffer = new CopyOnWriteArrayList<>();
    private volatile boolean collectingAudio = false;

    // 音频超时处理定时器
    private ScheduledExecutorService scheduler = Executors.newSingleThreadScheduledExecutor();
    private volatile ScheduledFuture<?> audioTimeoutTask = null;
    private static final long AUDIO_TIMEOUT_SECONDS = 3;

    public static class AudioWebSocketConfigurator extends ServerEndpointConfig.Configurator {
        @Override
        @SuppressWarnings("unchecked")
        public <T> T getEndpointInstance(Class<T> clazz) {
            try {
                return (T) clazz.getDeclaredConstructor().newInstance();
            } catch (Exception e) {
                throw new RuntimeException(e);
            }
        }
    }

    @OnOpen
    public void onOpen(Session session) {
        this.session = session;
        this.sessionId = "esp32-" + System.currentTimeMillis();
        this.collectingAudio = false;
        // 设置二进制消息缓冲区为 256KB（默认 8KB 太小，会截断 ~74KB 的 MP3 数据）
        session.setMaxBinaryMessageBufferSize(256 * 1024);
        log.info("[WS] ESP32 连接建立，sessionId={}, 协议={}, 最大消息大小={}",
                 sessionId, session.getNegotiatedSubprotocol(), session.getMaxTextMessageBufferSize());

        // 发送 hello 响应
        try {
            String hello = buildServerHello();
            session.getBasicRemote().sendText(hello);
            log.info("[WS] 发送 hello: {}", hello);
        } catch (IOException e) {
            log.error("[WS] 发送 hello 失败: {}", e.getMessage());
        }
    }

    @OnMessage
    public void onMessage(String message, Session session) {
        try {
            log.info("[WS] 收到文本消息，长度={}", message.length());
            JsonNode root = objectMapper.readTree(message);
            String type = root.path("type").asText();
            log.info("[WS] 消息类型: {}", type);

            if ("hello".equals(type)) {
                // ESP32 客户端 hello，发送服务端响应
                log.info("[WS] 收到客户端 hello，回复服务端 hello");
                String response = buildServerHello();
                session.getBasicRemote().sendText(response);

            } else if ("audio".equals(type)) {
                // 音频数据包
                log.info("[WS] 收到音频消息，开始处理...");
                handleAudioPacket(root);
                // 重置超时定时器
                resetAudioTimeout();

            } else if ("stop".equals(type)) {
                // 结束音频采集
                log.info("[WS] 收到 stop 消息");
                collectingAudio = false;
                cancelAudioTimeout();
                processAudioAndRespond();

            } else if ("listen".equals(type)) {
                // ESP32 告知开始说话（小智方案）
                log.info("[WS] 收到 listen 消息");
                // 如果已有音频数据，立即处理；否则等待超时
                if (!audioBuffer.isEmpty()) {
                    collectingAudio = false;
                    cancelAudioTimeout();
                    processAudioAndRespond();
                }
            } else if ("sleep".equals(type)) {
                // ESP32 请求休眠 TTS
                log.info("[WS] 收到 sleep 消息，生成休眠 TTS");
                String sleepText = "我去休息啦，有事喊我哦~";
                byte[] mp3Data = ttsService.synthesize(sleepText);
                if (mp3Data != null && mp3Data.length > 0) {
                    session.getBasicRemote().sendBinary(ByteBuffer.wrap(mp3Data));
                    log.info("[WS] 休眠 TTS 已发送，{} 字节", mp3Data.length);
                } else {
                    log.warn("[WS] 休眠 TTS 合成失败");
                }
            } else {
                log.warn("[WS] 未知消息类型: {}", type);
            }
        } catch (Exception e) {
            log.error("[WS] 处理消息失败: {}", e.getMessage(), e);
        }
    }

    @OnMessage
    public void onMessage(ByteBuffer byteBuffer, Session session) {
        try {
            log.info("[WS] 收到二进制消息，长度={}", byteBuffer.remaining());
            // 尝试作为文本解析
            byte[] bytes = new byte[byteBuffer.remaining()];
            byteBuffer.get(bytes);
            String message = new String(bytes, "UTF-8");
            log.info("[WS] 二进制消息内容: {}", message.substring(0, Math.min(100, message.length())));
            // 递归调用文本处理方法
            onMessage(message, session);
        } catch (Exception e) {
            log.error("[WS] 处理二进制消息失败: {}", e.getMessage(), e);
        }
    }

    @OnClose
    public void onClose(Session session) {
        log.info("[WS] 连接关闭，sessionId={}", sessionId);
    }

    @OnError
    public void onError(Session session, Throwable error) {
        log.error("[WS] WebSocket 错误: {}", error.getMessage());
    }

    private String buildServerHello() {
        return String.format(
            "{\"type\":\"hello\",\"session_id\":\"%s\",\"transport\":\"websocket\"," +
            "\"audio_params\":{\"format\":\"pcm\",\"sample_rate\":%d,\"channels\":1}}",
            sessionId, SAMPLE_RATE
        );
    }

    private void handleAudioPacket(JsonNode root) {
        collectingAudio = true;
        String audioBase64 = root.path("audio").asText();
        log.info("[WS] 收到音频包, audio长度={}, buffer已有{}个包", audioBase64.length(), audioBuffer.size());
        if (audioBase64.isEmpty()) return;

        try {
            // ESP32 发送的是 PCM 数据（pass-through Opus 实际是 PCM）
            byte[] pcmData = Base64.getDecoder().decode(audioBase64);
            audioBuffer.add(pcmData);
        } catch (Exception e) {
            log.error("[WS] 解码音频失败: {}", e.getMessage());
        }
    }

    private void resetAudioTimeout() {
        cancelAudioTimeout();
        audioTimeoutTask = scheduler.schedule(() -> {
            log.info("[WS] 音频超时 {} 秒，自动处理", AUDIO_TIMEOUT_SECONDS);
            if (!audioBuffer.isEmpty()) {
                collectingAudio = false;
                processAudioAndRespond();
            }
        }, AUDIO_TIMEOUT_SECONDS, TimeUnit.SECONDS);
    }

    private void cancelAudioTimeout() {
        if (audioTimeoutTask != null) {
            audioTimeoutTask.cancel(false);
            audioTimeoutTask = null;
        }
    }

    private void processAudioAndRespond() {
        if (audioBuffer.isEmpty()) {
            log.warn("[WS] 无音频数据");
            return;
        }

        log.info("[WS] 开始处理音频，共 {} 个数据包", audioBuffer.size());

        try {
            // 1. 合并 PCM 数据
            int totalSize = audioBuffer.stream().mapToInt(b -> b.length).sum();
            ByteBuffer allPcm = ByteBuffer.allocate(totalSize);
            for (byte[] chunk : audioBuffer) {
                allPcm.put(chunk);
            }
            allPcm.flip();

            byte[] pcmData = new byte[allPcm.remaining()];
            allPcm.get(pcmData);
            log.info("[WS] 合并 PCM 数据，大小: {} 字节", pcmData.length);

            // 2. PCM Base64 编码后调用 ASR
            String pcmBase64 = Base64.getEncoder().encodeToString(pcmData);
            String userText = callAsrService(pcmBase64, SAMPLE_RATE);
            log.info("[WS] ASR 识别结果: {}", userText);

            if (userText.isEmpty()) {
                userText = "[听不清，请再说一次]";
            }

            // 2.5 音量指令检测（本地处理，不走 AI，不走 TTS）
            String volumeAction = detectVolumeCommand(userText);
            if (volumeAction != null) {
                log.info("[WS] 检测到音量指令: {} → action={}", userText, volumeAction);
                // 只发音量控制消息，不发 MP3（避免 ESP32 状态机混乱）
                String volumeMsg = "{\"type\":\"volume\",\"action\":\"" + volumeAction + "\"}";
                session.getBasicRemote().sendText(volumeMsg);
                log.info("[WS] 音量消息已发送: {}", volumeAction);
                audioBuffer.clear();
                return;
            }

            // 3. AI 处理
            String textResponse = aiService.chatSync(100, userText);
            log.info("[WS] AI 回复: {}", textResponse);

            // 4. TTS 合成 MP3
            byte[] mp3Data = ttsService.synthesize(textResponse);
            if (mp3Data == null || mp3Data.length == 0) {
                log.error("[WS] TTS 合成失败");
                return;
            }
            log.info("[WS] TTS 合成成功，MP3 大小: {} 字节", mp3Data.length);

            // 5. 用 binary frame 发送 MP3 数据（避免 Base64 膨胀和 JSON 截断）
            session.getBasicRemote().sendBinary(ByteBuffer.wrap(mp3Data));
            log.info("[WS] 发送 MP3 binary 帧，大小: {} 字节", mp3Data.length);

        } catch (Exception e) {
            log.error("[WS] 处理失败: {}", e.getMessage());
            e.printStackTrace();
        }

        audioBuffer.clear();
    }

    /**
     * 检测音量控制指令
     * @return 动作名称 (up/down/max/min)，未匹配返回 null
     */
    private String detectVolumeCommand(String text) {
        if (text == null || text.isEmpty()) return null;
        if (text.contains("最大声") || text.contains("声音最大") || text.contains("音量最大")) {
            return "max";
        } else if (text.contains("最小声") || text.contains("声音最小") || text.contains("音量最小")) {
            return "min";
        } else if (text.contains("大点声") || text.contains("大声点") || text.contains("太小了")
                   || text.contains("声音大") || text.contains("大声")) {
            return "up";
        } else if (text.contains("小点声") || text.contains("小声点") || text.contains("太大了")
                   || text.contains("声音小") || text.contains("小声")) {
            return "down";
        }
        return null;
    }

    /**
     * 调用 Python ASR 服务
     */
    private String callAsrService(String pcmBase64, int sampleRate) {
        try {
            Map<String, Object> requestBody = Map.of(
                "audio", pcmBase64,
                "sample_rate", sampleRate
            );

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
            log.error("[WS] ASR 服务调用失败: {}", e.getMessage());
        }
        return "";
    }
}