package com.antifraud.feishu;

import org.springframework.beans.factory.annotation.Value;
import org.springframework.stereotype.Service;
import org.springframework.web.reactive.function.client.WebClient;
import reactor.core.publisher.Mono;

import java.util.HashMap;
import java.util.Map;

/**
 * 微信通知服务
 * 通过微信插件的 HTTP API 发送通知到子女微信
 *
 * 插件独立运行时，通过 GATEWAY_BASE_URL 指向我们的服务端
 * 我们通过插件暴露的 HTTP 接口发送消息
 */
@Service
public class WechatNotifyService {

    // 微信插件的 HTTP 服务端口（独立运行时应设为固定值）
    @Value("${wechat.plugin.port:18789}")
    private int pluginPort;

    private final WebClient webClient;

    public WechatNotifyService() {
        // 插件端口通过配置文件设置
        this.webClient = WebClient.builder().build();
    }

    /**
     * 发送诈骗预警通知到子女微信
     */
    public boolean sendScamAlert(String scamType, String message, String suggestion) {
        String alertText = String.format(
            "🚨 【苗苗反诈预警】\n\n" +
            "⚠️ 检测到诈骗风险！\n\n" +
            "📋 诈骗类型：%s\n" +
            "💬 老人说：%s\n\n" +
            "✅ 苗苗建议：%s\n\n" +
            "请及时联系老人确认情况！",
            scamType, message, suggestion
        );

        return sendMessage(alertText);
    }

    /**
     * 发送消息到微信
     * 通过插件的 HTTP API 发送
     */
    public boolean sendMessage(String text) {
        try {
            Map<String, Object> request = new HashMap<>();
            request.put("text", text);
            request.put("to_user", "子女微信ID"); // 需要配置

            String response = webClient.post()
                    .uri("http://localhost:" + pluginPort + "/ilink/bot/sendmessage")
                    .bodyValue(request)
                    .retrieve()
                    .bodyToMono(String.class)
                    .onErrorResume(e -> {
                        System.err.println("发送微信消息失败: " + e.getMessage());
                        return Mono.just("{\"ret\":1}");
                    })
                    .block();

            System.out.println("微信发送响应: " + response);
            return response != null && response.contains("\"ret\":0");
        } catch (Exception e) {
            System.err.println("发送微信消息异常: " + e.getMessage());
            return false;
        }
    }

    /**
     * 获取配置
     */
    public String getConfig() {
        try {
            return webClient.post()
                    .uri("http://localhost:" + pluginPort + "/ilink/bot/getconfig")
                    .bodyValue("{}")
                    .retrieve()
                    .bodyToMono(String.class)
                    .block();
        } catch (Exception e) {
            return "{\"error\": \"" + e.getMessage() + "\"}";
        }
    }
}
