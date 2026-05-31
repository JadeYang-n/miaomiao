package com.antifraud.gateway.session;

import java.util.Map;
import java.util.UUID;
import java.util.concurrent.ConcurrentHashMap;

public class SessionManager {
    private final Map<String, Session> sessions = new ConcurrentHashMap<>();

    public static class Session {
        private final String sessionId;
        private final String deviceId;
        private byte[] aesKey;
        private byte[] baseNonce;
        private String udpServer;
        private int udpPort;
        private long lastActivity;

        public Session(String deviceId) {
            this.sessionId = UUID.randomUUID().toString();
            this.deviceId = deviceId;
            this.lastActivity = System.currentTimeMillis();
        }

        public String getSessionId() { return sessionId; }
        public String getDeviceId() { return deviceId; }
        public byte[] getAesKey() { return aesKey; }
        public void setAesKey(byte[] aesKey) { this.aesKey = aesKey; }
        public byte[] getBaseNonce() { return baseNonce; }
        public void setBaseNonce(byte[] baseNonce) { this.baseNonce = baseNonce; }
        public String getUdpServer() { return udpServer; }
        public void setUdpServer(String udpServer) { this.udpServer = udpServer; }
        public int getUdpPort() { return udpPort; }
        public void setUdpPort(int udpPort) { this.udpPort = udpPort; }
        public long getLastActivity() { return lastActivity; }
        public void updateActivity() { this.lastActivity = System.currentTimeMillis(); }
    }

    public Session createSession(String deviceId) {
        Session session = new Session(deviceId);
        sessions.put(deviceId, session);
        return session;
    }

    public Session getSession(String deviceId) {
        return sessions.get(deviceId);
    }

    public void removeSession(String deviceId) {
        sessions.remove(deviceId);
    }

    public Session getOrCreate(String deviceId) {
        return sessions.computeIfAbsent(deviceId, Session::new);
    }
}