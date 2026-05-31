package com.antifraud.service;

import com.fasterxml.jackson.databind.JsonNode;
import com.fasterxml.jackson.databind.ObjectMapper;
import com.fasterxml.jackson.datatype.jsr310.JavaTimeModule;
import org.springframework.beans.factory.annotation.Value;
import org.springframework.stereotype.Service;
import org.springframework.web.reactive.function.client.WebClient;

import java.util.Map;

/**
 * 预警通知服务 - 支持飞书智能体/钉钉 Webhook
 */
@Service
public class NotificationService {

    @Value("${notification.feishu.app-id:}")
    private String feishuAppId;

    @Value("${notification.feishu.app-secret:}")
    private String feishuAppSecret;

    @Value("${notification.feishu.user-open-id:}")
    private String feishuUserOpenId;

    @Value("${notification.dingtalk.webhook-url:}")
    private String dingtalkWebhookUrl;

    @Value("${notification.enabled:false}")
    private boolean notificationEnabled;

    private final ObjectMapper objectMapper;
    private volatile String cachedToken = null;
    private volatile long tokenExpireTime = 0;

    public NotificationService() {
        this.objectMapper = new ObjectMapper();
        this.objectMapper.registerModule(new JavaTimeModule());
    }

    /**
     * 发送预警通知
     */
    public boolean sendAlert(String title, String content, int riskLevel) {
        if (!notificationEnabled) {
            System.out.println("[通知] 通知功能未启用，跳过发送");
            return false;
        }

        boolean success = false;

        if (feishuAppId != null && !feishuAppId.isEmpty()) {
            success |= sendFeishuMessage(title, content, riskLevel);
        }

        if (dingtalkWebhookUrl != null && !dingtalkWebhookUrl.isEmpty()) {
            success |= sendDingtalkNotification(title, content, riskLevel);
        }

        return success;
    }

    /**
     * 获取飞书 tenant_access_token
     */
    private String getFeishuToken() {
        // 缓存检查，token 有效期 2 小时
        if (cachedToken != null && System.currentTimeMillis() < tokenExpireTime - 60000) {
            return cachedToken;
        }

        try {
            WebClient client = WebClient.builder().build();

            Map<String, String> requestBody = Map.of(
                "app_id", feishuAppId,
                "app_secret", feishuAppSecret
            );

            String response = client.post()
                .uri("https://open.feishu.cn/open-apis/auth/v3/tenant_access_token/internal")
                .bodyValue(requestBody)
                .retrieve()
                .bodyToMono(String.class)
                .block();

            JsonNode json = objectMapper.readTree(response);
            if (json.has("tenant_access_token")) {
                cachedToken = json.get("tenant_access_token").asText();
                tokenExpireTime = System.currentTimeMillis() + 7200000; // 2小时
                System.out.println("[飞书] 获取 token 成功");
                return cachedToken;
            }
        } catch (Exception e) {
            System.err.println("[飞书] 获取 token 失败: " + e.getMessage());
        }
        return null;
    }

    /**
     * 发送飞书消息给指定用户
     */
    private boolean sendFeishuMessage(String title, String content, int riskLevel) {
        try {
            String token = getFeishuToken();
            if (token == null) {
                System.err.println("[飞书] 未获取到 token，发送失败");
                return false;
            }

            WebClient client = WebClient.builder().build();

            String emoji = riskLevel >= 2 ? "🚨" : "⚠️";
            String riskText = riskLevel >= 2 ? "高风险" : "注意";
            String message = String.format("%s 【%s】%s\n\n老人可能遇到诈骗：\n%s\n\n请尽快联系老人确认情况！",
                emoji, riskText, title, content);

            String messageContent = String.format("{\"text\":\"%s【%s】%s\\n\\n老人可能遇到诈骗：\\n%s\\n\\n请尽快联系老人确认情况！\"}",
                riskLevel >= 2 ? "🚨" : "⚠️",
                riskLevel >= 2 ? "高风险" : "注意",
                title,
                content);

            Map<String, Object> body = Map.of(
                "receive_id", feishuUserOpenId,
                "msg_type", "text",
                "content", messageContent
            );

            String response = client.post()
                .uri("https://open.feishu.cn/open-apis/im/v1/messages?receive_id_type=open_id")
                .header("Authorization", "Bearer " + token)
                .header("Content-Type", "application/json; charset=utf-8")
                .bodyValue(body)
                .retrieve()
                .bodyToMono(String.class)
                .block();

            JsonNode json = objectMapper.readTree(response);
            if (json.has("code") && json.get("code").asInt() == 0) {
                System.out.println("[飞书] 消息发送成功");
                return true;
            } else {
                System.err.println("[飞书] 消息发送失败: " + response);
                return false;
            }
        } catch (Exception e) {
            System.err.println("[飞书] 发送消息异常: " + e.getMessage());
            return false;
        }
    }

    /**
     * 发送钉钉 Webhook 通知
     */
    private boolean sendDingtalkNotification(String title, String content, int riskLevel) {
        try {
            WebClient client = WebClient.builder().build();

            String emoji = riskLevel >= 2 ? "🚨" : "⚠️";
            String riskText = riskLevel >= 2 ? "高风险" : "注意";

            Map<String, Object> body = Map.of(
                "msgtype", "text",
                "text", Map.of(
                    "content", String.format("%s 【%s】%s\n\n老人可能遇到诈骗：\n%s\n\n请尽快联系老人确认情况！",
                        emoji, riskText, title, content)
                )
            );

            client.post()
                .uri(dingtalkWebhookUrl)
                .bodyValue(body)
                .retrieve()
                .bodyToMono(String.class)
                .block();

            return true;
        } catch (Exception e) {
            System.err.println("[钉钉] 发送通知失败: " + e.getMessage());
            return false;
        }
    }
}