package com.antifraud.gateway.model;

public class HelloMessage {
    private String type;
    private int version;
    private String transport;
    private Features features;
    private AudioParams audioParams;

    public static class Features {
        private boolean aec;
        private boolean mcp;

        public boolean isAec() { return aec; }
        public void setAec(boolean aec) { this.aec = aec; }
        public boolean isMcp() { return mcp; }
        public void setMcp(boolean mcp) { this.mcp = mcp; }
    }

    public static class AudioParams {
        private String format;
        private int sampleRate;
        private int channels;
        private int frameDuration;

        public String getFormat() { return format; }
        public void setFormat(String format) { this.format = format; }
        public int getSampleRate() { return sampleRate; }
        public void setSampleRate(int sampleRate) { this.sampleRate = sampleRate; }
        public int getChannels() { return channels; }
        public void setChannels(int channels) { this.channels = channels; }
        public int getFrameDuration() { return frameDuration; }
        public void setFrameDuration(int frameDuration) { this.frameDuration = frameDuration; }
    }

    public String getType() { return type; }
    public void setType(String type) { this.type = type; }
    public int getVersion() { return version; }
    public void setVersion(int version) { this.version = version; }
    public String getTransport() { return transport; }
    public void setTransport(String transport) { this.transport = transport; }
    public Features getFeatures() { return features; }
    public void setFeatures(Features features) { this.features = features; }
    public AudioParams getAudioParams() { return audioParams; }
    public void setAudioParams(AudioParams audioParams) { this.audioParams = audioParams; }
}