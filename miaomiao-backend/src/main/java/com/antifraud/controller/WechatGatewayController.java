package com.antifraud.controller;

import org.springframework.web.bind.annotation.*;
import com.fasterxml.jackson.databind.ObjectMapper;

import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.atomic.AtomicReference;

/**
 * 微信插件网关控制器
 * 实现 ilink Bot API 协议，供微信插件连接并收发消息
 *
 * 通信机制：
 * - 插件作为客户端，通过长轮询连接我们的服务端
 * - 我们通过 sendmessage 接口主动向插件下发消息
 */
@RestController
@RequestMapping("/ilink/bot")
public class WechatGatewayController {

    private final ObjectMapper objectMapper = new ObjectMapper();

    // 待发送的消息队列（插件主动拉取）
    private final ConcurrentHashMap<String, Object> pendingMessages = new ConcurrentHashMap<>();
    private final AtomicReference<String> lastUpdateBuf = new AtomicReference<>("");

    /**
     * 长轮询获取新消息
     * 插件会不断调用此接口，有新消息时立即返回，否则挂起直到超时
     */
    @PostMapping("/getupdates")
    public Map<String, Object> getUpdates(@RequestBody Map<String, Object> req) {
        String getUpdatesBuf = (String) req.getOrDefault("get_updates_buf", "");

        // 打印插件请求的日志
        System.out.println("[微信网关] getupdates 请求: get_updates_buf=" + getUpdatesBuf);

        // 检查是否有待发送的消息
        Object pendingMsg = pendingMessages.remove("current");
        if (pendingMsg != null) {
            System.out.println("[微信网关] 发现待发送消息，返回给插件");
            lastUpdateBuf.set("msg_sent_" + System.currentTimeMillis());
            return Map.of(
                "ret", 0,
                "msgs", new Object[]{ pendingMsg },
                "get_updates_buf", lastUpdateBuf.get(),
                "longpolling_timeout_ms", 35000
            );
        }

        // 无新消息，返回空让插件继续轮询
        lastUpdateBuf.set("idle_" + System.currentTimeMillis());
        return Map.of(
            "ret", 0,
            "msgs", new Object[]{},
            "get_updates_buf", lastUpdateBuf.get(),
            "longpolling_timeout_ms", 35000
        );
    }

    /**
     * 接收微信插件推送的消息（用户发来的聊天数据）
     * 插件通过这个接口把微信用户的消息转发给我们
     */
    @PostMapping("/receive")
    public Map<String, Object> receiveMessage(@RequestBody Map<String, Object> req) {
        try {
            String json = objectMapper.writeValueAsString(req);
            System.out.println("[微信网关] 收到微信消息原始JSON:");
            System.out.println(json);

            // 解析并打印关键字段
            @SuppressWarnings("unchecked")
            var msgs = (Iterable<Map<String, Object>>) req.get("msgs");
            if (msgs != null) {
                for (Map<String, Object> msg : msgs) {
                    String fromUser = (String) msg.get("from_user_id");
                    String sessionId = (String) msg.get("session_id");
                    System.out.println("[微信网关] 发送者: " + fromUser + ", 会话: " + sessionId);

                    @SuppressWarnings("unchecked")
                    var itemList = (Iterable<Map<String, Object>>) msg.get("item_list");
                    if (itemList != null) {
                        for (Map<String, Object> item : itemList) {
                            int type = ((Number) item.getOrDefault("type", 0)).intValue();
                            if (type == 1) {
                                Map<String, Object> textItem = (Map<String, Object>) item.get("text_item");
                                if (textItem != null) {
                                    System.out.println("[微信网关] 文本内容: " + textItem.get("text"));
                                }
                            }
                        }
                    }
                }
            }

            return Map.of("ret", 0);
        } catch (Exception e) {
            System.err.println("[微信网关] receiveMessage 异常: " + e.getMessage());
            return Map.of("ret", 1, "errcode", -99, "errmsg", e.getMessage());
        }
    }

    /**
     * 发送消息
     * 我们的应用调用此接口，把消息发给微信插件，再由插件转发到微信用户
     */
    @PostMapping("/sendmessage")
    public Map<String, Object> sendMessage(@RequestBody Map<String, Object> req) {
        try {
            String json = objectMapper.writeValueAsString(req);
            System.out.println("[微信网关] sendmessage 请求原始JSON:");
            System.out.println(json);

            @SuppressWarnings("unchecked")
            Map<String, Object> msg = (req.get("msg") instanceof Map)
                ? (Map<String, Object>) req.get("msg")
                : objectMapper.readValue(objectMapper.writeValueAsString(req.get("msg")), Map.class);

            if (msg == null) {
                return Map.of("ret", 1, "errcode", -1, "errmsg", "msg is required");
            }

            String toUserId = (String) msg.get("to_user_id");
            @SuppressWarnings("unchecked")
            var itemList = (Iterable<Map<String, Object>>) msg.get("item_list");

            if (itemList == null) {
                return Map.of("ret", 1, "errcode", -2, "errmsg", "item_list is required");
            }

            // 构建发送到微信的文本内容
            StringBuilder textBuilder = new StringBuilder();
            for (Map<String, Object> item : itemList) {
                int type = ((Number) item.getOrDefault("type", 0)).intValue();
                if (type == 1) { // TEXT
                    Map<String, Object> textItem = (Map<String, Object>) item.get("text_item");
                    if (textItem != null) {
                        textBuilder.append(textItem.get("text"));
                    }
                }
            }

            String finalText = textBuilder.toString();
            System.out.println("[微信网关] 准备发送消息给 " + toUserId + ": " + finalText);

            // 将消息放入队列，等待插件下次轮询时取走
            Map<String, Object> queuedMsg = Map.of(
                "to_user_id", toUserId != null ? toUserId : "",
                "item_list", itemList
            );
            pendingMessages.put("current", Map.of(
                "message_id", System.currentTimeMillis(),
                "session_id", "session_" + System.currentTimeMillis(),
                "from_user_id", "bot",
                "to_user_id", toUserId,
                "create_time_ms", System.currentTimeMillis(),
                "message_type", 2,
                "message_state", 2,
                "item_list", itemList
            ));

            return Map.of("ret", 0);
        } catch (Exception e) {
            System.err.println("[微信网关] sendMessage 异常: " + e.getMessage());
            e.printStackTrace();
            return Map.of("ret", 1, "errcode", -99, "errmsg", e.getMessage());
        }
    }

    /**
     * 获取配置
     */
    @PostMapping("/getconfig")
    public Map<String, Object> getConfig(@RequestBody Map<String, Object> req) {
        System.out.println("[微信网关] getconfig 请求");
        return Map.of(
            "ret", 0,
            "typing_ticket", ""
        );
    }

    /**
     * 获取上传 URL
     */
    @PostMapping("/getuploadurl")
    public Map<String, Object> getUploadUrl(@RequestBody Map<String, Object> req) {
        System.out.println("[微信网关] getuploadurl 请求");
        return Map.of(
            "ret", 0,
            "upload_param", "",
            "thumb_upload_param", ""
        );
    }

    /**
     * 发送输入状态
     */
    @PostMapping("/sendtyping")
    public Map<String, Object> sendTyping(@RequestBody Map<String, Object> req) {
        return Map.of("ret", 0);
    }
}