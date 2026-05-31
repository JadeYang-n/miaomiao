package com.antifraud.gateway.mqtt;

import com.antifraud.gateway.session.SessionManager;
import com.fasterxml.jackson.databind.ObjectMapper;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.stereotype.Component;

import jakarta.annotation.PostConstruct;
import jakarta.annotation.PreDestroy;
import java.io.*;
import java.net.*;
import java.nio.charset.StandardCharsets;
import java.util.*;
import java.util.concurrent.*;

/**
 * 极简 MQTT Broker 实现
 * 支持 CONNECT, PUBLISH, SUBSCRIBE, PING 基础功能
 */
public class SimpleMqttBroker {
    private static final Logger log = LoggerFactory.getLogger(SimpleMqttBroker.class);

    private static final int MQTT_PORT = 1883;
    private static final int HEADER_SIZE = 2;

    private final int port;
    private final SessionManager sessionManager;
    private final ObjectMapper objectMapper;
    private final ExecutorService acceptor;
    private volatile boolean running = false;
    private ServerSocket serverSocket;

    // 订阅 topic -> 设备列表
    private final Map<String, Set<String>> subscriptions = new ConcurrentHashMap<>();

    // clientId -> output stream
    private final Map<String, ClientConnection> clients = new ConcurrentHashMap<>();

    public SimpleMqttBroker(SessionManager sessionManager, ObjectMapper objectMapper) {
        this(1883, sessionManager, objectMapper);
    }

    public SimpleMqttBroker(int port, SessionManager sessionManager, ObjectMapper objectMapper) {
        this.port = port;
        this.sessionManager = sessionManager;
        this.objectMapper = objectMapper;
        this.acceptor = Executors.newSingleThreadExecutor();
    }

    @PostConstruct
    public void start() {
        running = true;
        acceptor.execute(this::acceptConnections);
        log.info("MQTT Broker started on port {}", port);
    }

    private void acceptConnections() {
        try {
            serverSocket = new ServerSocket(port);
            while (running) {
                try {
                    Socket clientSocket = serverSocket.accept();
                    clientSocket.setSoTimeout(30000);
                    new Thread(() -> handleClient(clientSocket)).start();
                } catch (SocketException e) {
                    if (running) log.error("Server socket error", e);
                }
            }
        } catch (IOException e) {
            log.error("Failed to start MQTT broker", e);
        }
    }

    private void handleClient(Socket socket) {
        try {
            DataInputStream in = new DataInputStream(socket.getInputStream());
            DataOutputStream out = new DataOutputStream(socket.getOutputStream());

            // 读取 CONNECT 包
            int fixedHeader = in.readByte() & 0xFF;
            int remainingLength = readRemainingLength(in);

            if (remainingLength < 10) {
                log.warn("Invalid CONNECT packet");
                return;
            }

            // 解析 CONNECT
            String protocol = readString(in);
            int protocolLevel = in.readByte() & 0xFF;
            byte connectFlags = in.readByte();
            int keepalive = (in.readByte() & 0xFF) << 8 | (in.readByte() & 0xFF);
            String clientId = readString(in);

            log.info("MQTT client connect: clientId={}", clientId);

            // 发送 CONNACK
            byte[] connack = buildConnack();
            out.write(connack);
            out.flush();

            // 保存连接
            clients.put(clientId, new ClientConnection(clientId, socket, in, out));
            sessionManager.createSession(clientId);

            // 处理消息循环
            handleMqttMessages(clientId, in, out);

        } catch (SocketTimeoutException e) {
            // 正常超时
        } catch (IOException e) {
            log.error("Client handler error", e);
        } finally {
            try {
                socket.close();
            } catch (IOException ignored) {}
        }
    }

    private void handleMqttMessages(String clientId, DataInputStream in, DataOutputStream out) throws IOException {
        while (running) {
            try {
                int firstByte = in.readByte() & 0xFF;
                if (firstByte == -1) break;

                int packetType = firstByte >> 4;
                int flags = firstByte & 0x0F;
                int remainingLength = readRemainingLength(in);

                switch (packetType) {
                    case 0x0: // CONNECT 不处理
                        break;
                    case 0x1: // CONNACK 不处理
                        break;
                    case 0x3: // PUBLISH
                        handlePublish(in, remainingLength, clientId);
                        break;
                    case 0x6: // SUBSCRIBE
                        handleSubscribe(in, remainingLength, clientId);
                        break;
                    case 0x7: // SUBACK
                        break;
                    case 0x8: // UNSUBSCRIBE
                        handleUnsubscribe(in, remainingLength, clientId);
                        break;
                    case 0xC: // PINGREQ
                        handlePing(out);
                        break;
                    case 0xE: // DISCONNECT
                        log.info("Client disconnected: {}", clientId);
                        return;
                    default:
                        log.debug("Unknown packet type: {}", packetType);
                }
            } catch (SocketTimeoutException e) {
                // 继续等待
            } catch (EOFException | SocketException e) {
                break;
            }
        }
    }

    private int readRemainingLength(DataInputStream in) throws IOException {
        int multiplier = 1;
        int value = 0;
        int digit;
        do {
            digit = in.readByte() & 0xFF;
            value += (digit & 0x7F) * multiplier;
            multiplier *= 128;
        } while ((digit & 0x80) != 0);
        return value;
    }

    private String readString(DataInputStream in) throws IOException {
        int len = in.readShort();
        byte[] buf = new byte[len];
        in.readFully(buf);
        return new String(buf, StandardCharsets.UTF_8);
    }

    private byte[] buildConnack() {
        ByteArrayOutputStream baos = new ByteArrayOutputStream();
        baos.write(0x20); // CONNACK packet type
        baos.write(0x02); // remaining length
        baos.write(0x00); // connect ack flags
        baos.write(0x00); // return code 0 = success
        return baos.toByteArray();
    }

    private void handlePublish(DataInputStream in, int remainingLength, String senderId) throws IOException {
        int topicLen = in.readShort();
        byte[] topicBytes = new byte[topicLen];
        in.readFully(topicBytes);
        String topic = new String(topicBytes, StandardCharsets.UTF_8);

        remainingLength -= topicLen + 2;

        // 跳过 packet identifier if QoS > 0
        int identifier = 0;
        int qos = (in.readByte() & 0x06) >> 1;
        if (qos > 0) {
            identifier = in.readShort();
            remainingLength -= 2;
        }

        byte[] payload = new byte[remainingLength];
        in.readFully(payload);

        log.info("PUBLISH from {} to topic: {}, payload size: {}", senderId, topic, payload.length);

        // 解析设备消息
        if (topic.matches("server/\\w+/publish")) {
            String deviceId = topic.split("/")[1];
            handleDeviceMessage(deviceId, payload);
        }

        // 转发给订阅者
        for (Map.Entry<String, Set<String>> entry : subscriptions.entrySet()) {
            String subTopic = entry.getKey();
            if (topicMatches(subTopic, topic)) {
                for (String clientId : entry.getValue()) {
                    if (!clientId.equals(senderId)) {
                        sendPublish(clientId, topic, payload);
                    }
                }
            }
        }
    }

    private boolean topicMatches(String pattern, String topic) {
        // 简单实现：支持 + 和 #
        if (pattern.equals("#")) return true;
        if (pattern.contains("+")) {
            String[] pParts = pattern.split("/");
            String[] tParts = topic.split("/");
            if (pParts.length != tParts.length) return false;
            for (int i = 0; i < pParts.length; i++) {
                if (!pParts[i].equals("+") && !pParts[i].equals(tParts[i])) {
                    return false;
                }
            }
            return true;
        }
        return pattern.equals(topic);
    }

    private void handleSubscribe(DataInputStream in, int remainingLength, String clientId) throws IOException {
        int packetId = in.readShort();
        remainingLength -= 2;

        while (remainingLength > 0) {
            int topicLen = in.readShort();
            byte[] topicBytes = new byte[topicLen];
            in.readFully(topicBytes);
            String topic = new String(topicBytes, StandardCharsets.UTF_8);
            int qos = in.readByte() & 0x03;

            subscriptions.computeIfAbsent(topic, k -> ConcurrentHashMap.newKeySet()).add(clientId);
            log.info("Subscribe: clientId={} topic={}", clientId, topic);

            remainingLength -= topicLen + 3;
        }

        // 发送 SUBACK
        ByteArrayOutputStream baos = new ByteArrayOutputStream();
        baos.write(0x90); // SUBACK
        baos.write(0x03); // remaining length
        baos.write((byte)(packetId >> 8));
        baos.write((byte)(packetId & 0xFF));
        baos.write((byte)0x00); // granted QoS 0

        ClientConnection conn = clients.get(clientId);
        if (conn != null) {
            conn.out.write(baos.toByteArray());
            conn.out.flush();
        }
    }

    private void handleUnsubscribe(DataInputStream in, int remainingLength, String clientId) throws IOException {
        int packetId = in.readShort();
        remainingLength -= 2;

        while (remainingLength > 0) {
            int topicLen = in.readShort();
            remainingLength -= 2;
            String topic = "";
            if (topicLen > 0) {
                byte[] topicBytes = new byte[topicLen];
                in.readFully(topicBytes);
                topic = new String(topicBytes, StandardCharsets.UTF_8);
                remainingLength -= topicLen;
            }
            subscriptions.remove(topic);
        }
    }

    private void handlePing(DataOutputStream out) throws IOException {
        byte[] pingResp = {(byte)0xD0, 0x00};
        out.write(pingResp);
        out.flush();
    }

    private void sendPublish(String clientId, String topic, byte[] payload) {
        ClientConnection conn = clients.get(clientId);
        if (conn == null) return;

        try {
            ByteArrayOutputStream baos = new ByteArrayOutputStream();
            baos.write(0x30); // PUBLISH
            byte[] variableHeader = new byte[topic.length() + 2];
            variableHeader[0] = (byte)(topic.length() >> 8);
            variableHeader[1] = (byte)(topic.length() & 0xFF);
            System.arraycopy(topic.getBytes(StandardCharsets.UTF_8), 0, variableHeader, 2, topic.length());

            byte[] fixedAndVariable = baos.toByteArray();
            byte[] remaining = new byte[variableHeader.length + payload.length];
            System.arraycopy(variableHeader, 0, remaining, 0, variableHeader.length);
            System.arraycopy(payload, 0, remaining, variableHeader.length, payload.length);

            // Calculate remaining length
            int payloadLen = remaining.length;
            int remainingLenBytes = 1;
            if (payloadLen > 127) remainingLenBytes++;
            if (payloadLen > 16383) remainingLenBytes++;

            byte[] full = new byte[1 + remainingLenBytes + remaining.length];
            full[0] = 0x30;
            int offset = 1;
            int len = payloadLen;
            do {
                full[offset++] = (byte)((len & 0x7F) | (offset > 1 ? 0x80 : 0));
                len >>= 7;
            } while (len > 0);
            System.arraycopy(remaining, 0, full, offset, remaining.length);

            conn.out.write(full);
            conn.out.flush();
        } catch (IOException e) {
            log.error("Failed to send publish to {}", clientId, e);
        }
    }

    private void handleDeviceMessage(String deviceId, byte[] payload) {
        try {
            String json = new String(payload, StandardCharsets.UTF_8);
            log.info("Device {} message: {}", deviceId, json);

            Map<String, Object> msg = objectMapper.readValue(json, Map.class);
            String type = (String) msg.get("type");

            if ("hello".equals(type)) {
                handleHello(deviceId, msg);
            }
        } catch (Exception e) {
            log.error("Failed to handle device message", e);
        }
    }

    @SuppressWarnings("unchecked")
    private void handleHello(String deviceId, Map<String, Object> msg) {
        // Create session
        SessionManager.Session session = sessionManager.createSession(deviceId);

        // Generate random AES key and nonce
        byte[] aesKey = new byte[16];
        byte[] baseNonce = new byte[16];
        new java.security.SecureRandom().nextBytes(aesKey);
        new java.security.SecureRandom().nextBytes(baseNonce);

        session.setAesKey(aesKey);
        session.setBaseNonce(baseNonce);
        session.setUdpServer("127.0.0.1");
        session.setUdpPort(8888);

        // Build response
        Map<String, Object> response = new LinkedHashMap<>();
        response.put("type", "hello");
        response.put("transport", "udp");
        response.put("session_id", session.getSessionId());

        Map<String, Object> audioParams = new LinkedHashMap<>();
        audioParams.put("sample_rate", 24000);
        audioParams.put("frame_duration", 60);
        response.put("audio_params", audioParams);

        Map<String, Object> udp = new LinkedHashMap<>();
        udp.put("server", session.getUdpServer());
        udp.put("port", session.getUdpPort());
        udp.put("key", bytesToHex(aesKey));
        udp.put("nonce", bytesToHex(baseNonce));
        udp.put("encryption", "aes-128-ctr");
        response.put("udp", udp);

        try {
            String json = objectMapper.writeValueAsString(response);
            String topic = "server/" + deviceId + "/subscribe";
            byte[] responseBytes = json.getBytes(StandardCharsets.UTF_8);

            ByteArrayOutputStream baos = new ByteArrayOutputStream();
            baos.write(0x30); // PUBLISH

            byte[] varHeader = new byte[topic.length() + 2 + responseBytes.length];
            varHeader[0] = (byte)(topic.length() >> 8);
            varHeader[1] = (byte)(topic.length() & 0xFF);
            System.arraycopy(topic.getBytes(StandardCharsets.UTF_8), 0, varHeader, 2, topic.length());
            System.arraycopy(responseBytes, 0, varHeader, topic.length() + 2, responseBytes.length);

            int remainingLen = varHeader.length;
            baos.write((remainingLen > 127 ? 0x80 : 0) | (remainingLen & 0x7F));
            if (remainingLen > 127) {
                baos.write((remainingLen >> 7) | 0x80);
                baos.write(remainingLen & 0x7F);
            }
            baos.write(varHeader);

            // Send to all clients subscribed to this topic
            Set<String> subscribers = subscriptions.get(topic);
            if (subscribers != null) {
                for (String subscriber : subscribers) {
                    ClientConnection conn = clients.get(subscriber);
                    if (conn != null) {
                        conn.out.write(baos.toByteArray());
                        conn.out.flush();
                    }
                }
            }

            log.info("Sent hello response to device {}", deviceId);
        } catch (Exception e) {
            log.error("Failed to send hello response", e);
        }
    }

    private String bytesToHex(byte[] bytes) {
        StringBuilder sb = new StringBuilder();
        for (byte b : bytes) {
            sb.append(String.format("%02x", b & 0xFF));
        }
        return sb.toString();
    }

    private static class ClientConnection {
        final String clientId;
        final Socket socket;
        final DataInputStream in;
        final DataOutputStream out;

        ClientConnection(String clientId, Socket socket, DataInputStream in, DataOutputStream out) {
            this.clientId = clientId;
            this.socket = socket;
            this.in = in;
            this.out = out;
        }
    }

    @PreDestroy
    public void stop() {
        running = false;
        try {
            if (serverSocket != null) {
                serverSocket.close();
            }
        } catch (IOException ignored) {}
        acceptor.shutdown();
        for (ClientConnection conn : clients.values()) {
            try {
                conn.socket.close();
            } catch (IOException ignored) {}
        }
        log.info("MQTT Broker stopped");
    }
}