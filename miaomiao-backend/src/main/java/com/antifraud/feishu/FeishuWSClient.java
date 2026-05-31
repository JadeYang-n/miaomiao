package com.antifraud.feishu;

import com.fasterxml.jackson.databind.JsonNode;
import com.fasterxml.jackson.databind.ObjectMapper;
import com.fasterxml.jackson.databind.node.ObjectNode;
import jakarta.annotation.PostConstruct;
import jakarta.annotation.PreDestroy;
import jakarta.annotation.Resource;
import okhttp3.*;
import okio.ByteString;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.beans.factory.annotation.Value;
import org.springframework.stereotype.Component;

import java.util.Map;
import java.util.Set;
import java.util.concurrent.ConcurrentHashMap;

import java.util.concurrent.TimeUnit;

/**
 * 飞书 WebSocket 长连接客户端
 * 服务器主动连接飞书，无需公网地址
 *
 * 连接流程：
 * 1. 调用 /callback/ws/endpoint 获取 WebSocket URL
 * 2. 连接 WebSocket，接收事件消息
 */
@Component
public class FeishuWSClient {

    private static final Logger log = LoggerFactory.getLogger(FeishuWSClient.class);
    private static final String FEISHU_WS_ENDPOINT = "https://open.feishu.cn/callback/ws/endpoint";

    @Value("${feishu.app-id:}")
    private String appId;

    @Value("${feishu.app-secret:}")
    private String appSecret;

    @Resource
    private com.antifraud.service.AiService aiService;

    @Value("${feishu.bot-name:苗苗}")
    private String botName;

    @Value("${feishu.user-open-id:}")
    private String fixedUserOpenId;

    private WebSocket webSocket;
    private final ObjectMapper objectMapper = new ObjectMapper();
    private final OkHttpClient client = new OkHttpClient.Builder()
            .readTimeout(0, TimeUnit.MILLISECONDS)
            .pingInterval(120, TimeUnit.SECONDS)
            .build();
    private volatile boolean connected = false;
    // 去重：最近处理过的消息 ID，10秒内不重复处理
    private final Set<String> recentMsgIds = ConcurrentHashMap.newKeySet();

    @PostConstruct
    public void init() {
        if (appId == null || appId.isEmpty() || appSecret == null || appSecret.isEmpty()) {
            log.info("[飞书WS] 未配置 app-id 或 app-secret，跳过初始化");
            return;
        }

        try {
            log.info("[飞书WS] 初始化 WebSocket 客户端，appId: {}", appId);
            connect();
        } catch (Exception e) {
            log.error("[飞书WS] 初始化失败: {}", e.getMessage());
        }
    }

    /**
     * 获取 WebSocket URL 并连接
     */
    private void connect() {
        try {
            String token = getFeishuToken();
            if (token == null) {
                log.error("[飞书WS] 无法获取 token，连接失败");
                return;
            }

            // 获取 WebSocket 端点
            OkHttpClient tempClient = new OkHttpClient();
            String json = String.format("{\"AppID\":\"%s\",\"AppSecret\":\"%s\"}", appId, appSecret);

            RequestBody body = RequestBody.create(json, MediaType.parse("application/json"));
            Request request = new Request.Builder()
                    .url(FEISHU_WS_ENDPOINT)
                    .post(body)
                    .header("Authorization", "Bearer " + token)
                    .header("Content-Type", "application/json")
                    .build();

            try (Response response = tempClient.newCall(request).execute()) {
                if (response.body() != null) {
                    String resp = response.body().string();
                    log.info("[飞书WS] 获取 WebSocket 端点响应: {}", resp);

                    JsonNode jsonNode = objectMapper.readTree(resp);
                    if (jsonNode.has("code") && jsonNode.get("code").asInt() == 0) {
                        String wsUrl = jsonNode.get("data").get("URL").asText();
                        log.info("[飞书WS] 获得 WebSocket URL，开始连接...");
                        doConnect(wsUrl, token);
                    } else {
                        log.error("[飞书WS] 获取 WebSocket URL 失败: {}", resp);
                    }
                }
            }

        } catch (Exception e) {
            log.error("[飞书WS] 连接失败: {}", e.getMessage());
        }
    }

    private void doConnect(String wsUrl, String token) {
        Request request = new Request.Builder()
                .url(wsUrl)
                .header("Authorization", "Bearer " + token)
                .build();

        webSocket = client.newWebSocket(request, new WebSocketListener() {
            @Override
            public void onMessage(WebSocket webSocket, String text) {
                log.info("[飞书WS] 收到消息: {}", text);
                handleMessage(text);
            }

            @Override
            public void onMessage(WebSocket webSocket, ByteString bytes) {
                log.info("[飞书WS] 收到二进制消息: {} bytes, hex: {}", bytes.size(), bytes.hex());
                // 飞书可能发二进制 protobuf，先记录
                handleBinaryMessage(bytes);
            }

            @Override
            public void onFailure(WebSocket webSocket, Throwable t, Response response) {
                log.error("[飞书WS] 连接失败: {}, response: {}", t.getMessage(), response);
                connected = false;
                // 5秒后重连
                new Thread(() -> {
                    try { Thread.sleep(5000); connect(); } catch (InterruptedException ignored) {}
                }).start();
            }

            @Override
            public void onClosed(WebSocket webSocket, int code, String reason) {
                log.info("[飞书WS] 连接关闭, code: {}, reason: {}", code, reason);
                connected = false;
            }

            @Override
            public void onOpen(WebSocket webSocket, Response response) {
                log.info("[飞书WS] 连接成功打开");
                connected = true;
            }
        });
    }

    private String getFeishuToken() {
        try {
            OkHttpClient tempClient = new OkHttpClient();
            String json = String.format("{\"app_id\":\"%s\",\"app_secret\":\"%s\"}", appId, appSecret);

            RequestBody body = RequestBody.create(json, MediaType.parse("application/json"));
            Request request = new Request.Builder()
                    .url("https://open.feishu.cn/open-apis/auth/v3/tenant_access_token/internal")
                    .post(body)
                    .build();

            try (Response response = tempClient.newCall(request).execute()) {
                if (response.body() != null) {
                    String resp = response.body().string();
                    JsonNode jsonNode = objectMapper.readTree(resp);
                    if (jsonNode.has("tenant_access_token")) {
                        return jsonNode.get("tenant_access_token").asText();
                    }
                }
            }
        } catch (Exception e) {
            log.error("[飞书WS] 获取 token 失败: {}", e.getMessage());
        }
        return null;
    }

    private void handleMessage(String message) {
        try {
            JsonNode json = objectMapper.readTree(message);

            // 飞书消息格式：{"schema":"2.0","header":{"event_type":"..."},"event":{...}}
            // 提取 event_type 从 header
            JsonNode header = json.has("header") ? json.get("header") : null;
            String eventType = (header != null && header.has("event_type")) ?
                header.get("event_type").asText() : "";

            JsonNode event = json.has("event") ? json.get("event") : json;

            log.info("[飞书WS] 收到消息 event_type={}, event={}", eventType, event);

            if (eventType.contains("message")) {
                handleMessageEvent(event);
            } else {
                log.debug("[飞书WS] 未知事件类型: {}", eventType);
            }
        } catch (Exception e) {
            log.error("[飞书WS] 处理消息失败: {}", e.getMessage());
        }
    }

    private void handleBinaryMessage(ByteString bytes) {
        // 飞书 WebSocket 二进制消息格式：
        // [protobuf帧]... 包含JSON payload，JSON总是以 {" 开头
        try {
            byte[] data = bytes.toByteArray();

            // 查找 { 开头后面跟着 "schema"
            int jsonStart = -1;
            for (int i = 0; i <= data.length - 10; i++) {
                if (data[i] == '{' && data[i+1] == '"') {
                    // 检查是否是 {"schema
                    if (data[i+2] == 's' && data[i+3] == 'c' && data[i+4] == 'h' &&
                        data[i+5] == 'e' && data[i+6] == 'm' && data[i+7] == 'a') {
                        jsonStart = i;
                        break;
                    }
                }
            }

            if (jsonStart < 0) {
                log.warn("[飞书WS] 无法从二进制找到 {{\"schema\"");
                return;
            }

            String json = new String(data, jsonStart, data.length - jsonStart, "UTF-8");

            // 找到完整的 JSON（匹配的括号）
            int braceCount = 0;
            int jsonEnd = -1;
            for (int i = 0; i < json.length(); i++) {
                if (json.charAt(i) == '{') braceCount++;
                else if (json.charAt(i) == '}') {
                    braceCount--;
                    if (braceCount == 0) {
                        jsonEnd = i;
                        break;
                    }
                }
            }

            if (jsonEnd >= 0) {
                json = json.substring(0, jsonEnd + 1);
            }

            log.info("[飞书WS] 从二进制提取到 JSON: {}", json);
            handleMessage(json);
        } catch (Exception e) {
            log.error("[飞书WS] 处理二进制消息失败: {}", e.getMessage());
        }
    }

    private void handleMessageEvent(JsonNode data) {
        log.info("[飞书WS] 处理事件 data={}", data);
        try {
            JsonNode message = data.has("message") ? data.get("message") : null;
            if (message == null) {
                log.debug("[飞书WS] 无 message 字段");
                return;
            }

            // 去重：检查 message_id 是否已处理
            String messageId = message.has("message_id") ? message.get("message_id").asText() : null;
            if (messageId == null || messageId.isEmpty()) {
                log.warn("[飞书WS] 消息没有 message_id，无法去重，跳过");
                return;
            }
            if (recentMsgIds.contains(messageId)) {
                log.warn("[飞书WS] 消息 {} 已处理过，跳过", messageId);
                return;
            }
            recentMsgIds.add(messageId);

            String messageType = message.has("message_type") ? message.get("message_type").asText() : "";
            String content = message.has("content") ? message.get("content").asText() : "";

            if ("text".equals(messageType)) {
                JsonNode contentObj = objectMapper.readTree(content);
                String text = contentObj.has("text") ? contentObj.get("text").asText() : content;
                log.info("[飞书WS] 用户消息: {}", text);
                processUserMessage(text, data);
            } else {
                log.debug("[飞书WS] 忽略非文本消息类型: {}", messageType);
            }

        } catch (Exception e) {
            log.error("[飞书WS] 处理消息事件失败: {}", e.getMessage());
        }
    }

    private void processUserMessage(String text, JsonNode data) {
        log.info("[飞书WS] 准备处理用户消息: {}", text);

        // 获取发送者 open_id
        JsonNode sender = data.has("sender") ? data.get("sender") : null;
        String openId = null;
        if (sender != null && sender.has("sender_id")) {
            JsonNode senderId = sender.get("sender_id");
            if (senderId.has("open_id")) {
                openId = senderId.get("open_id").asText();
            }
        }

        // 判断用户类型：配置中的 user-open-id 是子女，其他是老人
        boolean isChild = openId != null && openId.equals(fixedUserOpenId);

        // 同步处理：等 AI 处理完再返回，防止飞书重复推送
        try {
            int memoryId = isChild ? 200 : 100;
            String aiResponse;
            if (isChild) {
                aiResponse = aiService.chatSyncForChild(text);
            } else {
                aiResponse = aiService.chatSync(memoryId, text);
            }
            log.info("[飞书WS] AI 回复: {}", aiResponse);

            // 同步发送回复
            if (openId != null && aiResponse != null) {
                sendReply(openId, aiResponse);
            }
        } catch (Exception e) {
            log.error("[飞书WS] 处理消息失败: {}", e.getMessage());
        }
    }

    public void sendReply(String openId, String replyText) {
        if (webSocket == null || !connected) {
            log.warn("[飞书WS] WebSocket 未连接，无法发送消息");
            return;
        }

        try {
            String token = getFeishuToken();
            // 构造消息：content 是 JSON 对象 {"text": "内容"} 序列化后的字符串
            ObjectNode contentNode = objectMapper.createObjectNode();
            contentNode.put("text", replyText);
            String contentJson = objectMapper.writeValueAsString(contentNode);

            ObjectNode messageBody = objectMapper.createObjectNode();
            messageBody.put("receive_id", openId);
            messageBody.put("msg_type", "text");
            messageBody.put("content", contentJson);

            String json = objectMapper.writeValueAsString(messageBody);
            log.info("[飞书WS] 发送消息 JSON: {}", json);

            RequestBody body = RequestBody.create(json, MediaType.parse("application/json"));
            Request request = new Request.Builder()
                    .url("https://open.feishu.cn/open-apis/im/v1/messages?receive_id_type=open_id")
                    .post(body)
                    .header("Authorization", "Bearer " + token)
                    .header("Content-Type", "application/json; charset=utf-8")
                    .build();

            OkHttpClient tempClient = new OkHttpClient();
            try (Response response = tempClient.newCall(request).execute()) {
                if (response.body() != null) {
                    String resp = response.body().string();
                    log.info("[飞书WS] 发送回复响应: {}", resp);
                }
            }
        } catch (Exception e) {
            log.error("[飞书WS] 发送消息失败: {}", e.getMessage());
        }
    }

    @PreDestroy
    public void shutdown() {
        connected = false;
        if (webSocket != null) {
            webSocket.close(1000, "shutdown");
            log.info("[飞书WS] WebSocket 连接已关闭");
        }
    }
}