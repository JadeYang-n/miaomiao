package com.antifraud.gateway.model;

public class HelloResponse {
    private String type = "hello";
    private String transport = "udp";
    private String sessionId;
    private AudioParams audioParams;
    private UdpInfo udp;

    public static class AudioParams {
        private int sampleRate = 24000;
        private int frameDuration = 60;

        public int getSampleRate() { return sampleRate; }
        public void setSampleRate(int sampleRate) { this.sampleRate = sampleRate; }
        public int getFrameDuration() { return frameDuration; }
        public void setFrameDuration(int frameDuration) { this.frameDuration = frameDuration; }
    }

    public static class UdpInfo {
        private String server;
        private int port;
        private String key;
        private String nonce;
        private String encryption = "aes-128-ctr";

        public String getServer() { return server; }
        public void setServer(String server) { this.server = server; }
        public int getPort() { return port; }
        public void setPort(int port) { this.port = port; }
        public String getKey() { return key; }
        public void setKey(String key) { this.key = key; }
        public String getNonce() { return nonce; }
        public void setNonce(String nonce) { this.nonce = nonce; }
        public String getEncryption() { return encryption; }
        public void setEncryption(String encryption) { this.encryption = encryption; }
    }

    public String getType() { return type; }
    public void setType(String type) { this.type = type; }
    public String getTransport() { return transport; }
    public void setTransport(String transport) { this.transport = transport; }
    public String getSessionId() { return sessionId; }
    public void setSessionId(String sessionId) { this.sessionId = sessionId; }
    public AudioParams getAudioParams() { return audioParams; }
    public void setAudioParams(AudioParams audioParams) { this.audioParams = audioParams; }
    public UdpInfo getUdp() { return udp; }
    public void setUdp(UdpInfo udp) { this.udp = udp; }
}