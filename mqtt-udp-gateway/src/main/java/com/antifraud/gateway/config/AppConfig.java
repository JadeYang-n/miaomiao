package com.antifraud.gateway.config;

import org.springframework.boot.context.properties.ConfigurationProperties;
import org.springframework.stereotype.Component;

@Component
@org.springframework.context.annotation.Primary
@ConfigurationProperties(prefix = "")
public class AppConfig {
    private MqttConfig mqtt = new MqttConfig();
    private UdpConfig udp = new UdpConfig();
    private SpringBootConfig springBoot = new SpringBootConfig();
    private AesConfig aes = new AesConfig();

    public static class MqttConfig {
        private String broker = "tcp://localhost:1883";
        private String clientId;
        private String username = "admin";
        private String password = "public";
        private int keepAlive = 60;
        private TopicsConfig topics = new TopicsConfig();

        public static class TopicsConfig {
            private String subscribe = "server/+/publish";
            private String publish = "server/%s/subscribe";

            public String getSubscribe() { return subscribe; }
            public void setSubscribe(String subscribe) { this.subscribe = subscribe; }
            public String getPublish() { return publish; }
            public void setPublish(String publish) { this.publish = publish; }
        }

        public String getBroker() { return broker; }
        public void setBroker(String broker) { this.broker = broker; }
        public String getClientId() { return clientId; }
        public void setClientId(String clientId) { this.clientId = clientId; }
        public String getUsername() { return username; }
        public void setUsername(String username) { this.username = username; }
        public String getPassword() { return password; }
        public void setPassword(String password) { this.password = password; }
        public int getKeepAlive() { return keepAlive; }
        public void setKeepAlive(int keepAlive) { this.keepAlive = keepAlive; }
        public TopicsConfig getTopics() { return topics; }
        public void setTopics(TopicsConfig topics) { this.topics = topics; }
    }

    public static class UdpConfig {
        private int port = 8888;

        public int getPort() { return port; }
        public void setPort(int port) { this.port = port; }
    }

    public static class SpringBootConfig {
        private String url = "http://localhost:8080";
        private String audioEndpoint = "/api/audio";

        public String getUrl() { return url; }
        public void setUrl(String url) { this.url = url; }
        public String getAudioEndpoint() { return audioEndpoint; }
        public void setAudioEndpoint(String audioEndpoint) { this.audioEndpoint = audioEndpoint; }
    }

    public static class AesConfig {
        private String key = "00112233445566778899aabbccddeeff";

        public String getKey() { return key; }
        public void setKey(String key) { this.key = key; }
    }

    public MqttConfig getMqtt() { return mqtt; }
    public void setMqtt(MqttConfig mqtt) { this.mqtt = mqtt; }
    public UdpConfig getUdp() { return udp; }
    public void setUdp(UdpConfig udp) { this.udp = udp; }
    public SpringBootConfig getSpringBoot() { return springBoot; }
    public void setSpringBoot(SpringBootConfig springBoot) { this.springBoot = springBoot; }
    public AesConfig getAes() { return aes; }
    public void setAes(AesConfig aes) { this.aes = aes; }
}